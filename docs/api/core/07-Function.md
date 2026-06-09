# Function<R(Args...)> 详细接口文档

## 概述

`Function<R(Args...)>` 是 MineFramework 中的移动专用函数包装器,提供带小缓冲区优化（Small Buffer Optimization, SBO）的可调用对象存储。它是 `std::function` 的轻量级替代,符合项目禁用异常和 RTTI 的约束。

### 核心特性

- **仅移动语义**：不支持拷贝,避免昂贵的闭包复制
- **SBO 优化**：32 字节内联存储,无堆分配
- **零异常**：所有接口均为 `noexcept`,符合项目约束
- **编译期检查**：超出 SBO 阈值时触发 `static_assert`,而非运行时分配
- **虚表优化**：使用静态函数指针表代替虚函数,节省一个指针开销
- **类型擦除**：统一接口,支持 lambda、函数指针、函数对象

### 设计动机

MineFramework 禁用 C++ 异常和 RTTI,使得 `std::function` 不适用:

1. **异常依赖**：`std::function` 在目标为空时调用会抛出 `std::bad_function_call`
2. **堆分配**：大型闭包会触发动态分配,在热路径上引入性能开销
3. **拷贝语义**：`std::function` 可拷贝,导致不必要的闭包复制

`Function<R(Args...)>` 解决了以上问题:

- 空函数调用触发 `MINE_ASSERT` 而非抛异常
- SBO 限制为 32 字节,超出时编译期失败（明确告知开发者）
- 仅移动语义,避免意外拷贝
- 不依赖 RTTI（无 `typeid`、`dynamic_cast`）

### 典型使用场景

| 场景 | 用法示例 |
|------|----------|
| 属性绑定回调 | `Function<Variant()> getter` |
| 事件处理器 | `Function<void(MouseEvent)> on_click` |
| 异步任务 | `Function<void()> task` |
| 延迟执行 | `Function<int()> deferred_compute` |
| 状态机转换 | `Function<State()> transition` |

---

## 类定义

```cpp
namespace mine::core {

template<class R, class... Args>
class Function<R(Args...)> {
public:
    static constexpr size_t kSBOSize  = 32;
    static constexpr size_t kSBOAlign = alignof(max_align_t);

    // 构造
    Function() noexcept = default;
    template<class F> Function(F&& f) noexcept;
    ~Function() noexcept;

    // 移动语义
    Function(Function&& o) noexcept;
    Function& operator=(Function&& o) noexcept;

    // 禁止拷贝
    Function(const Function&)            = delete;
    Function& operator=(const Function&) = delete;

    // 操作
    void reset() noexcept;
    [[nodiscard]] explicit operator bool() const noexcept;

    // 调用
    R operator()(Args... args) const noexcept;

private:
    struct Ops {
        R    (*invoke    )(void* self, Args...) noexcept;
        void (*destroy   )(void* self) noexcept;
        void (*move_from )(void* dst, void* src) noexcept;
    };

    alignas(kSBOAlign) mutable char buf_[kSBOSize]{};
    const Ops* ops_ = nullptr;
};

} // namespace mine::core
```

### 常量成员

| 常量 | 值 | 说明 |
|------|----|----|
| `kSBOSize` | 32 | SBO 缓冲区字节数（4 个指针大小） |
| `kSBOAlign` | `alignof(max_align_t)` | SBO 缓冲区对齐要求（通常 16） |

**SBO 阈值**：闭包大小 ≤ 32 字节时内联存储,超出触发 `static_assert`。

---

## 构造函数

### 默认构造

```cpp
Function() noexcept = default;
```

**描述**：构造空函数包装器,`operator bool()` 返回 `false`。

**后置条件**：
- `ops_ == nullptr`
- 调用 `operator()` 会触发断言

**时间复杂度**：O(1)

**示例**：

```cpp
mine::core::Function<int()> empty;
MINE_ASSERT(!empty);  // 空状态
// empty();           // Debug: 触发断言
```

---

### 从可调用对象构造

```cpp
template<class F, class = std::enable_if_t<!std::is_same_v<std::decay_t<F>, Function>>>
Function(F&& f) noexcept;
```

**描述**：从任意可调用对象（lambda、函数指针、函数对象）构造包装器。

**模板参数**：
- `F`：可调用类型（自动推导）

**约束（编译期检查）**：
1. `sizeof(Decay<F>) ≤ 32`（超出触发 `static_assert`）
2. `alignof(Decay<F>) ≤ alignof(max_align_t)`
3. `std::is_nothrow_move_constructible_v<Decay<F>>`（确保移动不抛异常）

**参数**：
- `f`：可调用对象（右值引用,将被移动到 SBO 缓冲区）

**效果**：
1. 为 `Decay<F>` 类型生成静态操作表 `Ops`
2. 将 `f` 移动/拷贝到 SBO 缓冲区 `buf_`
3. 设置 `ops_` 指向该类型的操作表

**隐式转换**：允许隐式转换（标记 `// NOLINT(google-explicit-constructor)`）,因为：
- lambda 到 `Function` 的转换是无损的
- 不涉及内存分配
- 符合直觉（函数对象本质上是可调用的）

**时间复杂度**：O(1) + `F` 的移动/拷贝开销

**示例**：

```cpp
// Lambda（捕获列表 ≤ 32 字节）
int x = 42;
mine::core::Function<int()> fn1 = [x]() noexcept { return x; };
MINE_ASSERT(fn1() == 42);

// 函数指针
int add(int a, int b) { return a + b; }
mine::core::Function<int(int, int)> fn2 = add;
MINE_ASSERT(fn2(1, 2) == 3);

// 函数对象
struct Multiplier {
    int factor;
    int operator()(int x) const noexcept { return x * factor; }
};
mine::core::Function<int(int)> fn3 = Multiplier{10};
MINE_ASSERT(fn3(5) == 50);

// ❌ 编译错误：超出 SBO 阈值
// char big_capture[64];
// mine::core::Function<void()> bad = [big_capture]() noexcept {};
// static_assert 失败："捕获列表过大,超过 32 字节 SBO 上限"
```

---

### 析构函数

```cpp
~Function() noexcept;
```

**描述**：析构被包装的可调用对象。

**效果**：
- 若 `ops_ != nullptr`：调用 `ops_->destroy(buf_)`
- 否则：无操作

**实现细节**：

```cpp
~Function() noexcept { reset(); }

void reset() noexcept {
    if (ops_) {
        ops_->destroy(buf_);
        ops_ = nullptr;
    }
}
```

**时间复杂度**：O(1) + 被包装类型的析构开销

---

### 移动构造

```cpp
Function(Function&& o) noexcept;
```

**描述**：移动构造,转移被包装对象的所有权。

**参数**：
- `o`：源 `Function` 对象（右值引用）

**效果**：
1. 若 `o.ops_ != nullptr`：
   - 调用 `o.ops_->move_from(this->buf_, o.buf_)`
   - 设置 `this->ops_ = o.ops_`
   - 清空 `o.ops_ = nullptr`
2. 否则：`this` 保持空状态

**后置条件**：
- `this` 持有原 `o` 的可调用对象
- `o` 变为空状态（`o.ops_ == nullptr`）

**时间复杂度**：O(1) + 被包装类型的移动构造开销

**示例**：

```cpp
int x = 10;
mine::core::Function<int()> fn1 = [x]() noexcept { return x * 2; };

mine::core::Function<int()> fn2 = std::move(fn1);  // 移动构造
MINE_ASSERT(fn2() == 20);
MINE_ASSERT(!fn1);  // fn1 现在为空
```

---

### 移动赋值

```cpp
Function& operator=(Function&& o) noexcept;
```

**描述**：移动赋值,先销毁当前对象再转移所有权。

**参数**：
- `o`：源 `Function` 对象（右值引用）

**效果**：
1. 检查自赋值（`this != &o`）
2. 调用 `this->reset()` 销毁当前对象
3. 通过 placement new 调用移动构造重建 `*this`

**实现细节**：

```cpp
Function& operator=(Function&& o) noexcept {
    if (this != &o) {
        reset();
        new(this) Function(std::move(o));
    }
    return *this;
}
```

**时间复杂度**：O(1) + 旧对象析构 + 新对象移动构造

**示例**：

```cpp
mine::core::Function<int()> fn1 = []() noexcept { return 42; };
mine::core::Function<int()> fn2 = []() noexcept { return 99; };

fn2 = std::move(fn1);  // 移动赋值
MINE_ASSERT(fn2() == 42);
MINE_ASSERT(!fn1);
```

---

## 操作方法

### reset

```cpp
void reset() noexcept;
```

**描述**：清空包装对象,析构被包装类型,恢复为空状态。

**效果**：
- 若 `ops_ != nullptr`：
  - 调用 `ops_->destroy(buf_)` 析构对象
  - 设置 `ops_ = nullptr`
- 否则：无操作

**后置条件**：
- `operator bool()` 返回 `false`
- 再次调用 `operator()` 会触发断言

**时间复杂度**：O(1) + 被包装类型的析构开销

**示例**：

```cpp
mine::core::Function<int()> fn = []() noexcept { return 42; };
MINE_ASSERT(fn);  // 非空

fn.reset();
MINE_ASSERT(!fn);  // 空状态
// fn();           // Debug: 触发断言
```

---

### operator bool

```cpp
[[nodiscard]] explicit operator bool() const noexcept;
```

**描述**：检查是否持有有效的可调用对象。

**返回值**：
- `true`：持有有效对象,可安全调用 `operator()`
- `false`：空状态,调用 `operator()` 会触发断言

**显式转换**：标记 `explicit`,防止意外的隐式转换。

**时间复杂度**：O(1)

**示例**：

```cpp
mine::core::Function<void()> fn;
if (!fn) {
    // 空状态,不能调用
}

fn = []() noexcept { /* ... */ };
if (fn) {
    fn();  // 安全调用
}
```

---

## 调用运算符

### operator()

```cpp
R operator()(Args... args) const noexcept;
```

**描述**：调用被包装的可调用对象。

**参数**：
- `args`：转发给被包装对象的参数

**返回值**：被包装对象的返回值（类型 `R`）

**前置条件**：`operator bool() == true`

**断言**：Debug 模式下检查 `ops_ != nullptr`,空状态时触发断言：

```
MINE_ASSERT(ops_ != nullptr);
```

**未定义行为**：在空状态下调用且未触发断言时,行为未定义。

**const 修饰**：标记 `const`,支持多次调用（`buf_` 标记 `mutable`）。

**时间复杂度**：O(1) + 被包装对象的执行开销

**实现细节**：

```cpp
R operator()(Args... args) const noexcept {
    MINE_ASSERT(ops_ != nullptr);
    return ops_->invoke(buf_, std::forward<Args>(args)...);
}
```

**示例**：

```cpp
// 无参数
mine::core::Function<int()> fn1 = []() noexcept { return 42; };
int result = fn1();
MINE_ASSERT(result == 42);

// 带参数
mine::core::Function<int(int, int)> fn2 = [](int a, int b) noexcept { return a + b; };
int sum = fn2(10, 20);
MINE_ASSERT(sum == 30);

// 可变对象（闭包捕获）
int counter = 0;
mine::core::Function<void()> fn3 = [&counter]() noexcept { ++counter; };
fn3();
fn3();
MINE_ASSERT(counter == 2);
```

---

## 操作表（Ops）设计

### Ops 结构

```cpp
struct Ops {
    R    (*invoke    )(void* self, Args...) noexcept;
    void (*destroy   )(void* self) noexcept;
    void (*move_from )(void* dst, void* src) noexcept;
};
```

**用途**：替代虚函数表,实现类型擦除的动态分发。

### 操作表生成

对于每个被包装类型 `Decay`,生成一个静态 `Ops` 实例:

```cpp
template<class F>
Function(F&& f) noexcept {
    using Decay = std::decay_t<F>;
    
    static const Ops ops{
        // invoke：将缓冲区解释为 Decay 并转发调用
        [](void* self, Args... args) noexcept -> R {
            return (*static_cast<Decay*>(self))(std::forward<Args>(args)...);
        },
        // destroy：调用 Decay 析构函数
        [](void* self) noexcept {
            static_cast<Decay*>(self)->~Decay();
        },
        // move_from：从 src 移动构造到 dst,然后析构 src
        [](void* dst, void* src) noexcept {
            ::new(dst) Decay(std::move(*static_cast<Decay*>(src)));
            static_cast<Decay*>(src)->~Decay();
        }
    };
    
    ::new(buf_) Decay(std::forward<F>(f));
    ops_ = &ops;
}
```

### 优势

| 特性 | 虚函数表 | 静态 Ops 表 |
|------|---------|-------------|
| 对象大小 | +8 字节（vptr） | 无额外开销 |
| 间接调用 | 通过 vptr | 通过 ops_ |
| 类型安全 | 编译期检查 | 编译期检查 |
| RTTI 依赖 | 需要 | 不需要 |
| 代码生成 | 每个类型一份虚表 | 每个类型一份 Ops 实例 |

**结论**：`Ops` 表与虚函数表性能相当,但避免了 RTTI 依赖和额外的 vptr 开销。

---

## 使用场景

### 1. 属性绑定回调

**问题**：属性系统需要存储 getter/setter 闭包。

**解决方案**：使用 `Function<Variant()>` 存储 getter。

```cpp
class Property {
    mine::core::Function<Variant()> getter_;
    mine::core::Function<void(Variant)> setter_;

public:
    template<typename T>
    void bind_getter(T&& getter) {
        getter_ = std::forward<T>(getter);
    }

    Variant get() const {
        if (getter_) return getter_();
        return {};
    }
};

// 使用示例
Property prop;
int value = 42;
prop.bind_getter([&value]() noexcept -> Variant {
    return Variant::from(value);
});
```

---

### 2. 事件处理器

**问题**：UI 控件需要存储事件回调。

**解决方案**：使用 `Function<void(Event)>` 存储处理器。

```cpp
class Button {
    mine::core::Function<void(MouseEvent)> on_click_;

public:
    void set_on_click(mine::core::Function<void(MouseEvent)> handler) {
        on_click_ = std::move(handler);
    }

    void handle_click(const MouseEvent& event) {
        if (on_click_) {
            on_click_(event);
        }
    }
};

// 使用示例
Button btn;
btn.set_on_click([](MouseEvent e) noexcept {
    if (e.button == MouseButton::Left) {
        do_something();
    }
});
```

---

### 3. 异步任务队列

**问题**：任务队列需要存储不同类型的可调用对象。

**解决方案**：使用 `Function<void()>` 统一接口。

```cpp
class TaskQueue {
    mine::containers::Vector<mine::core::Function<void()>> tasks_;

public:
    template<typename F>
    void enqueue(F&& task) {
        tasks_.push_back(std::forward<F>(task));
    }

    void execute_all() {
        for (auto& task : tasks_) {
            if (task) task();
        }
        tasks_.clear();
    }
};

// 使用示例
TaskQueue queue;
queue.enqueue([]() noexcept { load_texture("icon.png"); });
queue.enqueue([]() noexcept { init_audio_system(); });
queue.execute_all();
```

---

### 4. 延迟求值

**问题**：某些计算需要延迟到实际使用时。

**解决方案**：使用 `Function<T()>` 包装计算逻辑。

```cpp
template<typename T>
class Lazy {
    mutable mine::core::Function<T()> factory_;
    mutable std::optional<T> value_;

public:
    Lazy(mine::core::Function<T()> factory)
        : factory_{std::move(factory)} {}

    const T& get() const {
        if (!value_ && factory_) {
            value_ = factory_();
            factory_.reset();  // 释放闭包
        }
        return *value_;
    }
};

// 使用示例
Lazy<int> expensive_calc{[]() noexcept -> int {
    // 昂贵的计算...
    return 42;
}};

// 首次访问时才计算
int result = expensive_calc.get();
```

---

### 5. 状态机转换

**问题**：状态机的转换逻辑因状态而异。

**解决方案**：使用 `Function<State()>` 存储转换函数。

```cpp
enum class State { Idle, Loading, Playing };

class StateMachine {
    State current_{State::Idle};
    mine::core::Function<State()> transition_;

public:
    void set_transition(mine::core::Function<State()> fn) {
        transition_ = std::move(fn);
    }

    void update() {
        if (transition_) {
            current_ = transition_();
        }
    }
};

// 使用示例
StateMachine fsm;
fsm.set_transition([]() noexcept -> State {
    if (is_ready()) return State::Playing;
    return State::Loading;
});
```

---

## 性能分析

### 内存布局

```cpp
class Function<R(Args...)> {
    alignas(kSBOAlign) mutable char buf_[32];  // SBO 缓冲区
    const Ops* ops_;                            // 操作表指针
};
```

**大小**：

```cpp
sizeof(Function<R(Args...)>) == 40  // 32 (buf_) + 8 (ops_)
```

**对齐**：

```cpp
alignof(Function<R(Args...)>) == 16  // alignof(max_align_t)
```

### 操作时间复杂度

| 操作 | 时间复杂度 | 说明 |
|------|-----------|------|
| 构造 | O(1) + `F` 移动/拷贝 | 就地构造到 SBO 缓冲区 |
| 析构 | O(1) + `F` 析构 | 调用 `destroy` 函数指针 |
| 移动构造 | O(1) + `F` 移动构造 | 调用 `move_from` 函数指针 |
| `operator bool` | O(1) | 检查 `ops_` 是否为空 |
| `operator()` | O(1) + `F::operator()` | 调用 `invoke` 函数指针 |
| `reset` | O(1) + `F` 析构 | 调用 `destroy` 并清空 `ops_` |

### 调用开销

**间接调用**：`operator()` 涉及一次函数指针间接调用,相当于虚函数调用开销。

**内联限制**：由于函数指针,编译器通常无法内联 `operator()`（除非进行跨翻译单元优化）。

**寄存器传递**：`Function` 本身（40 字节）超出大多数架构的寄存器传递阈值,通常通过栈或引用传递。

**示例（x86-64）**：

```cpp
mine::core::Function<int()> fn = []() noexcept { return 42; };
int result = fn();

// 生成汇编（近似）
lea  rdi, [fn.buf_]       ; self = &buf_
mov  rax, [fn.ops_]       ; rax = ops_
call [rax]                ; 调用 ops_->invoke (间接调用)
```

---

## 线程安全性

### 不可变 Function

**安全**：多个线程同时调用 `const Function` 是安全的（前提是被包装对象线程安全）。

```cpp
const mine::core::Function<int()> shared = []() noexcept { return 42; };

// 线程 1
int r1 = shared();

// 线程 2（同时执行）
int r2 = shared();
```

### 可变 Function

**不安全**：并发修改（赋值、`reset()`）或一写多读需要外部同步。

```cpp
mine::core::Function<int()> fn = []() noexcept { return 10; };

// ❌ 数据竞争
// 线程 1: fn = []() noexcept { return 20; };
// 线程 2: int x = fn();

// ✅ 外部同步
std::mutex mtx;
// 线程 1: { std::lock_guard lock{mtx}; fn = ...; }
// 线程 2: { std::lock_guard lock{mtx}; int x = fn(); }
```

---

## 限制与注意事项

### 1. SBO 阈值限制

**限制**：闭包大小必须 ≤ 32 字节,超出触发 `static_assert`。

**示例**：

```cpp
// ✅ OK：捕获 4 个指针（32 字节）
void* p1, *p2, *p3, *p4;
mine::core::Function<void()> fn1 = [=]() noexcept {};

// ❌ 编译错误：捕获 5 个指针（40 字节）
void* p1, *p2, *p3, *p4, *p5;
mine::core::Function<void()> fn2 = [=]() noexcept {};
// static_assert 失败："捕获列表过大,超过 32 字节 SBO 上限"
```

**解决方案**：

```cpp
// 方案 1：减少捕获变量
struct Context { void *p1, *p2, *p3, *p4, *p5; };
Context* ctx = ...;
mine::core::Function<void()> fn = [ctx]() noexcept { /* 使用 ctx-> */ };

// 方案 2：使用引用捕获（注意生命周期）
mine::core::Function<void()> fn = [&p1, &p2, &p3, &p4, &p5]() noexcept {};
```

---

### 2. 不支持拷贝

**限制**：`Function` 不可拷贝,仅可移动。

```cpp
mine::core::Function<int()> fn1 = []() noexcept { return 42; };

// ❌ 编译错误：拷贝构造被删除
// mine::core::Function<int()> fn2 = fn1;

// ✅ OK：移动
mine::core::Function<int()> fn2 = std::move(fn1);
```

**原因**：
1. 避免昂贵的闭包拷贝
2. 某些闭包类型（如捕获 `OwnedPtr`）本身不可拷贝

---

### 3. 必须 noexcept 移动构造

**限制**：被包装类型必须 `noexcept` 移动构造,否则 `static_assert` 失败。

```cpp
struct NotNoexcept {
    NotNoexcept(NotNoexcept&&) {}  // 未标记 noexcept
    void operator()() const {}
};

// ❌ 编译错误
// mine::core::Function<void()> fn = NotNoexcept{};
// static_assert 失败："被包装类型须为 noexcept 移动可构造"
```

**原因**：项目禁用异常,移动操作必须无异常。

---

### 4. 空函数调用触发断言

**限制**：在空状态下调用 `operator()` 在 Debug 模式触发断言,Release 模式为未定义行为。

**最佳实践**：

```cpp
// ❌ 不安全
mine::core::Function<void()> fn;
fn();  // Debug: 触发断言; Release: 未定义行为

// ✅ 安全检查
if (fn) {
    fn();
}
```

---

### 5. 捕获引用的生命周期问题

**限制**：引用捕获的对象必须在 `Function` 使用期间保持有效。

```cpp
// ❌ 危险：悬垂引用
mine::core::Function<int()> create_dangling() {
    int local = 42;
    return [&local]() noexcept { return local; };  // 捕获局部变量引用
}

auto fn = create_dangling();
int x = fn();  // 未定义行为：local 已销毁

// ✅ 安全：值捕获
mine::core::Function<int()> create_safe() {
    int local = 42;
    return [local]() noexcept { return local; };  // 值捕获
}
```

---

## 最佳实践

### 1. 优先使用值捕获

**推荐**：除非明确需要引用语义,优先使用值捕获。

```cpp
// ❌ 易出错（生命周期管理复杂）
int x = 10;
mine::core::Function<int()> fn1 = [&x]() noexcept { return x; };

// ✅ 更安全
mine::core::Function<int()> fn2 = [x]() noexcept { return x; };
```

---

### 2. 减少捕获列表大小

**推荐**：通过指针或引用减少捕获变量数量。

```cpp
// ❌ 捕获多个大对象
std::string s1, s2, s3, s4;
mine::core::Function<void()> bad = [=]() noexcept { /* ... */ };  // 可能超出 32 字节

// ✅ 捕获指向结构体的指针
struct Context { std::string s1, s2, s3, s4; };
Context* ctx = ...;
mine::core::Function<void()> good = [ctx]() noexcept { /* 使用 ctx-> */ };
```

---

### 3. 调用前检查非空

**推荐**：在调用前显式检查 `operator bool()`。

```cpp
void execute(const mine::core::Function<void()>& task) {
    if (task) {  // 显式检查
        task();
    }
}
```

---

### 4. 使用 std::move 转移所有权

**推荐**：传递 `Function` 时使用 `std::move` 明确转移所有权。

```cpp
void schedule_task(mine::core::Function<void()> task) {
    task_queue_.push_back(std::move(task));
}

// 调用示例
mine::core::Function<void()> fn = []() noexcept { /* ... */ };
schedule_task(std::move(fn));  // 显式移动
MINE_ASSERT(!fn);  // fn 现在为空
```

---

### 5. 避免在 Function 中存储 Function

**推荐**：避免嵌套 `Function`,会导致双重间接调用开销。

```cpp
// ❌ 不推荐：双重间接调用
mine::core::Function<void()> outer = []() noexcept {
    mine::core::Function<void()> inner = []() noexcept { /* ... */ };
    inner();
};

// ✅ 推荐：直接存储 lambda
mine::core::Function<void()> fn = []() noexcept { /* ... */ };
```

---

### 6. 标记 lambda 为 noexcept

**推荐**：显式标记 lambda 为 `noexcept`,确保符合 `Function` 约束。

```cpp
// ✅ 显式 noexcept
mine::core::Function<int()> fn = []() noexcept { return 42; };

// ⚠️ 未标记（编译器可能推导为 noexcept(false)）
// mine::core::Function<int()> bad = []() { return 42; };
```

---

## 完整示例

### 示例 1：命令模式

```cpp
#include <mine/core/Function.h>
#include <mine/containers/Vector.h>

class Command {
    mine::core::Function<void()> execute_;
    mine::core::Function<void()> undo_;

public:
    Command(mine::core::Function<void()> exec,
            mine::core::Function<void()> undo)
        : execute_{std::move(exec)}, undo_{std::move(undo)} {}

    void execute() { if (execute_) execute_(); }
    void undo()    { if (undo_)    undo_(); }
};

class CommandHistory {
    mine::containers::Vector<Command> history_;

public:
    void execute(Command cmd) {
        cmd.execute();
        history_.push_back(std::move(cmd));
    }

    void undo_last() {
        if (!history_.empty()) {
            history_.back().undo();
            history_.pop_back();
        }
    }
};

// 使用示例
void example() {
    CommandHistory history;
    
    int value = 0;
    
    // 命令：增加
    Command increment{
        [&value]() noexcept { ++value; },  // execute
        [&value]() noexcept { --value; }   // undo
    };
    
    history.execute(std::move(increment));
    MINE_ASSERT(value == 1);
    
    history.undo_last();
    MINE_ASSERT(value == 0);
}
```

---

### 示例 2：观察者模式

```cpp
#include <mine/core/Function.h>
#include <mine/containers/Vector.h>

template<typename Event>
class Observable {
    mine::containers::Vector<mine::core::Function<void(const Event&)>> observers_;

public:
    void subscribe(mine::core::Function<void(const Event&)> observer) {
        observers_.push_back(std::move(observer));
    }

    void notify(const Event& event) {
        for (auto& observer : observers_) {
            if (observer) observer(event);
        }
    }
};

// 使用示例
struct ValueChangedEvent {
    int old_value;
    int new_value;
};

void example() {
    Observable<ValueChangedEvent> observable;
    
    // 订阅者 1：打印日志
    observable.subscribe([](const ValueChangedEvent& e) noexcept {
        log_info("Value changed: {} -> {}", e.old_value, e.new_value);
    });
    
    // 订阅者 2：更新 UI
    observable.subscribe([](const ValueChangedEvent& e) noexcept {
        update_ui_label(e.new_value);
    });
    
    // 触发事件
    observable.notify(ValueChangedEvent{10, 20});
}
```

---

### 示例 3：延迟初始化

```cpp
#include <mine/core/Function.h>
#include <optional>

template<typename T>
class LazyInit {
    mine::core::Function<T()> factory_;
    mutable std::optional<T> value_;

public:
    explicit LazyInit(mine::core::Function<T()> factory)
        : factory_{std::move(factory)} {}

    const T& get() const {
        if (!value_ && factory_) {
            value_ = factory_();
            factory_.reset();  // 释放闭包,节省内存
        }
        return *value_;
    }

    bool is_initialized() const { return value_.has_value(); }
};

// 使用示例
struct ExpensiveResource {
    ExpensiveResource() { /* 耗时初始化 */ }
    void use() const { /* ... */ }
};

void example() {
    LazyInit<ExpensiveResource> resource{[]() noexcept -> ExpensiveResource {
        return ExpensiveResource{};  // 延迟创建
    }};
    
    MINE_ASSERT(!resource.is_initialized());  // 未初始化
    
    resource.get().use();  // 首次访问时初始化
    MINE_ASSERT(resource.is_initialized());
}
```

---

## 与 std::function 的比较

| 特性 | `mine::core::Function` | `std::function` |
|------|------------------------|-----------------|
| 拷贝语义 | ❌ 仅移动 | ✅ 可拷贝 |
| 异常支持 | ❌ `noexcept`,空调用触发断言 | ✅ 空调用抛异常 |
| 堆分配 | ❌ 无（SBO ≤ 32 字节） | ⚠️ 大闭包可能堆分配 |
| RTTI 依赖 | ❌ 无 | ✅ `target_type()` 需 RTTI |
| 编译期检查 | ✅ 超 32 字节 `static_assert` | ⚠️ 运行时堆分配 |
| 对象大小 | 40 字节 | 32-48 字节（实现相关） |
| `target()` 方法 | ❌ 不提供 | ✅ 提供 |

**适用场景**：
- `mine::core::Function`：性能关键、禁用异常、嵌入式系统
- `std::function`：通用场景、需要拷贝语义、标准库兼容

---

## 常见问题

### Q1：为什么限制 SBO 为 32 字节？

**A**：平衡内存开销和覆盖率:
- 32 字节可容纳 4 个指针（64 位）或 8 个指针（32 位）
- 覆盖大多数常见 lambda（捕获 2-3 个变量）
- 超出时强制开发者优化捕获列表,避免隐式堆分配

---

### Q2：如何存储大型闭包？

**A**：通过指针间接存储:

```cpp
struct LargeContext {
    // 大量成员...
};

auto ctx = mine::core::make_owned<LargeContext>();
LargeContext* ptr = ctx.get();

mine::core::Function<void()> fn = [ptr]() noexcept {
    // 使用 ptr->...
};
```

---

### Q3：可以存储成员函数指针吗？

**A**：需要结合对象实例:

```cpp
struct Widget {
    void on_click() { /* ... */ }
};

Widget* widget = ...;
mine::core::Function<void()> fn = [widget]() noexcept {
    widget->on_click();
};
```

---

### Q4：如何实现可拷贝的函数包装器？

**A**：自行实现 `CopyableFunction`,要求闭包类型可拷贝:

```cpp
template<class Sig>
class CopyableFunction {
    // 实现类似 std::function 的拷贝语义
};
```

---

## 总结

`Function<R(Args...)>` 是 MineFramework 中高性能的函数包装器,适用于:

1. **零异常环境**：所有接口 `noexcept`,空调用触发断言
2. **移动专用场景**：避免昂贵的闭包拷贝
3. **SBO 优化**：32 字节内联存储,无堆分配
4. **类型擦除**：统一接口,支持 lambda/函数指针/函数对象

在设计回调系统、事件处理、异步任务时,优先使用 `Function` 而非原始函数指针或 `std::function`,这将显著提升代码的类型安全性和可维护性。
