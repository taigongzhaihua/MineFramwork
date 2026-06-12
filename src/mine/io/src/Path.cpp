/**
 * @file Path.cpp
 * @brief mine::io::Path 实现。
 *
 * 路径字符串内部以 '/' 为规范分隔符存储。
 * 内部使用小字符串优化（SBO）+ 堆回退。
 */

#include <mine/io/Path.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <utility>

#include <mine/core/Assert.h>

namespace mine::io {

// ──────────────────────────────────────────────────────────────────────────────
// 内部存储：SBO 优先 + 堆回退
// ──────────────────────────────────────────────────────────────────────────────

struct Path::Impl {
    static constexpr size_t SboCapacity = detail::kPathSBO;  // 31 字节 + null
    char  sbo[SboCapacity + 1]{};   // SBO 缓冲区
    char* heap = nullptr;           // 堆分配（长度 > SBO 时使用）
    size_t length = 0;              // 字符串长度（不含 null）

    /// 获取实际数据指针
    [[nodiscard]] const char* c_str() const noexcept {
        return heap ? heap : sbo;
    }

    /// 获取可变数据指针
    [[nodiscard]] char* data() noexcept {
        return heap ? heap : sbo;
    }

    /// 分配或重新分配堆内存
    void alloc(size_t new_len) noexcept {
        if (new_len <= SboCapacity) {
            // 回退到 SBO（释放堆内存）
            if (heap) {
                if (length > 0) {
                    std::memcpy(sbo, heap, length);
                }
                std::free(heap);
                heap = nullptr;
            }
            sbo[new_len] = '\0';
        } else {
            // 需要堆分配
            char* new_heap = static_cast<char*>(std::realloc(heap, new_len + 1));
            MINE_ASSERT(new_heap != nullptr);
            heap = new_heap;
            heap[new_len] = '\0';
        }
        length = new_len;
    }

    /// 设置内容（拷贝）
    void set(const char* str, size_t len) noexcept {
        if (len <= SboCapacity) {
            if (heap) {
                std::free(heap);
                heap = nullptr;
            }
            if (len > 0) {
                std::memcpy(sbo, str, len);
            }
            sbo[len] = '\0';
        } else {
            alloc(len);
            std::memcpy(data(), str, len);
            data()[len] = '\0';
        }
        length = len;
    }

    /// 追加字符串
    void append(const char* str, size_t append_len) noexcept {
        size_t new_len = length + append_len;
        if (new_len <= SboCapacity) {
            std::memcpy(sbo + length, str, append_len);
            sbo[new_len] = '\0';
            length = new_len;
        } else {
            // 确保足够容量
            size_t cap = SboCapacity;
            while (cap < new_len) cap *= 2;
            char* new_heap = static_cast<char*>(std::malloc(cap + 1));
            MINE_ASSERT(new_heap != nullptr);
            if (length > 0) {
                std::memcpy(new_heap, c_str(), length);
            }
            std::memcpy(new_heap + length, str, append_len);
            new_heap[new_len] = '\0';

            if (heap) std::free(heap);
            heap = new_heap;
            length = new_len;
        }
    }

    /// 清理
    void clear() noexcept {
        if (heap) {
            std::free(heap);
            heap = nullptr;
        }
        sbo[0] = '\0';
        length = 0;
    }

    /// 拷贝构造
    void copy_from(const Impl& other) noexcept {
        set(other.c_str(), other.length);
    }

    /// 移动构造
    void move_from(Impl& other) noexcept {
        if (other.heap) {
            heap = other.heap;
            other.heap = nullptr;
            length = other.length;
        } else {
            set(other.sbo, other.length);
        }
        other.clear();
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// 路径规范化辅助函数
// ──────────────────────────────────────────────────────────────────────────────

namespace {

/// 将字符串中的 '\\' 替换为 '/'
void normalize_separators(char* str, size_t len) noexcept {
    for (size_t i = 0; i < len; ++i) {
        if (str[i] == '\\') {
            str[i] = '/';
        }
    }
}

/// 移除冗余的 '/'（如 "a//b" → "a/b"，"/a/./b/" → "/a/b"）
size_t collapse_separators(char* str, size_t len) noexcept {
    if (len == 0) return 0;

    size_t write = 0;
    bool prev_slash = false;

    for (size_t read = 0; read < len; ++read) {
        if (str[read] == '/') {
            if (prev_slash) {
                continue;  // 跳过重复的 '/'
            }
            prev_slash = true;
        } else {
            prev_slash = false;
        }
        str[write++] = str[read];
    }

    // 移除末尾 '/'（保留根路径 "/"）
    if (write > 1 && str[write - 1] == '/') {
        --write;
    }
    str[write] = '\0';
    return write;
}

/// 解析 "." 和 ".." 路径组件
size_t resolve_dots(char* str, size_t len) noexcept {
    // 对于复杂路径，简化处理：保留原始输入
    // 完整实现需要处理如 "/a/./b/../c" → "/a/c"
    // 这里仅处理绝对路径的规范化
    for (size_t i = 0; i < len; ++i) {
        // 跳过 "." 独立组件
        if (str[i] == '/' && i + 2 <= len && str[i + 1] == '.' &&
            (i + 2 == len || str[i + 2] == '/')) {
            // 将 "/./" 压缩为 "/"
            size_t rest = len - (i + 2);
            if (rest > 0) {
                std::memmove(str + i, str + i + 2, rest);
            }
            len -= 2;
            str[len] = '\0';
            --i;  // 重新检查当前位置
        }
    }
    return len;
}

/// 完整规范化路径
size_t normalize_path(char* str, size_t len) noexcept {
    normalize_separators(str, len);
    len = collapse_separators(str, len);
    len = resolve_dots(str, len);
    return len;
}

/// 查找最后一次出现的分隔符 '/'
const char* find_last_separator(const char* str, size_t len) noexcept {
    const char* last = nullptr;
    for (size_t i = 0; i < len; ++i) {
        if (str[i] == '/') {
            last = str + i;
        }
    }
    return last;
}

/// 查找最后一次出现的 '.'
const char* find_last_dot(const char* str, size_t len) noexcept {
    const char* last_sep = find_last_separator(str, len);
    const char* start = last_sep ? last_sep + 1 : str;

    const char* last_dot = nullptr;
    for (const char* p = start; p < str + len; ++p) {
        if (*p == '.') {
            last_dot = p;
        }
    }
    return last_dot;
}

} // namespace

// ──────────────────────────────────────────────────────────────────────────────
// Path 构造
// ──────────────────────────────────────────────────────────────────────────────

Path::Path() noexcept
    : impl_{new Impl{}}
{
    impl_->sbo[0] = '.';
    impl_->sbo[1] = '\0';
    impl_->length = 1;
}

Path::Path(const char* str) noexcept
    : impl_{new Impl{}}
{
    size_t len = std::strlen(str);
    impl_->set(str, len);
    normalize_separators(impl_->data(), len);
    size_t new_len = normalize_path(impl_->data(), impl_->length);
    impl_->length = new_len;
    if (new_len <= Impl::SboCapacity) {
        impl_->sbo[new_len] = '\0';
    }
}

Path::Path(mine::core::StringView sv) noexcept
    : impl_{new Impl{}}
{
    impl_->set(sv.data(), sv.size());
    normalize_separators(impl_->data(), sv.size());
    size_t new_len = normalize_path(impl_->data(), impl_->length);
    impl_->length = new_len;
    if (new_len <= Impl::SboCapacity) {
        impl_->sbo[new_len] = '\0';
    }
}

Path::Path(const char* str, size_t len) noexcept
    : impl_{new Impl{}}
{
    impl_->set(str, len);
    normalize_separators(impl_->data(), len);
    size_t new_len = normalize_path(impl_->data(), impl_->length);
    impl_->length = new_len;
    if (new_len <= Impl::SboCapacity) {
        impl_->sbo[new_len] = '\0';
    }
}

Path::Path(const Path& other) noexcept
    : impl_{new Impl{}}
{
    impl_->copy_from(*other.impl_);
}

Path::Path(Path&& other) noexcept
    : impl_{new Impl{}}
{
    impl_->move_from(*other.impl_);
}

Path& Path::operator=(const Path& other) noexcept {
    if (this != &other) {
        impl_->copy_from(*other.impl_);
    }
    return *this;
}

Path& Path::operator=(Path&& other) noexcept {
    if (this != &other) {
        impl_->clear();
        impl_->move_from(*other.impl_);
    }
    return *this;
}

Path::~Path() noexcept {
    delete impl_;
}

// ──────────────────────────────────────────────────────────────────────────────
// 路径组件查询
// ──────────────────────────────────────────────────────────────────────────────

mine::core::StringView Path::string() const noexcept {
    return {impl_->c_str(), impl_->length};
}

const char* Path::c_str() const noexcept {
    return impl_->c_str();
}

bool Path::empty() const noexcept {
    return impl_->length == 0;
}

bool Path::is_absolute() const noexcept {
    if (impl_->length == 0) return false;
    // 以 '/' 开头
    if (impl_->c_str()[0] == '/') return true;
    // Windows 盘符：如 "C:/..."
    if (impl_->length >= 3 &&
        (impl_->c_str()[0] >= 'A' && impl_->c_str()[0] <= 'Z') &&
        impl_->c_str()[1] == ':' &&
        impl_->c_str()[2] == '/') {
        return true;
    }
    // 小写盘符
    if (impl_->length >= 3 &&
        (impl_->c_str()[0] >= 'a' && impl_->c_str()[0] <= 'z') &&
        impl_->c_str()[1] == ':' &&
        impl_->c_str()[2] == '/') {
        return true;
    }
    return false;
}

bool Path::is_relative() const noexcept {
    return !is_absolute();
}

Path Path::parent_path() const noexcept {
    const char* last_sep = find_last_separator(impl_->c_str(), impl_->length);
    if (!last_sep) {
        // 无分隔符，返回当前目录
        return Path{};
    }

    size_t parent_len = static_cast<size_t>(last_sep - impl_->c_str());
    if (parent_len == 0) {
        // 根路径
        return Path{"/"};
    }

    // 去除末尾斜杠（若存在连续斜杠已在上层规范化）
    return Path{impl_->c_str(), parent_len};
}

mine::core::StringView Path::filename() const noexcept {
    if (impl_->length == 0) return {};

    const char* last_sep = find_last_separator(impl_->c_str(), impl_->length);
    const char* start = last_sep ? last_sep + 1 : impl_->c_str();
    size_t remain = impl_->length - static_cast<size_t>(start - impl_->c_str());
    return {start, remain};
}

mine::core::StringView Path::extension() const noexcept {
    auto fname = filename();
    if (fname.empty()) return {};

    const char* last_dot = find_last_dot(fname.data(), fname.size());
    if (!last_dot) return {};
    // 检查是否为特殊文件（如 ".gitignore"）
    if (last_dot == fname.data()) return {};

    size_t ext_len = fname.size() - static_cast<size_t>(last_dot - fname.data());
    return {last_dot, ext_len};
}

mine::core::StringView Path::stem() const noexcept {
    auto fname = filename();
    if (fname.empty()) return {};

    const char* last_dot = find_last_dot(fname.data(), fname.size());
    if (!last_dot || last_dot == fname.data()) {
        return fname;
    }

    size_t stem_len = static_cast<size_t>(last_dot - fname.data());
    return {fname.data(), stem_len};
}

// ──────────────────────────────────────────────────────────────────────────────
// 路径修改
// ──────────────────────────────────────────────────────────────────────────────

void Path::replace_extension(mine::core::StringView new_ext) noexcept {
    auto fname = filename();
    if (fname.empty()) return;

    // 计算新路径
    size_t parent_len = impl_->length - fname.size();
    auto stem_val = stem();
    size_t stem_len = stem_val.size();

    size_t new_len = parent_len + stem_len + new_ext.size();
    impl_->alloc(new_len);
    char* buf = impl_->data();

    // 父路径前缀已存在，只需修改文件名部分
    auto old_fname_start = buf + parent_len;
    std::memmove(old_fname_start + stem_len + new_ext.size(),
                 old_fname_start + fname.size(),
                 impl_->length - parent_len - fname.size());
    // 实际上没必要移动剩余部分，这里直接截断后追加
    // 重新设置文件名区域
    if (stem_len > 0) {
        // stem 从 filename 中已就位
    }
    std::memcpy(buf + parent_len + stem_len, new_ext.data(), new_ext.size());
    impl_->length = parent_len + stem_len + new_ext.size();
    buf[impl_->length] = '\0';
}

void Path::remove_extension() noexcept {
    auto ext = extension();
    if (ext.empty()) return;

    impl_->length -= ext.size();
    impl_->data()[impl_->length] = '\0';
}

void Path::replace_filename(mine::core::StringView new_name) noexcept {
    auto fname = filename();
    if (fname.empty()) {
        // 空路径 → 直接设置
        impl_->set(new_name.data(), new_name.size());
        return;
    }

    size_t parent_len = impl_->length - fname.size();
    size_t new_len = parent_len + new_name.size();
    if (parent_len == 0) {
        impl_->set(new_name.data(), new_name.size());
    } else {
        impl_->alloc(new_len);
        impl_->data()[parent_len] = '\0';
        std::memcpy(impl_->data() + parent_len, new_name.data(), new_name.size());
        impl_->length = new_len;
        impl_->data()[new_len] = '\0';
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// 路径拼接
// ──────────────────────────────────────────────────────────────────────────────

Path Path::join(mine::core::StringView sub) const noexcept {
    if (sub.empty()) return *this;
    if (impl_->length == 0 || (impl_->length == 1 && impl_->c_str()[0] == '.')) {
        return Path{sub};
    }

    Path result;
    bool need_sep = (impl_->c_str()[impl_->length - 1] != '/');

    result.impl_->set(impl_->c_str(), impl_->length);
    if (need_sep) {
        result.impl_->append("/", 1);
    }
    result.impl_->append(sub.data(), sub.size());

    // 规范化
    size_t new_len = normalize_path(result.impl_->data(), result.impl_->length);
    result.impl_->length = new_len;
    if (new_len <= Impl::SboCapacity) {
        result.impl_->sbo[new_len] = '\0';
    } else {
        result.impl_->heap[new_len] = '\0';
    }

    return result;
}

// ──────────────────────────────────────────────────────────────────────────────
// 工具
// ──────────────────────────────────────────────────────────────────────────────

bool Path::ends_with(mine::core::StringView suffix) const noexcept {
    if (suffix.size() > impl_->length) return false;
    return std::strncmp(impl_->c_str() + impl_->length - suffix.size(),
                        suffix.data(), suffix.size()) == 0;
}

bool Path::starts_with(mine::core::StringView prefix) const noexcept {
    if (prefix.size() > impl_->length) return false;
    return std::strncmp(impl_->c_str(), prefix.data(), prefix.size()) == 0;
}

Path Path::relative_to(const Path& base) const noexcept {
    // 简化实现：如果当前路径以 base 开头，则返回减去 base 前缀的部分
    auto base_str = base.string();
    auto self_str = string();

    if (self_str.empty() || base_str.empty()) return *this;

    if (starts_with(base_str)) {
        size_t base_len = base_str.size();
        if (base_len >= self_str.size()) {
            return Path{"."};
        }
        // 跳过 base 后的 '/'
        size_t offset = base_len;
        if (offset < self_str.size() && impl_->c_str()[offset] == '/') {
            ++offset;
        }
        return Path{impl_->c_str() + offset, self_str.size() - offset};
    }

    return *this;
}

Path Path::normalize(mine::core::StringView raw) noexcept {
    return Path{raw};
}

} // namespace mine::io
