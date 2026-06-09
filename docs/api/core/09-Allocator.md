# Allocator 详细接口文档

## 概述

`Allocator` 模块提供了 MineFramework 的统一内存分配接口,包括 `IAllocator` 抽象接口和全局默认分配器访问函数。所有堆内存分配必须通过此接口进行,而不是直接调用 `malloc/free`。

### 核心特性

- **统一接口**：`IAllocator` 抽象类定义 `alloc()` / `dealloc()` / `realloc()` 方法
- **可替换性**：支持运行时替换默认分配器（追踪分配器、内存池等）
- **ABI 稳定**：接口不涉及模板,跨 DLL 边界安全
- **对齐支持**：所有分配至少对齐到 `alignof(max_align_t)`
- **失败处理**：分配失败返回 `nullptr`,由调用方通过 `MINE_CHECK` 检查

### 设计动机

直接使用 `malloc/free` 存在以下问题:

1. **无法追踪**：内存泄漏难以诊断
2. **无法定制**：无法使用内存池或自定义分配策略
3. **跨边界不安全**：不同 CRT 版本的堆不兼容

通过 `IAllocator` 接口:

- 测试时可替换为追踪分配器,检测泄漏
- 生产时可替换为内存池,减少碎片
- 跨 DLL 边界统一使用同一分配器实例

---

## IAllocator 接口

### 类定义

```cpp
class MINE_API IAllocator {
public:
    virtual ~IAllocator() noexcept = default;

    [[nodiscard]] virtual void* alloc(
        size_t bytes,
        size_t align = alignof(max_align_t)) noexcept = 0;

    virtual void dealloc(
        void*  ptr,
        size_t bytes,
        size_t align = alignof(max_align_t)) noexcept = 0;

    [[nodiscard]] virtual void* realloc(
        void*  ptr,
        size_t old_size,
        size_t new_size,
        size_t align = alignof(max_align_t)) noexcept;
};
```

### alloc

```cpp
[[nodiscard]] virtual void* alloc(
    size_t bytes,
    size_t align = alignof(max_align_t)) noexcept = 0;
```

**描述**：分配至少 `bytes` 字节的内存块,对齐到 `align`。

**参数**：
- `bytes`：请求分配的字节数（0 时行为由实现定义,通常返回 `nullptr`）
- `align`：对齐要求（字节）,必须为 2 的幂,默认为 `alignof(max_align_t)`（通常 16）

**返回值**：
- 成功：指向已分配内存的指针,对齐到 `align`
- 失败：`nullptr`

**实现约定**：
- 分配的内存未初始化
- 对齐要求必须为 2 的幂（`(align & (align - 1)) == 0`）
- 返回的指针必须满足 `(uintptr_t(ptr) % align) == 0`

**调用方责任**：检查返回值,分配失败时通过 `MINE_CHECK` 终止程序。

**时间复杂度**：O(1)（取决于实现）

**示例**：

```cpp
IAllocator* allocator = mine::core::default_allocator();

// 分配 1024 字节
void* ptr = allocator->alloc(1024);
MINE_CHECK(ptr != nullptr);  // 检查分配成功

// 使用内存...
allocator->dealloc(ptr, 1024);
```

---

### dealloc

```cpp
virtual void dealloc(
    void*  ptr,
    size_t bytes,
    size_t align = alignof(max_align_t)) noexcept = 0;
```

**描述**：释放由本分配器分配的内存块。

**参数**：
- `ptr`：要释放的指针（`nullptr` 时为空操作）
- `bytes`：当初分配时的字节数（部分实现需要此信息）
- `align`：当初分配时的对齐值

**前置条件**：
- 若 `ptr != nullptr`,则 `ptr` 必须由当前分配器通过 `alloc()` 或 `realloc()` 返回
- `bytes` 和 `align` 必须与分配时的值匹配

**实现约定**：
- 对 `nullptr` 调用是安全的（无操作）
- 释放后指针变为悬垂,不得再次使用

**时间复杂度**：O(1)（取决于实现）

**示例**：

```cpp
void* ptr = allocator->alloc(128);
// 使用 ptr...
allocator->dealloc(ptr, 128);  // 释放
```

---

### realloc

```cpp
[[nodiscard]] virtual void* realloc(
    void*  ptr,
    size_t old_size,
    size_t new_size,
    size_t align = alignof(max_align_t)) noexcept;
```

**描述**：重新分配内存块（可选实现,默认为 `alloc` + `memcpy` + `dealloc`）。

**参数**：
- `ptr`：当前内存块指针（`nullptr` 时等价于 `alloc(new_size, align)`）
- `old_size`：当前大小（字节）
- `new_size`：期望新大小（字节）
- `align`：对齐要求

**返回值**：
- 成功：新内存块指针,保留原内容的前 `min(old_size, new_size)` 字节
- 失败：`nullptr`,原块保持有效

**实现约定**：
- 若 `new_size > old_size`,新增部分未初始化
- 若 `new_size < old_size`,超出部分被丢弃
- 若分配失败,原块不被释放

**默认实现**：

```cpp
void* IAllocator::realloc(void* ptr, size_t old_size, size_t new_size, size_t align) noexcept {
    if (!ptr) return alloc(new_size, align);
    if (new_size == 0) {
        dealloc(ptr, old_size, align);
        return nullptr;
    }
    
    void* new_ptr = alloc(new_size, align);
    if (!new_ptr) return nullptr;
    
    size_t copy_size = (old_size < new_size) ? old_size : new_size;
    std::memcpy(new_ptr, ptr, copy_size);
    dealloc(ptr, old_size, align);
    
    return new_ptr;
}
```

**时间复杂度**：O(min(old_size, new_size))（默认实现需拷贝）

**示例**：

```cpp
void* ptr = allocator->alloc(128);
// ... 使用 ptr ...

// 扩展到 256 字节
void* new_ptr = allocator->realloc(ptr, 128, 256);
MINE_CHECK(new_ptr != nullptr);
// 原内容保留,ptr 不再有效
```

---

## 全局默认分配器

### default_allocator

```cpp
[[nodiscard]] MINE_API IAllocator* default_allocator() noexcept;
```

**描述**：获取进程级默认分配器（基于 `malloc/free`）。

**返回值**：指向默认分配器的指针（进程生命周期内始终有效）

**线程安全**：仅设置一次,之后只读访问,线程安全。

**实现**：

```cpp
IAllocator* default_allocator() noexcept {
    static DefaultAllocator instance;
    return &instance;
}
```

**使用场景**：
- 通用内存分配
- 构造 `OwnedPtr` / `Vector` 等容器
- PIMPL 模式的实现对象

**时间复杂度**：O(1)

**示例**：

```cpp
IAllocator* alloc = mine::core::default_allocator();
void* ptr = alloc->alloc(1024);
MINE_CHECK(ptr != nullptr);
// ...
alloc->dealloc(ptr, 1024);
```

---

### set_default_allocator

```cpp
MINE_API void set_default_allocator(IAllocator* allocator) noexcept;
```

**描述**：替换默认分配器（需在任何分配发生前调用）。

**参数**：
- `allocator`：新的分配器实例（调用方负责生命周期管理）

**前置条件**：
- 必须在多线程启动之前调用
- 必须在任何 `mine::` 对象创建之前调用
- `allocator` 必须在进程生命周期内保持有效

**警告**：
- 不检查是否有已分配内存泄漏
- 调用方负责确保旧分配器的所有内存已释放
- 不提供线程同步

**使用场景**：
- 测试时替换为追踪分配器
- 生产时替换为内存池分配器
- 嵌入式系统使用静态内存池

**示例**：

```cpp
// 程序启动时
class TrackingAllocator : public mine::core::IAllocator {
    // 实现追踪逻辑...
};

int main() {
    static TrackingAllocator tracker;
    mine::core::set_default_allocator(&tracker);
    
    // 之后所有分配都通过 tracker
    run_application();
    
    // 程序退出时检查泄漏
    tracker.check_leaks();
}
```

---

## 使用场景

### 1. 容器内部分配

```cpp
template<typename T>
class Vector {
    T* data_{nullptr};
    size_t size_{0};
    size_t capacity_{0};
    IAllocator* allocator_;

public:
    Vector(IAllocator* alloc = default_allocator())
        : allocator_{alloc} {}

    void push_back(const T& value) {
        if (size_ == capacity_) {
            size_t new_cap = capacity_ ? capacity_ * 2 : 8;
            void* new_data = allocator_->alloc(new_cap * sizeof(T), alignof(T));
            MINE_CHECK(new_data != nullptr);
            
            // 移动元素到新内存...
            allocator_->dealloc(data_, capacity_ * sizeof(T), alignof(T));
            data_ = static_cast<T*>(new_data);
            capacity_ = new_cap;
        }
        new (&data_[size_++]) T(value);
    }
};
```

---

### 2. PIMPL 模式实现对象

```cpp
// Widget.h
class Widget {
    struct Impl;
    OwnedPtr<Impl> pimpl_;
public:
    Widget();
    ~Widget();
};

// Widget.cpp
struct Widget::Impl {
    Vector<Child> children;
    // ...
};

Widget::Widget() {
    IAllocator* alloc = default_allocator();
    void* mem = alloc->alloc(sizeof(Impl), alignof(Impl));
    MINE_CHECK(mem != nullptr);
    
    pimpl_.reset(new (mem) Impl{}, [alloc](Impl* p) {
        p->~Impl();
        alloc->dealloc(p, sizeof(Impl), alignof(Impl));
    });
}
```

---

### 3. 内存池替换

```cpp
class PoolAllocator : public IAllocator {
    char pool_[1024 * 1024];  // 1MB 内存池
    size_t offset_{0};

public:
    void* alloc(size_t bytes, size_t align) noexcept override {
        size_t aligned_offset = (offset_ + align - 1) & ~(align - 1);
        if (aligned_offset + bytes > sizeof(pool_))
            return nullptr;  // 池耗尽
        
        void* ptr = pool_ + aligned_offset;
        offset_ = aligned_offset + bytes;
        return ptr;
    }

    void dealloc(void*, size_t, size_t) noexcept override {
        // 简单池不支持单独释放
    }
};

// 使用
int main() {
    static PoolAllocator pool;
    set_default_allocator(&pool);
    // 所有分配从池中进行
}
```

---

## 性能分析

### 内存开销

```cpp
sizeof(IAllocator*) == 8  // 64 位系统
```

**容器开销**：每个容器存储一个 `IAllocator*` 指针（8 字节）。

### 虚函数调用开销

| 操作 | 开销 | 说明 |
|------|------|------|
| `alloc()` | 虚函数调用 + 实际分配 | 通常 < 100 cycles |
| `dealloc()` | 虚函数调用 + 实际释放 | 通常 < 50 cycles |
| `realloc()` | 虚函数调用 + 拷贝 | O(min(old, new)) |

**优化**：
- 批量分配减少调用次数
- 内联小对象（SBO）避免堆分配
- 使用内存池减少系统调用

---

## 线程安全性

### IAllocator 接口

**不保证**：`IAllocator` 接口本身不保证线程安全,由具体实现决定。

**默认分配器**：基于 `malloc/free`,线程安全（CRT 提供同步）。

**自定义分配器**：若多线程并发分配,需实现内部同步。

**示例**：

```cpp
class ThreadSafeAllocator : public IAllocator {
    std::mutex mutex_;
    // ...

public:
    void* alloc(size_t bytes, size_t align) noexcept override {
        std::lock_guard lock{mutex_};
        return do_alloc(bytes, align);
    }
};
```

---

## 最佳实践

### 1. 始终检查分配结果

**推荐**：使用 `MINE_CHECK` 检查 `alloc()` 返回值。

```cpp
void* ptr = allocator->alloc(size);
MINE_CHECK(ptr != nullptr);  // 分配失败时终止程序
```

---

### 2. 传递相同的 bytes/align 给 dealloc

**推荐**：记录分配时的参数,释放时传递相同值。

```cpp
// ✅ 正确
size_t bytes = 128;
size_t align = 16;
void* ptr = allocator->alloc(bytes, align);
allocator->dealloc(ptr, bytes, align);  // 相同参数

// ❌ 错误
allocator->dealloc(ptr, 256, 8);  // 参数不匹配
```

---

### 3. 优先使用容器而非手动分配

**推荐**：使用 `Vector`、`OwnedPtr` 等容器自动管理生命周期。

```cpp
// ❌ 手动管理（易泄漏）
void* ptr = allocator->alloc(sizeof(Widget), alignof(Widget));
auto* widget = new (ptr) Widget{};
// ... 若提前返回,ptr 泄漏

// ✅ 使用 OwnedPtr
auto widget = mine::core::make_owned<Widget>();
// 自动析构和释放
```

---

### 4. 测试时使用追踪分配器

**推荐**：在测试环境中替换为追踪分配器,检测泄漏。

```cpp
#ifdef MINE_TEST_BUILD
class TrackingAllocator : public IAllocator {
    std::map<void*, size_t> allocations_;
public:
    void* alloc(size_t bytes, size_t align) noexcept override {
        void* ptr = ::malloc(bytes);
        if (ptr) allocations_[ptr] = bytes;
        return ptr;
    }
    
    void dealloc(void* ptr, size_t, size_t) noexcept override {
        allocations_.erase(ptr);
        ::free(ptr);
    }
    
    void check_leaks() {
        if (!allocations_.empty()) {
            log_error("Memory leak detected: {} allocations",
                      allocations_.size());
        }
    }
};
#endif
```

---

## 完整示例

### 示例：自定义内存池

```cpp
#include <mine/core/Allocator.h>
#include <vector>

class BlockAllocator : public mine::core::IAllocator {
    static constexpr size_t kBlockSize = 64;
    std::vector<char*> blocks_;
    size_t current_offset_{0};

public:
    ~BlockAllocator() noexcept override {
        for (char* block : blocks_) {
            ::free(block);
        }
    }

    void* alloc(size_t bytes, size_t align) noexcept override {
        if (bytes > kBlockSize) {
            return ::malloc(bytes);  // 大对象直接分配
        }
        
        if (blocks_.empty() || current_offset_ + bytes > kBlockSize) {
            char* new_block = static_cast<char*>(::malloc(kBlockSize));
            if (!new_block) return nullptr;
            blocks_.push_back(new_block);
            current_offset_ = 0;
        }
        
        char* block = blocks_.back();
        void* ptr = block + current_offset_;
        current_offset_ += bytes;
        return ptr;
    }

    void dealloc(void* ptr, size_t bytes, size_t) noexcept override {
        if (bytes > kBlockSize) {
            ::free(ptr);  // 大对象直接释放
        }
        // 小对象不单独释放（批量释放在析构时）
    }
};

void example() {
    BlockAllocator block_alloc;
    mine::core::set_default_allocator(&block_alloc);
    
    // 所有小对象分配从池中进行
    auto vec = mine::containers::Vector<int>{};
    for (int i = 0; i < 100; ++i) {
        vec.push_back(i);
    }
    
    // 析构时批量释放所有块
}
```

---

## 总结

`IAllocator` 提供了 MineFramework 的统一内存分配接口,具备以下优势:

1. **可替换性**：支持运行时替换分配器（追踪、池化）
2. **可测试性**：追踪分配器检测内存泄漏
3. **跨边界安全**：统一分配器避免堆不兼容
4. **ABI 稳定**：纯虚接口,无模板依赖

在实现容器或需要堆分配的类时,优先通过 `IAllocator` 接口进行分配,而不是直接调用 `malloc/free`,这将显著提升代码的可测试性和灵活性。
