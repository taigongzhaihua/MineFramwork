# ICommandList 详细接口文档

## 概述

`ICommandList` 是 `mine.gfx.rhi` 模块的**GPU 命令录制接口**。

**核心特性：**
- **录制控制**：begin/end
- **渲染目标**：set_render_target、clear_render_target、clear_depth_stencil
- **管线状态与视口**：set_viewport、set_scissor、set_stencil_ref
- **绘制命令**：set_pipeline、set_constant_buffer、set_vertex_buffer、set_index_buffer、draw、draw_indexed、set_shader_resource

---

## 文件位置

```
src/mine/gfx/rhi/api/include/mine/gfx/ICommandList.h
```

---

## 类定义

```cpp
class ICommandList {
public:
    virtual ~ICommandList() = default;

    // ── 录制控制 ──────────────────────────────────────────────────────────────
    virtual void begin() = 0;
    virtual void end()   = 0;

    // ── 渲染目标 ──────────────────────────────────────────────────────────────
    virtual void set_render_target(ITexture* rt, ITexture* depth = nullptr) = 0;
    virtual void clear_render_target(ITexture* rt, const Color4f& color) = 0;
    virtual void clear_depth_stencil(ITexture* depth_stencil, float depth_value = 1.0f, uint8_t stencil_value = 0) = 0;

    // ── 管线状态与视口 ────────────────────────────────────────────────────────
    virtual void set_viewport(const Viewport& viewport) = 0;
    virtual void set_scissor(const ScissorRect& rect) = 0;
    virtual void set_stencil_ref(uint8_t ref) = 0;

    // ── 绘制命令 ──────────────────────────────────────────────────────────────
    virtual void set_pipeline(IPipeline* pipeline) = 0;
    virtual void set_constant_buffer(uint32_t slot, IBuffer* buffer) = 0;
    virtual void set_vertex_buffer(uint32_t slot, IBuffer* buffer, uint64_t offset = 0) = 0;
    virtual void set_index_buffer(IBuffer* buffer, uint64_t offset = 0, bool use32bit = false) = 0;
    virtual void draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0, uint32_t first_instance = 0) = 0;
    virtual void draw_indexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t first_index = 0, int32_t base_vertex = 0, uint32_t first_instance = 0) = 0;
    virtual void set_shader_resource(uint32_t slot, ITexture* texture) = 0;
};
```

**描述**：GPU 命令录制接口（对应 D3D12 CommandList / Vulkan CommandBuffer / Metal CommandEncoder）。

**使用流程**：
1. begin() — 开始录制（重置内部状态）
2. set_render_target / clear / draw ... — 录制命令
3. end() — 结束录制
4. IQueue::submit() — 将命令列表提交到 GPU

**特征**：
- 同一时刻只允许单线程录制，不同线程应使用各自的 ICommandList 实例
- D3D12/Vulkan 后端使用真正的延迟命令列表
- D3D11 后端使用延迟上下文（Deferred Context）

---

## 成员方法

### begin()

```cpp
virtual void begin() = 0;
```

**描述**：开始录制（重置内部状态）。

**特征**：
- 清空上一帧生成的命令列表
- 重置所有绑定的资源状态
- 必须在录制命令前调用

---

### end()

```cpp
virtual void end() = 0;
```

**描述**：结束录制。

**特征**：
- 完成命令列表的生成
- 必须在提交到 GPU 前调用
- D3D11：调用 FinishCommandList() 生成 ID3D11CommandList

---

### set_render_target()

```cpp
virtual void set_render_target(ITexture* rt, ITexture* depth = nullptr) = 0;
```

**描述**：设置渲染目标和深度/模板缓冲。

**参数**：
- `rt`：渲染目标纹理（可为 nullptr）
- `depth`：深度/模板缓冲纹理（可选，默认 nullptr）

**特征**：
- 后续 draw 调用将渲染到此目标
- rt 必须以 TextureBindFlags::RenderTarget 创建
- depth 必须以 TextureBindFlags::DepthStencil 创建

---

### clear_render_target()

```cpp
virtual void clear_render_target(ITexture* rt, const Color4f& color) = 0;
```

**描述**：清屏渲染目标为指定颜色。

**参数**：
- `rt`：渲染目标纹理
- `color`：清屏颜色（线性 RGBA，范围 [0, 1]）

---

### clear_depth_stencil()

```cpp
virtual void clear_depth_stencil(ITexture* depth_stencil, float depth_value = 1.0f, uint8_t stencil_value = 0) = 0;
```

**描述**：清除深度/模板缓冲。

**参数**：
- `depth_stencil`：深度/模板缓冲纹理
- `depth_value`：深度值（默认 1.0）
- `stencil_value`：模板值（默认 0）

---

### set_viewport()

```cpp
virtual void set_viewport(const Viewport& viewport) = 0;
```

**描述**：设置视口。

**参数**：
- `viewport`：视口描述符

---

### set_scissor()

```cpp
virtual void set_scissor(const ScissorRect& rect) = 0;
```

**描述**：设置裁剪矩形。

**参数**：
- `rect`：裁剪矩形（整数像素坐标，左闭右开）

---

### set_stencil_ref()

```cpp
virtual void set_stencil_ref(uint8_t ref) = 0;
```

**描述**：动态设置模板参考值。

**参数**：
- `ref`：模板参考值（0–255）

**特征**：
- 用于嵌套裁剪栈（ClipPush/ClipPop/ClipTest）
- 在调用 set_pipeline() 之后或之前均可调用，最终在下一次 draw() 时生效

---

### set_pipeline()

```cpp
virtual void set_pipeline(IPipeline* pipeline) = 0;
```

**描述**：绑定图形/计算管线状态对象。

**参数**：
- `pipeline`：已编译的管线对象

---

### set_constant_buffer()

```cpp
virtual void set_constant_buffer(uint32_t slot, IBuffer* buffer) = 0;
```

**描述**：绑定常量缓冲（Uniform Buffer）到顶点/像素着色器。

**参数**：
- `slot`：绑定槽位（对应 HLSL: register(bN)）
- `buffer`：常量缓冲（必须以 BufferBindFlags::Constant 创建）

**特征**：
- 同时绑定到顶点着色器（VSSetConstantBuffers）和像素着色器（PSSetConstantBuffers）

---

### set_vertex_buffer()

```cpp
virtual void set_vertex_buffer(uint32_t slot, IBuffer* buffer, uint64_t offset = 0) = 0;
```

**描述**：绑定顶点缓冲。

**参数**：
- `slot`：绑定槽位
- `buffer`：顶点缓冲
- `offset`：缓冲内字节偏移（默认 0）

---

### set_index_buffer()

```cpp
virtual void set_index_buffer(IBuffer* buffer, uint64_t offset = 0, bool use32bit = false) = 0;
```

**描述**：绑定索引缓冲。

**参数**：
- `buffer`：索引缓冲
- `offset`：缓冲内字节偏移（默认 0）
- `use32bit`：是否使用 32 位索引（false = 16 位，true = 32 位，默认 false）

---

### draw()

```cpp
virtual void draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0, uint32_t first_instance = 0) = 0;
```

**描述**：绘制（非索引）。

**参数**：
- `vertex_count`：顶点数量
- `instance_count`：实例数量（默认 1）
- `first_vertex`：起始顶点索引（默认 0）
- `first_instance`：起始实例索引（默认 0）

---

### draw_indexed()

```cpp
virtual void draw_indexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t first_index = 0, int32_t base_vertex = 0, uint32_t first_instance = 0) = 0;
```

**描述**：绘制（索引）。

**参数**：
- `index_count`：索引数量
- `instance_count`：实例数量（默认 1）
- `first_index`：起始索引（默认 0）
- `base_vertex`：基础顶点偏移（默认 0）
- `first_instance`：起始实例索引（默认 0）

---

### set_shader_resource()

```cpp
virtual void set_shader_resource(uint32_t slot, ITexture* texture) = 0;
```

**描述**：绑定纹理到着色器资源槽位。

**参数**：
- `slot`：绑定槽位（对应 HLSL: register(tN)）
- `texture`：纹理（必须以 TextureBindFlags::ShaderResource 创建）

---

## 使用场景

### 1. 基本渲染流程

```cpp
// 创建命令列表
auto cmd = device->create_command_list();

// 开始录制
cmd->begin();

// 设置渲染目标
cmd->set_render_target(swapchain->current_render_target());

// 清屏
cmd->clear_render_target(swapchain->current_render_target(), {0.1f, 0.1f, 0.1f, 1.0f});

// 设置视口
Viewport vp;
vp.x = 0; vp.y = 0;
vp.width = 1920; vp.height = 1080;
vp.min_depth = 0.0f; vp.max_depth = 1.0f;
cmd->set_viewport(vp);

// 设置管线
cmd->set_pipeline(pipeline.get());

// 绑定常量缓冲
cmd->set_constant_buffer(0, constant_buffer.get());

// 绑定顶点缓冲
cmd->set_vertex_buffer(0, vertex_buffer.get(), 0);

// 绘制
cmd->draw(3, 1, 0, 0);

// 结束录制
cmd->end();

// 提交到 GPU
auto queue = device->create_queue(QueueType::Graphics);
queue->submit(cmd.get());
```

---

### 2. 索引绘制

```cpp
cmd->begin();
cmd->set_render_target(swapchain->current_render_target());
cmd->clear_render_target(swapchain->current_render_target(), {0.0f, 0.0f, 0.0f, 1.0f});
cmd->set_viewport(viewport);
cmd->set_pipeline(pipeline.get());
cmd->set_vertex_buffer(0, vertex_buffer.get(), 0);
cmd->set_index_buffer(index_buffer.get(), 0, false);  // 16 位索引
cmd->draw_indexed(index_count, 1, 0, 0, 0);
cmd->end();
```

---

### 3. 多渲染目标

```cpp
cmd->begin();

// 渲染到纹理 A
cmd->set_render_target(texture_a.get());
cmd->clear_render_target(texture_a.get(), {1.0f, 0.0f, 0.0f, 1.0f});
cmd->set_viewport(viewport_a);
cmd->set_pipeline(pipeline_a.get());
cmd->draw(3, 1, 0, 0);

// 渲染到纹理 B
cmd->set_render_target(texture_b.get());
cmd->clear_render_target(texture_b.get(), {0.0f, 1.0f, 0.0f, 1.0f});
cmd->set_viewport(viewport_b);
cmd->set_pipeline(pipeline_b.get());
cmd->draw(3, 1, 0, 0);

cmd->end();
```

---

### 4. 纹理采样

```cpp
cmd->begin();
cmd->set_render_target(swapchain->current_render_target());
cmd->clear_render_target(swapchain->current_render_target(), {0.0f, 0.0f, 0.0f, 1.0f});
cmd->set_viewport(viewport);
cmd->set_pipeline(pipeline.get());
cmd->set_constant_buffer(0, constant_buffer.get());
cmd->set_shader_resource(0, texture.get());  // 绑定纹理到槽位 0
cmd->set_vertex_buffer(0, vertex_buffer.get(), 0);
cmd->draw(6, 1, 0, 0);
cmd->end();
```

---

### 5. 模板裁剪（嵌套裁剪栈）

```cpp
cmd->begin();
cmd->set_render_target(swapchain->current_render_target());
cmd->clear_render_target(swapchain->current_render_target(), {1.0f, 1.0f, 1.0f, 1.0f});
cmd->clear_depth_stencil(depth_stencil.get(), 1.0f, 0);

// ClipPush: 绘制裁剪形状，模板值 +1
cmd->set_stencil_ref(1);
cmd->set_pipeline(clip_push_pipeline.get());
cmd->draw(clip_shape_vertex_count, 1, 0, 0);

// ClipTest: 绘制内容，仅当模板值 > 0 时通过
cmd->set_stencil_ref(1);
cmd->set_pipeline(clip_test_pipeline.get());
cmd->draw(content_vertex_count, 1, 0, 0);

// ClipPop: 绘制裁剪形状，模板值 -1
cmd->set_stencil_ref(1);
cmd->set_pipeline(clip_pop_pipeline.get());
cmd->draw(clip_shape_vertex_count, 1, 0, 0);

cmd->end();
```

---

## 性能分析

### begin/end 开销

**特征**：
- begin() 清空状态，开销较低
- end() 生成命令列表（D3D11: FinishCommandList），开销中等
- 一帧内尽量复用同一 ICommandList 实例

---

### 绘制命令开销

**特征**：
- draw/draw_indexed 是实际的 GPU 绘制调用
- 每次 draw 可能触发 GPU 状态切换
- 尽量批量绘制相同状态的对象

---

## 最佳实践

### 1. 一帧只调用一次 begin/end

```cpp
// ✅ 推荐：一帧一次 begin/end
cmd->begin();
cmd->draw(...);
cmd->draw(...);
cmd->draw(...);
cmd->end();

// ❌ 不推荐：多次 begin/end
cmd->begin();
cmd->draw(...);
cmd->end();

cmd->begin();
cmd->draw(...);
cmd->end();
```

---

### 2. 批量绘制相同状态的对象

```cpp
// ✅ 推荐：批量绘制
cmd->set_pipeline(pipeline.get());
cmd->set_constant_buffer(0, cb.get());
cmd->draw(obj1_vertex_count, 1, 0, 0);
cmd->draw(obj2_vertex_count, 1, obj1_vertex_count, 0);
cmd->draw(obj3_vertex_count, 1, obj1_vertex_count + obj2_vertex_count, 0);

// ❌ 不推荐：频繁切换状态
cmd->set_pipeline(pipeline1.get());
cmd->draw(obj1_vertex_count, 1, 0, 0);
cmd->set_pipeline(pipeline2.get());
cmd->draw(obj2_vertex_count, 1, 0, 0);
cmd->set_pipeline(pipeline3.get());
cmd->draw(obj3_vertex_count, 1, 0, 0);
```

---

### 3. 检查资源有效性

```cpp
// ✅ 推荐：检查资源有效性
if (cmd && vertex_buffer && pipeline) {
    cmd->begin();
    cmd->set_pipeline(pipeline.get());
    cmd->set_vertex_buffer(0, vertex_buffer.get(), 0);
    cmd->draw(3, 1, 0, 0);
    cmd->end();
}
```

---

## 常见陷阱

### 1. 忘记调用 begin/end

```cpp
// ❌ 问题：忘记调用 begin
cmd->set_render_target(swapchain->current_render_target());
cmd->draw(3, 1, 0, 0);

// ✅ 解决：显式调用 begin/end
cmd->begin();
cmd->set_render_target(swapchain->current_render_target());
cmd->draw(3, 1, 0, 0);
cmd->end();
```

---

### 2. 提交前忘记调用 end

```cpp
// ❌ 问题：提交前忘记调用 end
cmd->begin();
cmd->draw(3, 1, 0, 0);
queue->submit(cmd.get());  // 错误：未调用 end

// ✅ 解决：提交前调用 end
cmd->begin();
cmd->draw(3, 1, 0, 0);
cmd->end();
queue->submit(cmd.get());
```

---

### 3. 多线程同时录制同一 ICommandList

```cpp
// ❌ 问题：多线程同时录制同一 ICommandList
std::thread t1([&]() { cmd->begin(); cmd->draw(...); cmd->end(); });
std::thread t2([&]() { cmd->begin(); cmd->draw(...); cmd->end(); });

// ✅ 解决：每个线程使用各自的 ICommandList
auto cmd1 = device->create_command_list();
auto cmd2 = device->create_command_list();
std::thread t1([&]() { cmd1->begin(); cmd1->draw(...); cmd1->end(); });
std::thread t2([&]() { cmd2->begin(); cmd2->draw(...); cmd2->end(); });
```

---

## 完整示例

见 [02-IBuffer-ITexture.md 完整示例](02-IBuffer-ITexture.md#完整示例)

---

## 总结

`ICommandList` 是 `mine.gfx.rhi` 模块的 GPU 命令录制接口，具备：

1. **录制控制**：begin/end
2. **渲染目标**：set_render_target、clear_render_target、clear_depth_stencil
3. **管线状态与视口**：set_viewport、set_scissor、set_stencil_ref
4. **绘制命令**：set_pipeline、set_constant_buffer、set_vertex_buffer、set_index_buffer、draw、draw_indexed、set_shader_resource

**使用建议**：
- 一帧只调用一次 begin/end
- 批量绘制相同状态的对象
- 检查资源有效性
- 每个线程使用各自的 ICommandList 实例
- 提交前必须调用 end
