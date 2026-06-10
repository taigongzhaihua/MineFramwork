# IPipeline 详细接口文档

## 概述

`IPipeline` 是 `mine.gfx.rhi` 模块的**图形/计算管线状态对象接口**。

**核心特性：**
- **管线类型**：Graphics（图形管线）/ Compute（计算管线）
- **状态封装**：着色器程序、顶点输入布局、混合状态、光栅化状态、深度/模板状态
- **预编译**：创建开销较高，应在初始化阶段预创建并复用

---

## 文件位置

```
src/mine/gfx/rhi/api/include/mine/gfx/IPipeline.h
```

---

## 类定义

```cpp
enum class PipelineType : int {
    Graphics, ///< 含顶点着色器、像素着色器的图形管线
    Compute,  ///< 仅含计算着色器的计算管线
};

class IPipeline {
public:
    virtual ~IPipeline() = default;

    /// 返回管线类型
    [[nodiscard]] virtual PipelineType type() const noexcept = 0;
};
```

**描述**：预编译管线状态对象接口。

**创建**：IDevice::create_pipeline(PipelineDesc)

**特征**：
- D3D11：封装 ID3D11VertexShader、ID3D11PixelShader、ID3D11InputLayout、ID3D11BlendState、ID3D11RasterizerState、ID3D11DepthStencilState
- D3D12：封装 ID3D12PipelineState
- Vulkan：封装 VkPipeline

---

## PipelineDesc 结构体

```cpp
struct PipelineDesc {
    ShaderDesc   vertex_shader;                  ///< 顶点着色器
    ShaderDesc   pixel_shader;                   ///< 像素着色器
    VertexElement vertex_elements[8]{};          ///< 顶点输入布局（最多 8 个元素）
    uint32_t     vertex_element_count{0};        ///< 实际使用的元素数量
    uint32_t     vertex_stride{0};               ///< 单个顶点字节大小
    bool         enable_blend{false};            ///< 是否启用 Alpha 混合（预乘 Alpha 模式）
    bool         enable_depth{false};            ///< 是否启用深度测试（2D 渲染通常关闭）
    StencilMode  stencil_mode{StencilMode::None};///< 模板模式（None/ClipWrite/ClipPop/ClipTest）
};
```

**描述**：图形管线创建描述符。

**字段说明**：
- **vertex_shader**：顶点着色器描述符（ShaderDesc）
  - `data`：HLSL 源码或预编译字节码
  - `size`：数据字节大小
  - `language`：ShaderLanguage（HLSL/SPIRV/MSL）
  - `entry_point`：入口点函数名（默认 "main"）
  - `is_source`：是否为源码（true = 源码，false = 字节码）
- **pixel_shader**：像素着色器描述符（同上）
- **vertex_elements**：顶点输入布局数组（最多 8 个元素）
  - `semantic`：VertexSemantic（Position/Color/TexCoord/Normal）
  - `semantic_index`：语义索引（对应 HLSL POSITION0/POSITION1）
  - `format`：VertexElementFormat（Float2/Float3/Float4/UByte4_UNorm）
  - `slot`：输入槽位（0-7）
  - `offset`：缓冲内字节偏移
- **vertex_element_count**：实际使用的元素数量
- **vertex_stride**：单个顶点字节大小
- **enable_blend**：是否启用 Alpha 混合（预乘 Alpha 模式：SrcBlend=One, DestBlend=InvSrcAlpha）
- **enable_depth**：是否启用深度测试（2D 渲染通常关闭）
- **stencil_mode**：模板模式
  - `None`：模板完全禁用
  - `ClipWrite`：压入裁剪层（Equal 测试，IncrSat 写入）
  - `ClipPop`：弹出裁剪层（Equal 测试，DecrSat 写入）
  - `ClipTest`：正常绘制 + 裁剪测试（Greater 测试）

---

## 成员方法

### type()

```cpp
[[nodiscard]] virtual PipelineType type() const noexcept = 0;
```

**描述**：返回管线类型。

**返回**：PipelineType（Graphics / Compute）

---

## 使用场景

### 1. 创建纯色管线（2D）

```cpp
// 顶点着色器 HLSL
const char* vs_basic = R"(
    cbuffer ViewportCB : register(b0) {
        float4x4 proj;
    };
    struct VSInput {
        float2 pos : POSITION;
        float4 color : COLOR;
    };
    struct PSInput {
        float4 pos : SV_Position;
        float4 color : COLOR;
    };
    PSInput main(VSInput input) {
        PSInput output;
        output.pos = mul(proj, float4(input.pos, 0, 1));
        output.color = input.color;
        return output;
    }
)";

// 像素着色器 HLSL
const char* ps_basic = R"(
    struct PSInput {
        float4 pos : SV_Position;
        float4 color : COLOR;
    };
    float4 main(PSInput input) : SV_Target {
        return input.color;
    }
)";

// 管线描述符
PipelineDesc desc{};

desc.vertex_shader.data = vs_basic;
desc.vertex_shader.size = strlen(vs_basic);
desc.vertex_shader.language = ShaderLanguage::HLSL;
desc.vertex_shader.entry_point = "main";
desc.vertex_shader.is_source = true;  // 源码模式

desc.pixel_shader.data = ps_basic;
desc.pixel_shader.size = strlen(ps_basic);
desc.pixel_shader.language = ShaderLanguage::HLSL;
desc.pixel_shader.entry_point = "main";
desc.pixel_shader.is_source = true;

desc.vertex_element_count = 2;
desc.vertex_elements[0].semantic = VertexSemantic::Position;
desc.vertex_elements[0].semantic_index = 0;
desc.vertex_elements[0].format = VertexElementFormat::Float2;
desc.vertex_elements[0].slot = 0;
desc.vertex_elements[0].offset = 0;

desc.vertex_elements[1].semantic = VertexSemantic::Color;
desc.vertex_elements[1].semantic_index = 0;
desc.vertex_elements[1].format = VertexElementFormat::Float4;
desc.vertex_elements[1].slot = 0;
desc.vertex_elements[1].offset = sizeof(float) * 2;

desc.vertex_stride = sizeof(float) * 6;  // 2 (pos) + 4 (color)
desc.enable_blend = true;   // 启用 Alpha 混合
desc.enable_depth = false;  // 2D 不使用深度测试
desc.stencil_mode = StencilMode::None;

// 创建管线
auto pipeline = device->create_pipeline(desc);
```

---

### 2. 创建纹理采样管线

```cpp
// 顶点着色器 HLSL
const char* vs_texture = R"(
    cbuffer ViewportCB : register(b0) {
        float4x4 proj;
    };
    struct VSInput {
        float2 pos : POSITION;
        float2 uv : TEXCOORD0;
    };
    struct PSInput {
        float4 pos : SV_Position;
        float2 uv : TEXCOORD0;
    };
    PSInput main(VSInput input) {
        PSInput output;
        output.pos = mul(proj, float4(input.pos, 0, 1));
        output.uv = input.uv;
        return output;
    }
)";

// 像素着色器 HLSL
const char* ps_texture = R"(
    Texture2D tex : register(t0);
    SamplerState samp : register(s0);
    struct PSInput {
        float4 pos : SV_Position;
        float2 uv : TEXCOORD0;
    };
    float4 main(PSInput input) : SV_Target {
        return tex.Sample(samp, input.uv);
    }
)";

PipelineDesc desc{};
// ... (同上设置 vertex_shader/pixel_shader)

desc.vertex_element_count = 2;
desc.vertex_elements[0].semantic = VertexSemantic::Position;
desc.vertex_elements[0].format = VertexElementFormat::Float2;
desc.vertex_elements[0].offset = 0;

desc.vertex_elements[1].semantic = VertexSemantic::TexCoord;
desc.vertex_elements[1].format = VertexElementFormat::Float2;
desc.vertex_elements[1].offset = sizeof(float) * 2;

desc.vertex_stride = sizeof(float) * 4;
desc.enable_blend = true;
desc.enable_depth = false;

auto pipeline = device->create_pipeline(desc);
```

---

### 3. 创建裁剪管线（ClipWrite）

```cpp
PipelineDesc desc{};
// ... (设置 vertex_shader/pixel_shader)

desc.vertex_element_count = 2;
desc.vertex_elements[0].semantic = VertexSemantic::Position;
desc.vertex_elements[0].format = VertexElementFormat::Float2;
desc.vertex_elements[0].offset = 0;

desc.vertex_elements[1].semantic = VertexSemantic::Color;
desc.vertex_elements[1].format = VertexElementFormat::Float4;
desc.vertex_elements[1].offset = sizeof(float) * 2;

desc.vertex_stride = sizeof(float) * 6;
desc.enable_blend = true;
desc.enable_depth = false;
desc.stencil_mode = StencilMode::ClipWrite;  // 压入裁剪层

auto clip_push_pipeline = device->create_pipeline(desc);
```

---

### 4. 使用预编译字节码

```cpp
// 预编译字节码（外部编译工具生成）
extern const uint8_t g_vs_bytecode[];
extern const uint32_t g_vs_bytecode_size;
extern const uint8_t g_ps_bytecode[];
extern const uint32_t g_ps_bytecode_size;

PipelineDesc desc{};

desc.vertex_shader.data = g_vs_bytecode;
desc.vertex_shader.size = g_vs_bytecode_size;
desc.vertex_shader.language = ShaderLanguage::HLSL;
desc.vertex_shader.is_source = false;  // 字节码模式

desc.pixel_shader.data = g_ps_bytecode;
desc.pixel_shader.size = g_ps_bytecode_size;
desc.pixel_shader.language = ShaderLanguage::HLSL;
desc.pixel_shader.is_source = false;

// ... (其他设置同上)

auto pipeline = device->create_pipeline(desc);
```

---

## 性能分析

### 管线创建开销

**特征**：
- D3D11：编译 HLSL + 创建状态对象（~10-50 ms）
- 源码模式比字节码模式慢（运行时编译）
- 应在初始化阶段预创建所有管线

---

### 管线切换开销

**特征**：
- ICommandList::set_pipeline() 绑定所有状态对象（~0.01-0.1 ms）
- 频繁切换管线会降低性能
- 尽量批量绘制相同管线的对象

---

## 最佳实践

### 1. 预创建所有管线

```cpp
// ✅ 推荐：初始化阶段预创建所有管线
auto solid_pipeline = device->create_pipeline(solid_desc);
auto texture_pipeline = device->create_pipeline(texture_desc);
auto clip_push_pipeline = device->create_pipeline(clip_push_desc);

while (running) {
    // 使用预创建的管线
    cmd->set_pipeline(solid_pipeline.get());
    cmd->draw(...);
}

// ❌ 不推荐：每帧创建管线
while (running) {
    auto pipeline = device->create_pipeline(desc);  // 慢
    cmd->set_pipeline(pipeline.get());
    cmd->draw(...);
}
```

---

### 2. 使用预编译字节码

```cpp
// ✅ 推荐：使用预编译字节码（更快）
desc.vertex_shader.data = g_vs_bytecode;
desc.vertex_shader.size = g_vs_bytecode_size;
desc.vertex_shader.is_source = false;

// ❌ 不推荐：运行时编译源码（慢）
desc.vertex_shader.data = vs_source;
desc.vertex_shader.size = strlen(vs_source);
desc.vertex_shader.is_source = true;
```

---

### 3. 批量绘制相同管线的对象

```cpp
// ✅ 推荐：批量绘制相同管线的对象
cmd->set_pipeline(pipeline.get());
cmd->draw(obj1);
cmd->draw(obj2);
cmd->draw(obj3);

// ❌ 不推荐：频繁切换管线
cmd->set_pipeline(pipeline1.get());
cmd->draw(obj1);
cmd->set_pipeline(pipeline2.get());
cmd->draw(obj2);
cmd->set_pipeline(pipeline3.get());
cmd->draw(obj3);
```

---

### 4. 检查管线创建成功

```cpp
// ✅ 推荐：检查管线创建成功
auto pipeline = device->create_pipeline(desc);
if (!pipeline) {
    // 管线创建失败（HLSL 编译错误、输入布局不匹配等）
    return false;
}
```

---

## 常见陷阱

### 1. 顶点输入布局与着色器不匹配

```cpp
// ❌ 问题：顶点输入布局与着色器不匹配
// 顶点着色器需要 POSITION + COLOR
struct VSInput {
    float2 pos : POSITION;
    float4 color : COLOR;
};

// 顶点输入布局只定义了 POSITION
desc.vertex_element_count = 1;
desc.vertex_elements[0].semantic = VertexSemantic::Position;
desc.vertex_elements[0].format = VertexElementFormat::Float2;

auto pipeline = device->create_pipeline(desc);  // 创建失败

// ✅ 解决：顶点输入布局与着色器匹配
desc.vertex_element_count = 2;
desc.vertex_elements[0].semantic = VertexSemantic::Position;
desc.vertex_elements[0].format = VertexElementFormat::Float2;
desc.vertex_elements[1].semantic = VertexSemantic::Color;
desc.vertex_elements[1].format = VertexElementFormat::Float4;

auto pipeline = device->create_pipeline(desc);  // 创建成功
```

---

### 2. 顶点步长设置错误

```cpp
// ❌ 问题：顶点步长设置错误
desc.vertex_stride = sizeof(float) * 4;  // 错误：实际顶点大小是 6 floats

// ✅ 解决：正确设置顶点步长
desc.vertex_stride = sizeof(float) * 6;  // POSITION(2) + COLOR(4)
```

---

### 3. HLSL 编译错误未检查

```cpp
// ❌ 问题：HLSL 编译错误未检查
auto pipeline = device->create_pipeline(desc);
// 未检查 pipeline 是否为 nullptr

cmd->set_pipeline(pipeline.get());  // 崩溃

// ✅ 解决：检查管线创建成功
auto pipeline = device->create_pipeline(desc);
if (!pipeline) {
    // HLSL 编译错误（D3D11 会输出调试信息）
    return false;
}
```

---

### 4. 忘记设置 vertex_element_count

```cpp
// ❌ 问题：忘记设置 vertex_element_count
desc.vertex_elements[0].semantic = VertexSemantic::Position;
desc.vertex_elements[1].semantic = VertexSemantic::Color;
// 缺少 desc.vertex_element_count = 2;

auto pipeline = device->create_pipeline(desc);  // 创建失败或行为未定义

// ✅ 解决：设置 vertex_element_count
desc.vertex_element_count = 2;
desc.vertex_elements[0].semantic = VertexSemantic::Position;
desc.vertex_elements[1].semantic = VertexSemantic::Color;
```

---

## 完整示例

见 [03-ICommandList.md 完整示例](03-ICommandList.md#完整示例)

---

## 总结

`IPipeline` 是 `mine.gfx.rhi` 模块的图形/计算管线状态对象接口，具备：

1. **管线类型**：Graphics（图形管线）/ Compute（计算管线）
2. **状态封装**：着色器程序、顶点输入布局、混合状态、光栅化状态、深度/模板状态
3. **预编译**：创建开销较高，应在初始化阶段预创建并复用

**使用建议**：
- 预创建所有管线
- 使用预编译字节码
- 批量绘制相同管线的对象
- 检查管线创建成功
- 顶点输入布局与着色器匹配
- 正确设置顶点步长和元素数量
