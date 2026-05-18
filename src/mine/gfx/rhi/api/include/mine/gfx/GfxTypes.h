/**
 * @file GfxTypes.h
 * @brief RHI 基础类型、枚举与描述符结构体。
 *
 * 此头文件不依赖任何具体图形 API（D3D/Vulkan/Metal），
 * 所有后端实现均以这些类型作为公共语言。
 */

#pragma once

#include <cstdint>

namespace mine::gfx {

// ── 后端与队列 ──────────────────────────────────────────────────────────────

/// 图形后端选择
enum class Backend : int {
    D3D11,   ///< Direct3D 11（兼容后端，FL11.0+）
    D3D12,   ///< Direct3D 12（Windows 推荐，FL12.0+）
    Metal,   ///< Metal（macOS / iOS）
    Vulkan,  ///< Vulkan（跨平台）
    GLES,    ///< OpenGL ES（移动端回退）
    Auto,    ///< 运行时自动选择最优后端
};

/// 命令队列类型
enum class QueueType : int {
    Graphics,  ///< 图形渲染队列（含光栅化）
    Compute,   ///< 纯计算队列
    Copy,      ///< 数据传输队列
};

// ── 像素格式 ─────────────────────────────────────────────────────────────────

/// 纹理/交换链像素格式
enum class PixelFormat : int {
    Unknown,

    // ── 8-bit 通道格式 ──────────────────────────────────
    RGBA8_UNorm,        ///< 8-bit RGBA，线性空间
    RGBA8_UNorm_sRGB,   ///< 8-bit RGBA，sRGB 空间
    BGRA8_UNorm,        ///< 8-bit BGRA，线性空间（DXGI/Win32 首选）
    BGRA8_UNorm_sRGB,   ///< 8-bit BGRA，sRGB 空间

    // ── 浮点格式 ─────────────────────────────────────────
    R16G16B16A16_Float, ///< 16-bit RGBA 半精度浮点（HDR）
    R32G32B32A32_Float, ///< 32-bit RGBA 单精度浮点

    // ── 深度/模板格式 ────────────────────────────────────
    D24_UNorm_S8_UInt,  ///< 24-bit 深度 + 8-bit 模板
    D32_Float,          ///< 32-bit 单精度浮点深度
    D32_Float_S8_UInt,  ///< 32-bit 深度 + 8-bit 模板
};

// ── 垂直同步 ─────────────────────────────────────────────────────────────────

/// 垂直同步模式
enum class Vsync : int {
    Off,      ///< 关闭（允许撕裂，最低延迟）
    On,       ///< 开启垂直同步
    Adaptive, ///< 自适应（DXGI / G-Sync / FreeSync）
};

// ── 绑定标志 ─────────────────────────────────────────────────────────────────

/// 纹理绑定标志（可位或组合）
enum class TextureBindFlags : uint32_t {
    None            = 0,
    ShaderResource  = 1u << 0,  ///< 可作为着色器采样纹理（SRV）
    RenderTarget    = 1u << 1,  ///< 可作为渲染目标（RTV）
    DepthStencil    = 1u << 2,  ///< 可作为深度/模板缓冲（DSV）
    UnorderedAccess = 1u << 3,  ///< 可作为无序访问视图（UAV）
};

inline TextureBindFlags operator|(TextureBindFlags a, TextureBindFlags b) noexcept {
    return static_cast<TextureBindFlags>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool has_flag(TextureBindFlags flags, TextureBindFlags flag) noexcept {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

/// 缓冲绑定标志（可位或组合）
enum class BufferBindFlags : uint32_t {
    None            = 0,
    Vertex          = 1u << 0, ///< 顶点缓冲
    Index           = 1u << 1, ///< 索引缓冲
    Constant        = 1u << 2, ///< 常量缓冲（Uniform Buffer）
    ShaderResource  = 1u << 3, ///< 结构化缓冲 SRV
    UnorderedAccess = 1u << 4, ///< UAV
};

inline BufferBindFlags operator|(BufferBindFlags a, BufferBindFlags b) noexcept {
    return static_cast<BufferBindFlags>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool has_flag(BufferBindFlags flags, BufferBindFlags flag) noexcept {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

// ── 基础数据结构 ──────────────────────────────────────────────────────────────

/// 清屏颜色（线性 RGBA，各分量范围 [0, 1]）
struct Color4f {
    float r{0.0f}, g{0.0f}, b{0.0f}, a{1.0f};
};

/// 视口描述（归一化设备坐标 → 屏幕空间映射）
struct Viewport {
    float x{0.0f};
    float y{0.0f};
    float width{0.0f};
    float height{0.0f};
    float min_depth{0.0f};
    float max_depth{1.0f};
};

/// 裁剪矩形（整数像素坐标，左闭右开）
struct ScissorRect {
    int32_t left{0};
    int32_t top{0};
    int32_t right{0};
    int32_t bottom{0};
};

// ── 资源描述符 ────────────────────────────────────────────────────────────────

/// 纹理创建描述符
struct TextureDesc {
    uint32_t         width{1};
    uint32_t         height{1};
    uint32_t         depth{1};          ///< 3D 纹理层数（2D 设为 1）
    uint32_t         mip_levels{1};     ///< Mipmap 级数（0 = 自动生成完整链）
    uint32_t         array_size{1};     ///< 纹理数组大小
    PixelFormat      format{PixelFormat::RGBA8_UNorm};
    TextureBindFlags bind_flags{TextureBindFlags::ShaderResource};
};

/// 缓冲创建描述符
struct BufferDesc {
    uint64_t        size{0};    ///< 缓冲字节大小
    uint32_t        stride{0};  ///< 结构化缓冲元素步长（字节）
    BufferBindFlags bind_flags{BufferBindFlags::Vertex};
};

/// 交换链创建描述符
struct SwapchainDesc {
    void*       native_window{nullptr}; ///< 平台原生窗口句柄（Win32: HWND）
    uint32_t    width{0};
    uint32_t    height{0};
    PixelFormat format{PixelFormat::BGRA8_UNorm_sRGB}; ///< 推荐使用 sRGB 格式
    uint32_t    image_count{3};  ///< 缓冲帧数（2=双缓冲，3=三缓冲）
    Vsync       vsync{Vsync::On};
};

// ── 着色器与管线 ──────────────────────────────────────────────────────────────

/// 着色器源码/字节码语言标识
enum class ShaderLanguage : int {
    HLSL,   ///< 高级着色器语言（Direct3D 11/12）
    SPIRV,  ///< SPIR-V 中间语言（Vulkan）
    MSL,    ///< Metal 着色器语言（macOS/iOS）
};

/**
 * @brief 单个着色器描述符（含源码或预编译字节码）。
 *
 * M0 阶段：D3D11 后端接受 HLSL 源码，在 create_pipeline() 内部调用
 * D3DCompile 完成编译；后续工具链（tool.shadercc）可提供预编译字节码。
 */
struct ShaderDesc {
    const void*    data{nullptr};                  ///< 着色器源码或字节码指针
    size_t         size{0};                        ///< 数据字节长度（源码可传 0 由后端自动计算）
    ShaderLanguage language{ShaderLanguage::HLSL}; ///< 语言类型
    const char*    entry_point{"main"};            ///< 入口函数名
    bool           is_source{true};               ///< true=源码字符串，false=预编译字节码
};

/// 顶点输入元素语义
enum class VertexSemantic : int {
    Position, ///< 位置（POSITION）
    Color,    ///< 颜色（COLOR）
    TexCoord, ///< 纹理坐标（TEXCOORD）
    Normal,   ///< 法线（NORMAL）
};

/// 顶点元素数据格式
enum class VertexElementFormat : int {
    Float2,       ///< 两个 float（8 字节）
    Float3,       ///< 三个 float（12 字节）
    Float4,       ///< 四个 float（16 字节）
    UByte4_UNorm, ///< 四个无符号字节归一化（4 字节，等价 RGBA8）
};

/// 顶点输入布局元素（对应 D3D11 D3D11_INPUT_ELEMENT_DESC）
struct VertexElement {
    VertexSemantic      semantic{VertexSemantic::Position}; ///< 语义类型
    uint32_t            semantic_index{0};                  ///< 语义索引（多纹理坐标时使用）
    VertexElementFormat format{VertexElementFormat::Float2};///< 数据格式
    uint32_t            slot{0};                            ///< 顶点缓冲绑定槽位
    uint32_t            offset{0};                          ///< 元素在顶点结构内的字节偏移
};

/// 图形管线创建描述符（M0 最小集，支持 2D 纯色渲染）
struct PipelineDesc {
    ShaderDesc   vertex_shader;                  ///< 顶点着色器
    ShaderDesc   pixel_shader;                   ///< 像素着色器
    VertexElement vertex_elements[8]{};          ///< 顶点输入布局（最多 8 个元素）
    uint32_t     vertex_element_count{0};        ///< 实际使用的元素数量
    uint32_t     vertex_stride{0};               ///< 单个顶点字节大小
    bool         enable_blend{false};            ///< 是否启用 Alpha 混合（预乘 Alpha 模式）
    bool         enable_depth{false};            ///< 是否启用深度测试（2D 渲染通常关闭）
};

} // namespace mine::gfx
