# IQueue 与 IFence 详细接口文档

## 概述

`IQueue` 和 `IFence` 是 `mine.gfx.rhi` 模块的**GPU 命令提交与同步接口**。

**核心特性：**
- **IQueue**：GPU 命令提交队列（submit、wait_idle、type）
- **IFence**：GPU-CPU 同步围栏（signal、wait、completed_value）

---

## 文件位置

```
src/mine/gfx/rhi/api/include/mine/gfx/IQueue.h
src/mine/gfx/rhi/api/include/mine/gfx/IFence.h
```

---

## IQueue 接口

### 类定义

```cpp
class IQueue {
public:
    virtual ~IQueue() = default;

    /// 将命令列表提交到 GPU 执行（异步）
    virtual void submit(ICommandList* cmd) = 0;

    /// 等待此队列上所有已提交命令执行完毕（同步阻塞）
    virtual void wait_idle() = 0;

    /// 队列类型
    [[nodiscard]] virtual QueueType type() const noexcept = 0;
};
```

**描述**：GPU 命令提交队列接口。

**创建**：IDevice::create_queue(QueueType)

**用法**：将录制完毕的 ICommandList 提交到此队列，GPU 异步执行。

**同步**：通过 IFence 或 wait_idle() 等待 GPU 完成。

**特征**：
- D3D11：封装立即上下文（Immediate Context），通过 ExecuteCommandList() 提交
- D3D12/Vulkan：封装命令队列（ID3D12CommandQueue / VkQueue）

---

### IQueue::submit()

```cpp
virtual void submit(ICommandList* cmd) = 0;
```

**描述**：将命令列表提交到 GPU 执行（异步）。

**参数**：
- `cmd`：已调用 end() 的命令列表

**特征**：
- D3D11：通过立即上下文的 ExecuteCommandList() 将延迟上下文中录制的命令转交给驱动执行
- 此调用是同步的（命令被驱动接收，不保证 GPU 完成）
- 提交后立即释放命令列表的原生对象，避免持有旧后缓冲引用

---

### IQueue::wait_idle()

```cpp
virtual void wait_idle() = 0;
```

**描述**：等待此队列上所有已提交命令执行完毕（同步阻塞）。

**特征**：
- D3D11：通过 Flush() + ID3D11Query(EVENT) 实现，性能开销较大
- 仅在资源销毁 / Swapchain resize 等必要场合使用
- 性能敏感路径不应频繁调用

---

### IQueue::type()

```cpp
[[nodiscard]] virtual QueueType type() const noexcept = 0;
```

**描述**：队列类型。

**返回**：QueueType（Graphics / Compute / Copy）

---

## IFence 接口

### 类定义

```cpp
class IFence {
public:
    virtual ~IFence() = default;

    /// 发出信号，通知 GPU/CPU 当前值已达到 value
    virtual void signal(uint64_t value) = 0;

    /// 阻塞调用线程，直到围栏计数器 ≥ value 或超时
    virtual void wait(uint64_t value, uint64_t timeout_ns = 0) = 0;

    /// 返回 GPU 已完成的最新计数器值
    [[nodiscard]] virtual uint64_t completed_value() const noexcept = 0;
};
```

**描述**：GPU-CPU 同步围栏接口。

**围栏使用单调递增的 uint64 计数器：**
- GPU 提交时带上目标值（via IQueue 内部使用）
- CPU 通过 wait() 阻塞直到 GPU 完成对应值的信号
- completed_value() 返回 GPU 已完成的最新值

**特征**：
- D3D12：ID3D12Fence
- Vulkan：VkSemaphore/VkFence
- D3D11：ID3D11Query（Timestamp 或 Event）

---

### IFence::signal()

```cpp
virtual void signal(uint64_t value) = 0;
```

**描述**：发出信号，通知 GPU/CPU 当前值已达到 value。

**参数**：
- `value`：目标计数器值

**特征**：
- D3D12：通过 ID3D12CommandQueue::Signal 完成（GPU 端信号）
- D3D11：插入 GPU 事件查询并等待查询完成

---

### IFence::wait()

```cpp
virtual void wait(uint64_t value, uint64_t timeout_ns = 0) = 0;
```

**描述**：阻塞调用线程，直到围栏计数器 ≥ value 或超时。

**参数**：
- `value`：等待的目标计数器值
- `timeout_ns`：超时时间（纳秒），UINT64_MAX 表示无限等待（默认 0 = 无限等待）

**特征**：
- D3D11：轮询等待完成值达到 value，或超时返回

---

### IFence::completed_value()

```cpp
[[nodiscard]] virtual uint64_t completed_value() const noexcept = 0;
```

**描述**：返回 GPU 已完成的最新计数器值。

**返回**：uint64_t 完成值

---

## 使用场景

### 1. 基本命令提交流程

```cpp
// 创建队列
auto queue = device->create_queue(QueueType::Graphics);

// 录制命令
auto cmd = device->create_command_list();
cmd->begin();
cmd->set_render_target(swapchain->current_render_target());
cmd->clear_render_target(swapchain->current_render_target(), {0.1f, 0.1f, 0.1f, 1.0f});
cmd->draw(3, 1, 0, 0);
cmd->end();

// 提交到 GPU
queue->submit(cmd.get());

// 等待 GPU 完成
queue->wait_idle();
```

---

### 2. 使用围栏同步

```cpp
// 创建围栏
auto fence = device->create_fence(0);

// 提交命令并发出信号
queue->submit(cmd.get());
fence->signal(1);

// CPU 等待 GPU 完成
fence->wait(1, UINT64_MAX);  // 无限等待

// 查询完成值
uint64_t completed = fence->completed_value();
```

---

### 3. 多帧并行（Fence 计数器）

```cpp
uint64_t frame_index = 0;
auto fence = device->create_fence(0);

while (running) {
    frame_index++;

    // 等待上一帧完成（双缓冲）
    if (frame_index >= 2) {
        fence->wait(frame_index - 2, UINT64_MAX);
    }

    // 录制并提交当前帧
    cmd->begin();
    cmd->clear_render_target(swapchain->current_render_target(), {0.0f, 0.0f, 0.0f, 1.0f});
    cmd->draw(3, 1, 0, 0);
    cmd->end();
    queue->submit(cmd.get());
    fence->signal(frame_index);
}
```

---

### 4. 资源销毁前等待 GPU 完成

```cpp
// 提交命令
queue->submit(cmd.get());

// 等待 GPU 完成
queue->wait_idle();

// 安全销毁资源
vertex_buffer.reset();
texture.reset();
```

---

### 5. 交换链 resize 前等待 GPU 完成

```cpp
// 等待 GPU 完成
queue->wait_idle();

// 安全 resize
swapchain->resize(new_width, new_height);
```

---

## 性能分析

### IQueue::submit() 开销

**特征**：
- D3D11：ExecuteCommandList() 开销中等（~0.1-0.5 ms）
- 提交后立即释放命令列表的原生对象，避免持有旧后缓冲引用

---

### IQueue::wait_idle() 开销

**特征**：
- D3D11：Flush() + 轮询 ID3D11Query，开销较大（~1-10 ms）
- 仅在必要场合使用（资源销毁、resize）

---

### IFence::signal/wait 开销

**特征**：
- D3D11：插入事件查询 + 轮询 GetData，开销中等（~0.5-2 ms）
- M0 阶段以轮询方式实现

---

## 最佳实践

### 1. 复用 IQueue 实例

```cpp
// ✅ 推荐：复用 IQueue 实例
auto queue = device->create_queue(QueueType::Graphics);

while (running) {
    queue->submit(cmd.get());
}

// ❌ 不推荐：每帧创建新 IQueue
while (running) {
    auto queue = device->create_queue(QueueType::Graphics);
    queue->submit(cmd.get());
}
```

---

### 2. 使用围栏实现多帧并行

```cpp
// ✅ 推荐：使用围栏实现多帧并行（双缓冲）
uint64_t frame_index = 0;
auto fence = device->create_fence(0);

while (running) {
    frame_index++;
    if (frame_index >= 2) {
        fence->wait(frame_index - 2, UINT64_MAX);
    }
    queue->submit(cmd.get());
    fence->signal(frame_index);
}

// ❌ 不推荐：每帧 wait_idle（串行）
while (running) {
    queue->submit(cmd.get());
    queue->wait_idle();  // 每帧等待 GPU 完成，无法并行
}
```

---

### 3. 仅在必要场合使用 wait_idle

```cpp
// ✅ 推荐：仅在资源销毁前使用 wait_idle
queue->wait_idle();
vertex_buffer.reset();

// ❌ 不推荐：每帧使用 wait_idle
while (running) {
    queue->submit(cmd.get());
    queue->wait_idle();  // 性能开销大
}
```

---

## 常见陷阱

### 1. 提交前忘记调用 end()

```cpp
// ❌ 问题：提交前忘记调用 end()
cmd->begin();
cmd->draw(3, 1, 0, 0);
queue->submit(cmd.get());  // 错误：未调用 end()

// ✅ 解决：提交前调用 end()
cmd->begin();
cmd->draw(3, 1, 0, 0);
cmd->end();
queue->submit(cmd.get());
```

---

### 2. 资源销毁前未等待 GPU 完成

```cpp
// ❌ 问题：资源销毁前未等待 GPU 完成
queue->submit(cmd.get());
vertex_buffer.reset();  // 错误：GPU 可能仍在使用

// ✅ 解决：资源销毁前等待 GPU 完成
queue->submit(cmd.get());
queue->wait_idle();
vertex_buffer.reset();
```

---

### 3. 交换链 resize 前未等待 GPU 完成

```cpp
// ❌ 问题：交换链 resize 前未等待 GPU 完成
swapchain->resize(new_width, new_height);  // 错误：GPU 可能仍在渲染到旧后缓冲

// ✅ 解决：resize 前等待 GPU 完成
queue->wait_idle();
swapchain->resize(new_width, new_height);
```

---

### 4. 围栏 wait 超时设置不当

```cpp
// ❌ 问题：围栏 wait 超时设置过短
fence->wait(1, 1'000'000);  // 1 ms 超时，可能 GPU 未完成

// ✅ 解决：使用无限等待或合理超时
fence->wait(1, UINT64_MAX);  // 无限等待
```

---

## 完整示例

```cpp
#include <mine/gfx/Gfx.h>

using namespace mine::gfx;

int main() {
    // 创建设备
    auto device = create_device(Backend::D3D11);
    
    // 创建队列
    auto queue = device->create_queue(QueueType::Graphics);
    
    // 创建围栏
    auto fence = device->create_fence(0);
    
    // 创建命令列表
    auto cmd = device->create_command_list();
    
    uint64_t frame_index = 0;
    
    while (running) {
        frame_index++;
        
        // 等待上一帧完成（双缓冲）
        if (frame_index >= 2) {
            fence->wait(frame_index - 2, UINT64_MAX);
        }
        
        // 录制命令
        cmd->begin();
        cmd->set_render_target(swapchain->current_render_target());
        cmd->clear_render_target(swapchain->current_render_target(), {0.1f, 0.1f, 0.1f, 1.0f});
        cmd->set_viewport(viewport);
        cmd->set_pipeline(pipeline.get());
        cmd->set_vertex_buffer(0, vertex_buffer.get(), 0);
        cmd->draw(3, 1, 0, 0);
        cmd->end();
        
        // 提交到 GPU
        queue->submit(cmd.get());
        
        // 发出信号
        fence->signal(frame_index);
        
        // 呈现
        swapchain->present();
    }
    
    // 退出前等待 GPU 完成
    queue->wait_idle();
    
    return 0;
}
```

---

## 总结

`IQueue` 和 `IFence` 是 `mine.gfx.rhi` 模块的 GPU 命令提交与同步接口，具备：

1. **IQueue**：GPU 命令提交队列（submit、wait_idle、type）
2. **IFence**：GPU-CPU 同步围栏（signal、wait、completed_value）

**使用建议**：
- 复用 IQueue 实例
- 使用围栏实现多帧并行（双缓冲）
- 仅在必要场合使用 wait_idle（资源销毁、resize）
- 提交前必须调用 end()
- 资源销毁前等待 GPU 完成
- 围栏 wait 使用无限等待或合理超时
