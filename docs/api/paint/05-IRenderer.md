# IRenderer 详细接口文档

## 概述

`IRenderer` 是 `mine.paint` 模块的**渲染器抽象接口**。

**核心特性：**
- **渲染器接口**：begin_frame、end_frame、render、set_dpi_scale
- **工厂函数**：create_renderer（创建 D3D11 后端实现）
- **平台抽象**：接受 IDevice（RHI 抽象），输出到 ITexture（RHI 纹理）

---

## 文件位置

```
src/mine/paint/api/include/mine/paint/IRenderer.h
```

---

## IRenderer 接口

### 类定义

```cpp
class IRenderer {
public:
    virtual ~IRenderer() = default;
    
    /**
     * @brief 开始一帧渲染。
     *
     * 在每帧开始时调用，初始化渲染器状态。
     */
    virtual void begin_frame() = 0;
    
    /**
     * @brief 结束一帧渲染。
     *
     * 在每帧结束时调用，提交所有渲染命令。
     */
    virtual void end_frame() = 0;
    
    /**
     * @brief 渲染 DisplayList 到目标纹理。
     *
     * @param display_list  待渲染的命令列表
     * @param target        目标纹理（RHI 纹理，ITexture*）
     */
    virtual void render(const DisplayList& display_list, gfx::rhi::ITexture* target) = 0;
    
    /**
     * @brief 设置 DPI 缩放比例。
     *
     * 用于高 DPI 显示器适配。所有绘制单位（逻辑像素）将乘以此比例转换为物理像素。
     *
     * @param scale  DPI 缩放比例（1.0 = 96 DPI，1.5 = 144 DPI，2.0 = 192 DPI）
     */
    virtual void set_dpi_scale(float scale) = 0;
};
```

**描述**：渲染器抽象接口。

**特征**：
- 纯虚接口，由具体后端实现（如 D3D11Renderer）
- 接受 DisplayList，输出到 ITexture
- 支持 DPI 缩放

---

## 工厂函数

### create_renderer()

```cpp
[[nodiscard]] core::OwnedPtr<IRenderer> create_renderer(gfx::rhi::IDevice* device);
```

**描述**：创建渲染器实例（D3D11 后端）。

**参数**：
- `device`：RHI 设备（IDevice*）

**返回**：渲染器实例（OwnedPtr<IRenderer>）

**特征**：
- M0 阶段仅支持 D3D11 后端
- M1+ 将支持 Vulkan 后端

---

## 成员方法

### IRenderer::begin_frame()

```cpp
virtual void begin_frame() = 0;
```

**描述**：开始一帧渲染。

**特征**：
- 在每帧开始时调用
- 初始化渲染器状态

---

### IRenderer::end_frame()

```cpp
virtual void end_frame() = 0;
```

**描述**：结束一帧渲染。

**特征**：
- 在每帧结束时调用
- 提交所有渲染命令

---

### IRenderer::render()

```cpp
virtual void render(const DisplayList& display_list, gfx::rhi::ITexture* target) = 0;
```

**描述**：渲染 DisplayList 到目标纹理。

**参数**：
- `display_list`：待渲染的命令列表
- `target`：目标纹理（RHI 纹理，ITexture*）

**特征**：
- 一帧内可多次调用（渲染到不同纹理）

---

### IRenderer::set_dpi_scale()

```cpp
virtual void set_dpi_scale(float scale) = 0;
```

**描述**：设置 DPI 缩放比例。

**参数**：
- `scale`：DPI 缩放比例（1.0 = 96 DPI，1.5 = 144 DPI，2.0 = 192 DPI）

**特征**：
- 用于高 DPI 显示器适配
- 所有绘制单位（逻辑像素）将乘以此比例转换为物理像素

---

## 使用场景

### 1. 基本渲染流程

```cpp
// 创建渲染器
auto renderer = create_renderer(device);
renderer->set_dpi_scale(1.0f);

// 每帧渲染
while (running) {
    renderer->begin_frame();
    
    // 渲染 UI
    Canvas canvas;
    canvas.fill_rect({10, 10, 200, 100}, Brush::solid_rgb(0xFF0000));
    DisplayList dl = canvas.end();
    
    renderer->render(dl, back_buffer);
    
    renderer->end_frame();
}
```

---

### 2. 高 DPI 适配

```cpp
// 创建渲染器
auto renderer = create_renderer(device);

// 获取 DPI 缩放比例
float dpi_scale = get_system_dpi_scale();  // 例如：1.5（144 DPI）

// 设置 DPI 缩放
renderer->set_dpi_scale(dpi_scale);

// 渲染（所有单位自动缩放）
Canvas canvas;
canvas.fill_rect({10, 10, 200, 100}, brush);  // 逻辑像素：10x10，物理像素：15x15
DisplayList dl = canvas.end();

renderer->begin_frame();
renderer->render(dl, back_buffer);
renderer->end_frame();
```

---

### 3. 多纹理渲染

```cpp
// 创建渲染器
auto renderer = create_renderer(device);

// 渲染到多个纹理
renderer->begin_frame();

// 渲染到纹理 A
Canvas canvas_a;
canvas_a.fill_rect({0, 0, 256, 256}, Brush::solid_rgb(0xFF0000));
DisplayList dl_a = canvas_a.end();
renderer->render(dl_a, texture_a);

// 渲染到纹理 B
Canvas canvas_b;
canvas_b.fill_rect({0, 0, 256, 256}, Brush::solid_rgb(0x0000FF));
DisplayList dl_b = canvas_b.end();
renderer->render(dl_b, texture_b);

renderer->end_frame();
```

---

### 4. 离屏渲染

```cpp
// 创建离屏纹理
auto offscreen_texture = device->create_texture({512, 512}, TextureFormat::RGBA8);

// 渲染到离屏纹理
Canvas canvas;
canvas.fill_rect({0, 0, 512, 512}, Brush::solid_rgb(0xFF0000));
DisplayList dl = canvas.end();

renderer->begin_frame();
renderer->render(dl, offscreen_texture.get());
renderer->end_frame();

// 保存到文件或用于后续渲染
```

---

## 性能分析

### begin_frame/end_frame 开销

**特征**：
- begin_frame 初始化渲染器状态（清空命令缓冲、重置常量缓冲区等）
- end_frame 提交所有渲染命令到 GPU
- 一帧内只调用一次

---

### render() 开销

**特征**：
- 遍历 DisplayList，逐条执行 DrawCmd
- 每条命令可能触发 GPU 绘制调用（Draw、DrawIndexed）
- SDF 渲染（多边形、圆弧、贝塞尔曲线）需要 GPU 计算

---

### DPI 缩放

**特征**：
- 所有坐标乘以 DPI 缩放比例
- 纹理分辨率也需要相应调整

---

## 最佳实践

### 1. 一帧只调用一次 begin_frame/end_frame

```cpp
// ✅ 推荐：一帧一次
renderer->begin_frame();

renderer->render(dl1, texture1);
renderer->render(dl2, texture2);

renderer->end_frame();

// ❌ 不推荐：多次调用
renderer->begin_frame();
renderer->render(dl1, texture1);
renderer->end_frame();

renderer->begin_frame();
renderer->render(dl2, texture2);
renderer->end_frame();
```

---

### 2. 合理设置 DPI 缩放

```cpp
// ✅ 推荐：根据系统 DPI 设置
float dpi_scale = get_system_dpi_scale();
renderer->set_dpi_scale(dpi_scale);

// ❌ 不推荐：固定 DPI
renderer->set_dpi_scale(1.0f);  // 高 DPI 显示器会模糊
```

---

### 3. 复用 DisplayList

```cpp
// ✅ 推荐：静态内容复用 DisplayList
Canvas canvas;
canvas.fill_rect({10, 10, 200, 100}, brush);
DisplayList dl = canvas.end();

// 多帧渲染
for (int i = 0; i < 100; ++i) {
    renderer->begin_frame();
    renderer->render(dl, back_buffer);  // 无需重新录制
    renderer->end_frame();
}

// ❌ 不推荐：每帧重新录制
for (int i = 0; i < 100; ++i) {
    Canvas canvas;
    canvas.fill_rect({10, 10, 200, 100}, brush);
    DisplayList dl = canvas.end();
    
    renderer->begin_frame();
    renderer->render(dl, back_buffer);
    renderer->end_frame();
}
```

---

### 4. 检查 target 有效性

```cpp
// ✅ 推荐：检查 target 有效性
if (target) {
    renderer->begin_frame();
    renderer->render(dl, target);
    renderer->end_frame();
}

// ❌ 不推荐：不检查
renderer->begin_frame();
renderer->render(dl, nullptr);  // 崩溃
renderer->end_frame();
```

---

## 常见陷阱

### 1. 忘记调用 begin_frame/end_frame

```cpp
// ❌ 问题：忘记调用 begin_frame/end_frame
renderer->render(dl, back_buffer);  // 错误：未开始帧

// ✅ 解决：显式调用 begin_frame/end_frame
renderer->begin_frame();
renderer->render(dl, back_buffer);
renderer->end_frame();
```

---

### 2. begin_frame/end_frame 不匹配

```cpp
// ❌ 问题：begin_frame/end_frame 不匹配
renderer->begin_frame();
renderer->render(dl1, texture1);
// 缺少 renderer->end_frame();

renderer->begin_frame();  // 错误：上一帧未结束
renderer->render(dl2, texture2);
renderer->end_frame();

// ✅ 解决：确保 begin_frame/end_frame 匹配
renderer->begin_frame();
renderer->render(dl1, texture1);
renderer->end_frame();

renderer->begin_frame();
renderer->render(dl2, texture2);
renderer->end_frame();
```

---

### 3. DPI 缩放未设置

```cpp
// ❌ 问题：DPI 缩放未设置
auto renderer = create_renderer(device);
// 缺少 renderer->set_dpi_scale(scale);

renderer->begin_frame();
renderer->render(dl, back_buffer);  // 高 DPI 显示器会模糊
renderer->end_frame();

// ✅ 解决：设置 DPI 缩放
auto renderer = create_renderer(device);
float dpi_scale = get_system_dpi_scale();
renderer->set_dpi_scale(dpi_scale);

renderer->begin_frame();
renderer->render(dl, back_buffer);
renderer->end_frame();
```

---

### 4. 渲染到无效纹理

```cpp
// ❌ 问题：渲染到无效纹理
ITexture* target = nullptr;
renderer->begin_frame();
renderer->render(dl, target);  // 崩溃
renderer->end_frame();

// ✅ 解决：检查纹理有效性
ITexture* target = get_back_buffer();
if (target) {
    renderer->begin_frame();
    renderer->render(dl, target);
    renderer->end_frame();
}
```

---

## 完整示例

```cpp
#include <mine/paint/IRenderer.h>
#include <mine/paint/Canvas.h>
#include <mine/paint/Brush.h>
#include <mine/gfx/rhi/IDevice.h>
#include <mine/gfx/rhi/ITexture.h>

using namespace mine::paint;
using namespace mine::gfx::rhi;

class Application {
public:
    void init() {
        // 创建 RHI 设备
        device_ = create_device();
        
        // 创建渲染器
        renderer_ = create_renderer(device_.get());
        
        // 设置 DPI 缩放
        float dpi_scale = get_system_dpi_scale();
        renderer_->set_dpi_scale(dpi_scale);
        
        // 创建后台缓冲
        back_buffer_ = device_->create_back_buffer({800, 600}, TextureFormat::RGBA8);
    }
    
    void render_frame() {
        // 开始帧
        renderer_->begin_frame();
        
        // 绘制 UI
        Canvas canvas;
        
        // 背景
        canvas.fill_rect({0, 0, 800, 600}, Brush::solid_rgb(0xFFFFFF));
        
        // 标题
        canvas.fill_rect({10, 10, 780, 60}, Brush::solid_rgb(0x3498DB));
        
        // 按钮
        canvas.fill_rounded_rect({{100, 100, 200, 50}, 8.f}, Brush::solid_rgb(0x2ECC71));
        canvas.fill_rounded_rect({{100, 170, 200, 50}, 8.f}, Brush::solid_rgb(0xE74C3C));
        
        // 完成录制
        DisplayList dl = canvas.end();
        
        // 渲染到后台缓冲
        renderer_->render(dl, back_buffer_.get());
        
        // 结束帧
        renderer_->end_frame();
        
        // 显示
        device_->present();
    }
    
    void cleanup() {
        renderer_.reset();
        back_buffer_.reset();
        device_.reset();
    }
    
private:
    core::OwnedPtr<IDevice>   device_;
    core::OwnedPtr<IRenderer> renderer_;
    core::OwnedPtr<ITexture>  back_buffer_;
};

int main() {
    Application app;
    app.init();
    
    // 渲染循环
    while (true) {
        app.render_frame();
    }
    
    app.cleanup();
    return 0;
}
```

---

## 总结

`IRenderer` 是 `mine.paint` 模块的渲染器抽象接口，具备：

1. **渲染器接口**：begin_frame、end_frame、render、set_dpi_scale
2. **工厂函数**：create_renderer（创建 D3D11 后端实现）
3. **平台抽象**：接受 IDevice（RHI 抽象），输出到 ITexture（RHI 纹理）

**使用建议**：
- 一帧只调用一次 begin_frame/end_frame
- 合理设置 DPI 缩放
- 复用 DisplayList
- 检查 target 有效性
- 确保 begin_frame/end_frame 匹配
