/**
 * @file PipeStream.cpp
 * @brief mine::io::PipeStream 实现。
 *
 * 缓冲式顺序流，包装 File 提供更高层的行读取/写入操作。
 */

#include <mine/io/PipeStream.h>
#include <mine/io/File.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <utility>

#include <mine/core/Assert.h>

namespace mine::io {

// ──────────────────────────────────────────────────────────────────────────────
// PipeStream::Impl
// ──────────────────────────────────────────────────────────────────────────────

struct PipeStream::Impl {
    File  file;                        ///< 底层 File 对象
    bool  is_open_flag = false;        ///< 是否已打开
    bool  error_flag   = false;        ///< 是否发生错误
    bool  eof_flag     = false;        ///< 是否已到流末尾
    mine::core::Status last_error_;    ///< 最近一次错误状态

    // 写缓冲区
    char* write_buf = nullptr;
    size_t write_buf_size = 0;
    size_t write_buf_used = 0;

    // 读缓冲区
    char* read_buf = nullptr;
    size_t read_buf_size = 0;
    size_t read_buf_pos  = 0;
    size_t read_buf_end  = 0;

    StreamAccess access = StreamAccess::Read;

    void init_buffers(size_t size) noexcept {
        write_buf_size = size;
        read_buf_size  = size;
        write_buf = static_cast<char*>(std::malloc(size));
        read_buf  = static_cast<char*>(std::malloc(size));
        MINE_ASSERT(write_buf != nullptr && read_buf != nullptr);
    }

    void cleanup() noexcept {
        std::free(write_buf);
        std::free(read_buf);
        write_buf = nullptr;
        read_buf  = nullptr;
    }

    mine::core::Status flush_write() noexcept {
        if (write_buf_used == 0) return mine::core::Status::success();

        auto result = file.write_all({write_buf, write_buf_used});
        write_buf_used = 0;
        return result;
    }

    mine::core::Status fill_read() noexcept {
        auto result = file.read({read_buf, read_buf_size});
        if (!result.ok()) {
            error_flag = true;
            last_error_ = result.error();
            return result.error();
        }
        read_buf_pos = 0;
        read_buf_end = result.value();
        if (read_buf_end == 0) {
            eof_flag = true;
        }
        return mine::core::Status::success();
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// 构造
// ──────────────────────────────────────────────────────────────────────────────

PipeStream::PipeStream() noexcept : impl_{new Impl{}} {}

PipeStream::PipeStream(PipeStream&& other) noexcept
    : impl_{other.impl_} {
    other.impl_ = new Impl{};
}

PipeStream& PipeStream::operator=(PipeStream&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = new Impl{};
    }
    return *this;
}

PipeStream::~PipeStream() noexcept {
    if (impl_->is_open_flag) {
        (void)impl_->flush_write();  // 析构中忽略刷新失败
        (void)impl_->file.close();   // 析构中忽略关闭失败
    }
    impl_->cleanup();
    delete impl_;
}

// ──────────────────────────────────────────────────────────────────────────────
// 内部构造函数（须在工厂方法之前定义）
// ──────────────────────────────────────────────────────────────────────────────

PipeStream::PipeStream(Impl* impl) noexcept : impl_{impl} {}

// ──────────────────────────────────────────────────────────────────────────────
// 工厂方法
// ──────────────────────────────────────────────────────────────────────────────

mine::core::Result<PipeStream>
PipeStream::from_file_read(const Path& path, size_t buffer_size) noexcept {
    auto file_result = File::open(path, FileMode::DefaultRead);
    if (!file_result.ok()) {
        return mine::core::Result<PipeStream>{mine::core::err_tag, file_result.error()};
    }

    auto* impl = new Impl{};
    impl->file = std::move(file_result.value());
    impl->is_open_flag = true;
    impl->access = StreamAccess::Read;
    impl->init_buffers(buffer_size > 0 ? buffer_size : kDefaultBufferSize);

    return mine::core::Result<PipeStream>{mine::core::ok_tag, PipeStream{impl}};
}

mine::core::Result<PipeStream>
PipeStream::from_file_write(const Path& path, size_t buffer_size) noexcept {
    auto file_result = File::open(path, FileMode::DefaultWrite);
    if (!file_result.ok()) {
        return mine::core::Result<PipeStream>{mine::core::err_tag, file_result.error()};
    }

    auto* impl = new Impl{};
    impl->file = std::move(file_result.value());
    impl->is_open_flag = true;
    impl->access = StreamAccess::Write;
    impl->init_buffers(buffer_size > 0 ? buffer_size : kDefaultBufferSize);

    return mine::core::Result<PipeStream>{mine::core::ok_tag, PipeStream{impl}};
}

mine::core::Result<PipeStream>
PipeStream::from_file_append(const Path& path, size_t buffer_size) noexcept {
    auto file_result = File::open(path,
        FileMode::Write | FileMode::Create | FileMode::Append);
    if (!file_result.ok()) {
        return mine::core::Result<PipeStream>{mine::core::err_tag, file_result.error()};
    }

    auto* impl = new Impl{};
    impl->file = std::move(file_result.value());
    impl->is_open_flag = true;
    impl->access = StreamAccess::Write;
    impl->init_buffers(buffer_size > 0 ? buffer_size : kDefaultBufferSize);

    return mine::core::Result<PipeStream>{mine::core::ok_tag, PipeStream{impl}};
}

// ──────────────────────────────────────────────────────────────────────────────
// 读取
// ──────────────────────────────────────────────────────────────────────────────

mine::core::Result<size_t>
PipeStream::read_some(mine::core::Span<char> buf) noexcept {
    if (!impl_->is_open_flag) {
        return mine::core::Result<size_t>{mine::core::err_tag,
            mine::core::Status{mine::core::StatusCode::InvalidState}};
    }

    // 先从内部缓冲区读取
    size_t avail = impl_->read_buf_end - impl_->read_buf_pos;
    if (avail > 0) {
        size_t to_copy = std::min(avail, buf.size());
        std::memcpy(buf.data(), impl_->read_buf + impl_->read_buf_pos, to_copy);
        impl_->read_buf_pos += to_copy;
        return mine::core::Result<size_t>{mine::core::ok_tag, to_copy};
    }

    if (impl_->eof_flag) {
        return mine::core::Result<size_t>{mine::core::ok_tag, size_t{0}};
    }

    // 内部缓冲区为空，填充
    auto fill_status = impl_->fill_read();
    if (!fill_status.ok()) {
        return mine::core::Result<size_t>{mine::core::err_tag, fill_status};
    }

    avail = impl_->read_buf_end - impl_->read_buf_pos;
    size_t to_copy = std::min(avail, buf.size());
    if (to_copy > 0) {
        std::memcpy(buf.data(), impl_->read_buf + impl_->read_buf_pos, to_copy);
        impl_->read_buf_pos += to_copy;
    }

    return mine::core::Result<size_t>{mine::core::ok_tag, to_copy};
}

mine::core::Result<size_t>
PipeStream::read_exact(mine::core::Span<char> buf) noexcept {
    size_t total = 0;
    while (total < buf.size()) {
        auto result = read_some(buf.subspan(total));
        if (!result.ok()) return result;
        size_t n = result.value();
        if (n == 0) break;  // EOF
        total += n;
    }
    return mine::core::Result<size_t>{mine::core::ok_tag, total};
}

mine::core::Result<size_t>
PipeStream::read_line(mine::core::Span<char> buf, char delimiter) noexcept {
    if (!impl_->is_open_flag || buf.empty()) {
        return mine::core::Result<size_t>{mine::core::err_tag,
            mine::core::Status{mine::core::StatusCode::InvalidState}};
    }

    size_t line_len = 0;
    bool found_delimiter = false;

    while (line_len < buf.size() - 1) {  // 保留 null 终止符空间
        // 检查内部缓冲区
        size_t avail = impl_->read_buf_end - impl_->read_buf_pos;
        if (avail == 0) {
            if (impl_->eof_flag) break;

            auto fill_status = impl_->fill_read();
            if (!fill_status.ok()) {
                return mine::core::Result<size_t>{mine::core::err_tag, fill_status};
            }
            avail = impl_->read_buf_end - impl_->read_buf_pos;
            if (avail == 0) break;  // EOF after fill
        }

        // 拷贝一个字符
        char ch = impl_->read_buf[impl_->read_buf_pos++];
        if (ch == delimiter) {
            found_delimiter = true;
            break;
        }
        buf.data()[line_len++] = ch;
    }

    buf.data()[line_len] = '\0';
    return mine::core::Result<size_t>{mine::core::ok_tag, line_len};
}

// ──────────────────────────────────────────────────────────────────────────────
// 写入
// ──────────────────────────────────────────────────────────────────────────────

mine::core::Result<size_t>
PipeStream::write_some(mine::core::Span<const char> buf) noexcept {
    if (!impl_->is_open_flag) {
        return mine::core::Result<size_t>{mine::core::err_tag,
            mine::core::Status{mine::core::StatusCode::InvalidState}};
    }

    size_t written = 0;

    // 如果写缓冲区有空间，先填充缓冲区
    size_t free_space = impl_->write_buf_size - impl_->write_buf_used;
    if (free_space > 0) {
        size_t to_buffer = std::min(free_space, buf.size());
        std::memcpy(impl_->write_buf + impl_->write_buf_used, buf.data(), to_buffer);
        impl_->write_buf_used += to_buffer;
        written = to_buffer;
    }

    // 如果缓冲区满了，刷新
    if (impl_->write_buf_used >= impl_->write_buf_size) {
        auto flush_status = impl_->flush_write();
        if (!flush_status.ok()) {
            return mine::core::Result<size_t>{mine::core::err_tag, flush_status};
        }

        // 剩余数据直接写入
        if (written < buf.size()) {
            auto remaining = buf.subspan(written);
            auto result = impl_->file.write(remaining);
            if (!result.ok()) {
                return mine::core::Result<size_t>{mine::core::err_tag, result.error()};
            }
            written += result.value();
        }
    }

    return mine::core::Result<size_t>{mine::core::ok_tag, written};
}

mine::core::Status PipeStream::write_all(mine::core::Span<const char> buf) noexcept {
    size_t total = 0;
    while (total < buf.size()) {
        auto result = write_some(buf.subspan(total));
        if (!result.ok()) return result.error();
        total += result.value();
    }
    return mine::core::Status::success();
}

mine::core::Status PipeStream::write_line(mine::core::Span<const char> line) noexcept {
    auto status = write_all(line);
    if (!status.ok()) return status;

    const char newline = '\n';
    return write_all({&newline, 1});
}

// ──────────────────────────────────────────────────────────────────────────────
// 刷新与状态
// ──────────────────────────────────────────────────────────────────────────────

mine::core::Status PipeStream::flush() noexcept {
    auto status = impl_->flush_write();
    if (!status.ok()) return status;
    return impl_->file.flush();
}

bool PipeStream::is_open() const noexcept {
    return impl_->is_open_flag;
}

bool PipeStream::eof() const noexcept {
    if (!impl_->is_open_flag) return true;
    // 还有缓冲区数据可读时不算 EOF
    if (impl_->read_buf_end > impl_->read_buf_pos) return false;
    return impl_->eof_flag;
}

bool PipeStream::has_error() const noexcept {
    return impl_->error_flag;
}

mine::core::Status PipeStream::last_error() const noexcept {
    return impl_->last_error_;
}

mine::core::Status PipeStream::close() noexcept {
    if (!impl_->is_open_flag) {
        return mine::core::Status::success();
    }

    mine::core::Status flush_status = impl_->flush_write();
    impl_->cleanup();

    auto status = impl_->file.close();
    impl_->is_open_flag = false;
    impl_->eof_flag = true;
    // 优先返回 close 的错误，其次 flush 的错误
    if (!status.ok()) return status;
    return flush_status;
}

} // namespace mine::io
