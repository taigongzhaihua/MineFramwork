# IBuffer 与 ITexture 详细接口文档

## 概述

`IBuffer` 和 `ITexture` 是 `mine.gfx.rhi` 模块的**GPU 资源抽象接口**。

**核心特性：**
- **IBuffer**：GPU 缓冲资源（顶点/索引/常量/结构化缓冲）
- **ITexture**：GPU 纹理资源（2D/3D/渲染目标/深度/模板）
- **desc() 查询**：返回创建时的描述符（只读）
- **内联访问器**：便捷访问常用属性（size/width/height/format）

---

## 文件位置

```
src/mine/gfx/rhi/api/include/mine/gfx/IBuffer.h
src/mine/gfx/rhi/api/include/mine/gfx/ITexture.h
```

---

## IBuffer 接口

### 类定义

```cpp
class IBuffer {
public:
    virtual ~IBuffer() = default;

    /// 返回创建时的描述符（只读）
    [[nodiscard]] virtual const BufferDesc& desc() const noexcept = 0;

    /// 缓冲字节大小
    [[nodiscard]] uint64_t size() const noexcept { return desc().size; }
};
```

**描述**：GPU 缓冲资源抽象接口。

**特征**：
- 通过 IDevice::create_buffer() 创建
- 由 OwnedPtr<IBuffer> 管理生命周期
- 支持顶点缓冲、索引缓冲、常量缓冲、结构化缓冲等用途（由 BufferDesc::bind_flags 决定）

---

### IBuffer::desc()

```cpp
[[nodiscard]] virtual const BufferDesc& desc() const noexcept = 0;
```

**描述**：返回创建时的描述符（只读）。

**返回**：BufferDesc& 引用

**特征**：
- 纯虚接口，由具体后端实现
- 描述符包含：size（缓冲字节大小）、stride（结构化缓冲元素步长）、bind_flags（绑定标志）

---

### IBuffer::size()

```cpp
[[nodiscard]] uint64_t size() const noexcept { return desc().size; }
```

**描述**：缓冲字节大小。

**返回**：uint64_t 缓冲字节大小

**特征**：
- 内联访问器，等价于 desc().size
- 便捷访问常用属性

---

## ITexture 接口

### 类定义

```cpp
class ITexture {
public:
    virtual ~ITexture() = default;

    /// 返回创建时的描述符（只读）
    [[nodiscard]] virtual const TextureDesc& desc() const noexcept = 0;

    /// 纹理宽度（像素）
    [[nodiscard]] uint32_t width() const noexcept { return desc().width; }

    /// 纹理高度（像素）
    [[nodiscard]] uint32_t height() const noexcept { return desc().height; }

    /// 像素格式
    [[nodiscard]] PixelFormat format() const noexcept { return desc().format; }
};
```

**描述**：GPU 纹理资源抽象接口。

**特征**：
- 通过 IDevice::create_texture() 或交换链内部创建
- 由 OwnedPtr<ITexture> 管理生命周期（用户创建的纹理）
- 交换链后缓冲纹理的生命周期由 ISwapchain 管理，返回的是裸指针，不转移所有权

---

### ITexture::desc()

```cpp
[[nodiscard]] virtual const TextureDesc& desc() const noexcept = 0;
```

**描述**：返回创建时的描述符（只读）。

**返回**：TextureDesc& 引用

**特征**：
- 纯虚接口，由具体后端实现
- 描述符包含：width/height/depth、mip_levels、array_size、format、bind_flags

---

### ITexture::width()

```cpp
[[nodiscard]] uint32_t width() const noexcept { return desc().width; }
```

**描述**：纹理宽度（像素）。

**返回**：uint32_t 纹理宽度

**特征**：
- 内联访问器，等价于 desc().width
- 便捷访问常用属性

---

### ITexture::height()

```cpp
[[nodiscard]] uint32_t height() const noexcept { return desc().height; }
```

**描述**：纹理高度（像素）。

**返回**：uint32_t 纹理高度

**特征**：
- 内联访问器，等价于 desc().height
- 便捷访问常用属性

---

### ITexture::format()

```cpp
[[nodiscard]] PixelFormat format() const noexcept { return desc().format; }
```

**描述**：像素格式。

**返回**：PixelFormat 像素格式

**特征**：
- 内联访问器，等价于 desc().format
- 便捷访问常用属性

---

## 使用场景

### 1. 创建顶点缓冲

```cpp
// 顶点缓冲描述符
BufferDesc desc;
desc.size = sizeof(vertices);
desc.stride = sizeof(Vertex);
desc.bind_flags = BufferBindFlags::Vertex;

// 创建顶点缓冲（含初始数据）
auto buffer = device->create_buffer(desc, vertices);

// 查询缓冲大小
uint64_t size = buffer->size();  // 等价于 buffer->desc().size
```

---

### 2. 创建索引缓冲

```cpp
// 索引缓冲描述符
BufferDesc desc;
desc.size = sizeof(indices);
desc.stride = sizeof(uint16_t);
desc.bind_flags = BufferBindFlags::Index;

// 创建索引缓冲（含初始数据）
auto buffer = device->create_buffer(desc, indices);
```

---

### 3. 创建常量缓冲

```cpp
// 常量缓冲描述符
BufferDesc desc;
desc.size = sizeof(ConstantBuffer);
desc.stride = 0;  // 常量缓冲无需 stride
desc.bind_flags = BufferBindFlags::Constant;

// 创建常量缓冲（无初始数据）
auto buffer = device->create_buffer(desc, nullptr);
```

---

### 4. 创建纹理

```cpp
// 纹理描述符
TextureDesc desc;
desc.width = 512;
desc.height = 512;
desc.format = PixelFormat::RGBA8_UNorm;
desc.bind_flags = TextureBindFlags::ShaderResource | TextureBindFlags::RenderTarget;

// 创建纹理
auto texture = device->create_texture(desc);

// 查询纹理属性
uint32_t width = texture->width();    // 等价于 texture->desc().width
uint32_t height = texture->height();  // 等价于 texture->desc().height
PixelFormat format = texture->format(); // 等价于 texture->desc().format
```

---

### 5. 查询交换链后缓冲纹理

```cpp
// 获取交换链后缓冲纹理（裸指针，不转移所有权）
ITexture* back_buffer = swapchain->current_render_target();

// 查询纹理属性
uint32_t width = back_buffer->width();
uint32_t height = back_buffer->height();
PixelFormat format = back_buffer->format();
```

---

### 6. 更新纹理内容

```cpp
// 创建纹理
TextureDesc desc;
desc.width = 512;
desc.height = 512;
desc.format = PixelFormat::RGBA8_UNorm;
desc.bind_flags = TextureBindFlags::ShaderResource;
auto texture = device->create_texture(desc);

// 更新纹理内容（子区域）
uint32_t x = 0, y = 0;
uint32_t width = 256, height = 256;
uint32_t row_pitch = width * 4;  // RGBA8 = 4 字节/像素
device->update_texture_region(texture.get(), x, y, width, height, data, row_pitch);
```

---

## 性能分析

### IBuffer 大小

**特征**：
- IBuffer 是纯虚接口（只有虚函数表指针）
- 实际大小由具体后端实现决定（D3D11Buffer ~32 字节）

---

### ITexture 大小

**特征**：
- ITexture 是纯虚接口（只有虚函数表指针）
- 实际大小由具体后端实现决定（D3D11Texture ~64 字节）

---

### 内联访问器

**特征**：
- size()、width()、height()、format() 是内联函数
- 无虚函数调用开销（编译器内联）
- 直接访问 desc() 的字段

---

## 最佳实践

### 1. 使用内联访问器

```cpp
// ✅ 推荐：使用内联访问器
uint32_t width = texture->width();
uint32_t height = texture->height();

// ✅ 也可以：直接访问 desc()
uint32_t width = texture->desc().width;
uint32_t height = texture->desc().height;
```

---

### 2. 创建缓冲时提供初始数据

```cpp
// ✅ 推荐：创建时提供初始数据（不可变缓冲，最佳性能）
auto buffer = device->create_buffer(desc, vertices);

// ✅ 也可以：创建后更新（可变缓冲，稍慢）
auto buffer = device->create_buffer(desc, nullptr);
// 后续通过 update_buffer() 更新
```

---

### 3. 常量缓冲大小必须是 16 字节的倍数

```cpp
// ❌ 不推荐：常量缓冲大小不是 16 字节的倍数
struct ConstantBuffer {
    float value;  // 4 字节
};  // 总计 4 字节，不满足 D3D11 要求

BufferDesc desc;
desc.size = sizeof(ConstantBuffer);  // 4 字节
desc.bind_flags = BufferBindFlags::Constant;
auto buffer = device->create_buffer(desc, nullptr);  // 创建失败

// ✅ 推荐：常量缓冲大小是 16 字节的倍数
struct ConstantBuffer {
    float value;  // 4 字节
    float padding[3];  // 12 字节填充
};  // 总计 16 字节

BufferDesc desc;
desc.size = sizeof(ConstantBuffer);  // 16 字节
desc.bind_flags = BufferBindFlags::Constant;
auto buffer = device->create_buffer(desc, nullptr);  // 创建成功
```

---

### 4. 检查资源有效性

```cpp
// ✅ 推荐：检查资源有效性
auto buffer = device->create_buffer(desc, data);
if (buffer) {
    // 缓冲创建成功
}

auto texture = device->create_texture(desc);
if (texture) {
    // 纹理创建成功
}
```

---

## 常见陷阱

### 1. 交换链后缓冲纹理不转移所有权

```cpp
// ❌ 问题：尝试转移交换链后缓冲纹理所有权
ITexture* back_buffer = swapchain->current_render_target();
core::OwnedPtr<ITexture> owned = core::OwnedPtr<ITexture>(back_buffer, deleter);  // 错误：会导致 double free

// ✅ 解决：交换链后缓冲纹理由 ISwapchain 管理，不转移所有权
ITexture* back_buffer = swapchain->current_render_target();
// 直接使用裸指针，不尝试转移所有权
```

---

### 2. 常量缓冲大小不是 16 字节的倍数

```cpp
// ❌ 问题：常量缓冲大小不是 16 字节的倍数
struct ConstantBuffer {
    float value;  // 4 字节
};  // 总计 4 字节，不满足 D3D11 要求

BufferDesc desc;
desc.size = sizeof(ConstantBuffer);  // 4 字节
desc.bind_flags = BufferBindFlags::Constant;
auto buffer = device->create_buffer(desc, nullptr);  // 创建失败

// ✅ 解决：常量缓冲大小是 16 字节的倍数
struct ConstantBuffer {
    float value;  // 4 字节
    float padding[3];  // 12 字节填充
};  // 总计 16 字节

BufferDesc desc;
desc.size = sizeof(ConstantBuffer);  // 16 字节
desc.bind_flags = BufferBindFlags::Constant;
auto buffer = device->create_buffer(desc, nullptr);  // 创建成功
```

---

### 3. 忘记设置 bind_flags

```cpp
// ❌ 问题：忘记设置 bind_flags
BufferDesc desc;
desc.size = sizeof(vertices);
// 缺少 desc.bind_flags = BufferBindFlags::Vertex;

auto buffer = device->create_buffer(desc, vertices);  // 创建失败或行为未定义

// ✅ 解决：设置 bind_flags
BufferDesc desc;
desc.size = sizeof(vertices);
desc.bind_flags = BufferBindFlags::Vertex;

auto buffer = device->create_buffer(desc, vertices);  // 创建成功
```

---

### 4. 纹理格式与数据不匹配

```cpp
// ❌ 问题：纹理格式与数据不匹配
TextureDesc desc;
desc.width = 512;
desc.height = 512;
desc.format = PixelFormat::RGBA8_UNorm;  // 4 字节/像素

uint32_t row_pitch = desc.width * 3;  // 错误：3 字节/像素
device->update_texture_region(texture.get(), 0, 0, desc.width, desc.height, data, row_pitch);

// ✅ 解决：纹理格式与数据匹配
TextureDesc desc;
desc.width = 512;
desc.height = 512;
desc.format = PixelFormat::RGBA8_UNorm;  // 4 字节/像素

uint32_t row_pitch = desc.width * 4;  // 正确：4 字节/像素
device->update_texture_region(texture.get(), 0, 0, desc.width, desc.height, data, row_pitch);
```

---

## 完整示例

```cpp
#include <mine/gfx/Gfx.h>

using namespace mine::gfx;

int main() {
    // 创建设备
    auto device = create_device(Backend::D3D11);
    
    // 创建顶点缓冲
    struct Vertex {
        float x, y;
        float r, g, b, a;
    };
    Vertex vertices[] = {
        {0.0f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f},
        {0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f},
        {-0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f},
    };
    
    BufferDesc vb_desc;
    vb_desc.size = sizeof(vertices);
    vb_desc.stride = sizeof(Vertex);
    vb_desc.bind_flags = BufferBindFlags::Vertex;
    auto vertex_buffer = device->create_buffer(vb_desc, vertices);
    
    // 查询缓冲大小
    uint64_t size = vertex_buffer->size();
    
    // 创建纹理
    TextureDesc tex_desc;
    tex_desc.width = 512;
    tex_desc.height = 512;
    tex_desc.format = PixelFormat::RGBA8_UNorm;
    tex_desc.bind_flags = TextureBindFlags::ShaderResource | TextureBindFlags::RenderTarget;
    auto texture = device->create_texture(tex_desc);
    
    // 查询纹理属性
    uint32_t width = texture->width();
    uint32_t height = texture->height();
    PixelFormat format = texture->format();
    
    // 创建常量缓冲
    struct ConstantBuffer {
        float mvp[16];  // 4x4 矩阵，64 字节
    };
    
    BufferDesc cb_desc;
    cb_desc.size = sizeof(ConstantBuffer);
    cb_desc.bind_flags = BufferBindFlags::Constant;
    auto constant_buffer = device->create_buffer(cb_desc, nullptr);
    
    return 0;
}
```

---

## 总结

`IBuffer` 和 `ITexture` 是 `mine.gfx.rhi` 模块的 GPU 资源抽象接口，具备：

1. **IBuffer**：GPU 缓冲资源（顶点/索引/常量/结构化缓冲）
2. **ITexture**：GPU 纹理资源（2D/3D/渲染目标/深度/模板）
3. **desc() 查询**：返回创建时的描述符（只读）
4. **内联访问器**：便捷访问常用属性（size/width/height/format）

**使用建议**：
- 使用内联访问器便捷访问常用属性
- 创建缓冲时提供初始数据（不可变缓冲，最佳性能）
- 常量缓冲大小必须是 16 字节的倍数
- 检查资源有效性
- 交换链后缓冲纹理不转移所有权
- 纹理格式与数据匹配
