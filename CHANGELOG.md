# Changelog

本项目遵循 Keep a Changelog，版本号遵循语义化版本。

## [Unreleased]

### Added
- **paint（变换系统）**：实现 Canvas 2D 变换系统（平移、缩放、旋转）：
  - `DrawCmdKind::TransformSet`：新增非压栈变换命令（`transform()`/`translate()`/`rotate()`/`scale()` 使用）
  - `DrawCmdKind::TransformPush`：语义修正为仅保存当前变换快照（`save()` 使用，不级联变换）
  - `Canvas::save()`/`restore()`：正确实现 HTML Canvas 风格的状态保存/恢复语义
  - `Canvas::translate(Vec2)`/`scale(float)`/`scale(Vec2)`/`rotate(float)` 现在实际生效
  - `RhiRenderer::render()` 新增 CPU 端变换状态追踪（`current_transform` + `transform_save_stack`）
  - 方案：CPU 端后置变换 — 对 SDF 顶点屏幕坐标应用变换，`local` 坐标保持画布空间不变
  - GPU 属性插值机制确保旋转/缩放后 SDF 仍在画布本地坐标系中正确计算，**无需修改任何 HLSL**
  - 支持无限嵌套变换栈（`save()`/`restore()` 正确恢复整组变换）
  - 所有 SDF 形状（FillRect/FillRoundedRect/FillEllipse/StrokeRect/StrokeLine/StrokeArc/StrokeQuadBezier/StrokeCubicBezier/FillPolygon 等）、文字（DrawText）均支持当前累积变换
  - 当变换为单位矩阵时零开销（`current_transform_is_identity` 快速路径跳过顶点遍历）
  - 支持 `Transform2D::rotation_about(angle, pivot)` 绕任意点旋转

- **samples（样例更新）**：
  - `00-hello-rect`：增加 2 行新演示（行16-17）：
    - 行16左：`save/translate/rotate/restore` — 旋转 30° 的圆角矩形（含未旋转轮廓参考）
    - 行16右：`save/translate/scale/restore` — 放大 1.5 倍的椭圆（含正常大小轮廓参考）
    - 行17左：嵌套变换演示 — 外层旋转15° 绿色矩形 + 内层叠加旋转30°+缩放0.6的橙色矩形
    - 行17右：`rotation_about` 演示 — 五个彩色圆角矩形绕格子中心以72°间隔均匀分布

- **paint（裁剪系统）**：新增基于 D3D11 模板缓冲（Stencil Buffer）的 SDF 嵌套裁剪系统：
  - `Canvas::clip_rect(Rect)`：压入矩形裁剪区域
  - `Canvas::clip_rounded_rect(RoundedRect)`：压入均匀圆角矩形裁剪区域
  - `Canvas::clip_complex_rounded_rect(ComplexRoundedRect)`：压入四角独立圆角矩形裁剪区域
  - `Canvas::clip_polygon(Span<const Vec2>)`：压入任意多边形裁剪区域（凸/凹均支持，最多 64 顶点）
  - `Canvas::clip_pop()`：弹出最近一次压入的裁剪区域
  - 支持嵌套裁剪（基于 IncrSat/DecrSat 模板操作，最大嵌套深度 255）
  - 新增 `k_sdf_clip_pixel_shader_hlsl`（SDF clip(-d) 裁剪像素着色器）
  - 新增 5 个裁剪管线变体（`sdf_clip_write`、`sdf_clip_erase`、`sdf_clip_test`、`text_clip_test`、`solid_clip_test`）
  - `render()` 绘制循环自动按 `clip_depth` 切换到对应 ClipTest 管线变体，对所有形状/文字命令生效

- **gfx.rhi / gfx.d3d11（接口扩展）**：
  - `StencilMode` 枚举（None / ClipWrite / ClipTest / ClipErase）及 `PipelineDesc.stencil_mode` 字段
  - `ICommandList::set_stencil_ref(uint8_t ref)` 接口（D3D11 后端已实现）
  - `D3D11Pipeline` 支持四种深度/模板状态的完整创建路径

- **samples（样例更新）**：
  - `00-hello-rect`：增加 2 行新演示（行14-15）：矩形裁剪/圆角矩形裁剪/四角独立圆角裁剪/三角形多边形裁剪

- **paint（渐变与亚克力画刷）**：新增多种画刷类型（M0 阶段）：
  - `BrushKind::LinearGradient`：线性渐变画刷，起点/终点使用归一化坐标 [0,1]，最多 4 个色标
  - `BrushKind::RadialGradient`：径向渐变画刷，支持 `inner_radius` 实现环形渐变效果
  - `BrushKind::AcrylicBrush`：亚克力画刷（捕获上一帧内容 + 高斯模糊 + 饱和度调整 + 色调叠加）
  - `GradientStop { float offset; math::Color color; }`：色标结构体
  - `LinearGradientData` / `RadialGradientData` / `AcrylicData`：各类型画刷数据结构（POD，无动态分配）
  - `Brush::linear_gradient(start, end, Span<GradientStop>)` 工厂方法
  - `Brush::radial_gradient(center, outer_radius, Span<GradientStop>)` 工厂方法
  - `Brush::radial_gradient_ring(center, inner_radius, outer_radius, Span<GradientStop>)` 工厂方法
  - `Brush::acrylic(tint_color, tint_opacity, blur_amount, saturation)` 工厂方法
  - `Brush` 类改为 union 内联存储（不超过 4 个色标，无堆分配），总大小约 104 字节

- **gfx.rhi（接口扩展）**：
  - `IDevice::copy_texture(dst, src)`：GPU-to-GPU 全量纹理拷贝接口（亚克力背景捕获用）

- **gfx.d3d11（后端实现）**：
  - `D3D11Device::copy_texture()`：使用 `ID3D11DeviceContext::CopyResource` 实现

- **paint（SDF 渲染管线扩展）**：
  - `BrushDataCB`（HLSL/C++ 双端，128 字节，b2 槽）：每 DrawCall 传递画刷类型及渐变参数
  - SDF 像素着色器新增 `sample_gradient()` 辅助函数（支持 2-4 个色标的分段线性插值）
  - SDF 像素着色器支持 `brush_kind` 分支：纯色/线性渐变/径向渐变/亚克力，适用于所有 fill 形状
  - `ViewportCB` 增加 `phys_width/phys_height` 字段（取代原 padding），供亚克力着色器将 SV_Position 转为 UV
  - 新增高斯模糊管线 `blur_pipeline_`（BlurVertex 16 字节，9-tap 分离式卷积，H/V 两趟复用同一管线）
  - 新增 `ensure_scratch_textures()` 亚克力 scratch 纹理懒创建（尺寸变化时自动重建）
  - 新增 `make_brush_cb()` 静态方法（归一化坐标 → local 像素坐标转换，生成 GPU 画刷参数缓冲）
  - `render()` 新增亚克力前处理步骤（copy_texture + H 模糊 + V 模糊，在主绘制循环前执行）
  - 所有 fill 形状命令（FillRect/FillRoundedRect/FillComplexRoundedRect/FillEllipse）现支持渐变和亚克力画刷

- **samples（样例更新）**：
  - `00-hello-rect`：增加 3 行新演示（行11-13）：线性渐变矩形/径向渐变椭圆/渐变圆角矩形/多色标径向渐变/亚克力圆角矩形/亚克力椭圆

- **text（mine.text 模块）**：新增基于 FreeType 的字体光栅化模块（M0 阶段）：
  - `FontFace`：字体面封装类（FreeType `FT_Face`），支持 `load_from_file` / `load_from_memory` 工厂方法，`set_pixel_size` / `rasterize` / `ascender` / `descender` / `line_height` API
  - `GlyphMetrics` / `GlyphBitmap`：字形度量和位图结构体（bearing / advance / pitch）
  - `FT_RENDER_MODE_NORMAL`：8-bit 灰度 Alpha 光栅化（非 SDF），与 GPU 字形图集匹配
  - FreeType 库通过 xmake 包管理器自动获取（`add_requires("freetype", {system=false})`）
  - `mine_module.lua` 扩展支持 `packages` 参数，传给 `add_packages()`

- **gfx.rhi（接口扩展）**：
  - `PixelFormat::R8_UNorm`：新增 8-bit 单通道线性格式，D3D11 映射为 `DXGI_FORMAT_R8_UNORM`
  - `ICommandList::set_shader_resource(slot, texture)`：像素着色器 SRV 绑定（同时绑定线性采样器）
  - `IDevice::update_texture_region(texture, x, y, w, h, data, row_pitch)`：部分纹理区域 CPU→GPU 上传

- **gfx.d3d11（后端实现）**：
  - `D3D11CommandList`：实现 `set_shader_resource()`，构造函数中预创建双线性线性采样器（CLAMP 模式）
  - `D3D11Device`：实现 `update_texture_region()`，使用 `D3D11_BOX` + `UpdateSubresource`

- **paint（文字渲染管线）**：
  - `DrawCmdKind::DrawText`：新绘制命令，`path_index` 复用为 `text_run` 数组下标
  - `TextRun`：内联字符串（512 字节 UTF-8）+ `font_face`（`void*`）+ `size_px` + 基线原点
  - `DisplayList`：新增 `text_runs_` 成员，三参数构造，`text_runs()` / `text_run_count()` 访问器
  - `Canvas::draw_text(text, origin, font_face, size_px, brush)`：录制文字绘制命令
  - `RhiRenderer` 新增文字渲染管线 `text_pipeline_`（`TextVertex`，32 字节：pos + uv + color）和 `GlyphAtlas`（字形 GPU 图集管理器）：
    - `GlyphAtlas`：1024×1024 R8_UNorm CPU 位图 + Shelf Packer 分配策略 + 线性缓存（最多 2048 条）+ dirty flag 全量上传
    - `k_text_vertex_shader_hlsl` / `k_text_pixel_shader_hlsl`：文字专用 HLSL（R8 图集采样，预乘 Alpha 输出）
    - `DrawText` 命令处理：UTF-8 解码 → 字形光栅化写入图集 → GPU 上传 → 生成 TextVertex 四边形 → DrawCall

- **samples/00-hello-rect**：在 10 行演示网格顶部边距绘制 `draw_text()` 标题文字，展示 FreeType M0 文字渲染效果（加载 Segoe UI / Arial 系统字体）


  - `k_sdf_vertex_shader_hlsl`：SDF 顶点着色器，将屏幕坐标变换到 NDC 并透传形状参数
  - `k_sdf_pixel_shader_hlsl`：SDF 像素着色器，包含 `box_sdf`（矩形）/ `rounded_box_sdf`（均匀圆角矩形）/ `complex_rounded_box_sdf`（四角独立圆角矩形，基于 IQ sdRoundedBox）/ `ellipse_sdf`（IQ 近似椭圆 SDF）四种 SDF 函数，使用 `fwidth()` 实现亚像素级抗锯齿；填充和描边（中心对齐）统一在一个着色器中处理
  - `SdfVertex`（64 字节）：新顶点格式，包含屏幕坐标、颜色、本地坐标、形状参数（kind / half_w / half_h / p0-p3 圆角半径 / stroke_w）
  - `create_sdf_pipeline()`：新 SDF 管线（5 个顶点元素），与保留的 `solid_pipeline_`（折线描边）并行存在
  - `push_sdf_quad_vertices()`：为形状包围盒（含 padding）生成 6 个 SdfVertex（两个三角形），本地坐标以形状中心为原点
  - `render()` 完全重写：改为逐命令 DrawCall 架构，每个 `DrawCmd` 独立创建顶点缓冲并提交；视口常量缓冲全帧共享
  - 支持命令：`FillRect` / `StrokeRect` / `FillRoundedRect` / `StrokeRoundedRect` / `FillComplexRoundedRect` / `StrokeComplexRoundedRect` / `FillEllipse` / `StrokeEllipse`（均 SDF）；`StrokeLine` 保留折线展开方案（`solid_pipeline_`）
  - 移除：旧 PathBuilder 依赖（`flatten_cubic_bezier` / `flatten_path_to_polygon` / `push_convex_polygon_vertices` / `push_rect_vertices`）
  - 支持命令：`FillRect` / `StrokeRect` / `FillRoundedRect` / `StrokeRoundedRect` / `FillComplexRoundedRect` / `StrokeComplexRoundedRect` / `FillEllipse` / `StrokeEllipse`（均 SDF）
- **paint（StrokeBorderedRect）**：新增四边各自独立宽度的矩形内侧描边命令（CSS border 语义，SDF kind=4）：
  - `BorderWidths` 结构体：top/right/bottom/left 四个 float（内侧描边宽度，逻辑像素），含 `all(w)` 和 `axes(v, h)` 工厂方法
  - `DrawCmdKind::StrokeBorderedRect`：新绘制命令，使用 `DrawCmd::border_widths` 字段
  - `Canvas::stroke_bordered_rect(rect, brush, widths)`：录制对应命令
  - SDF 着色器 kind=4：四边各自独立计算到边缘的距离遮罩（smoothstep AA），取联集（max），乘以外矩形轮廓 alpha，零宽边遮罩恒为 0 避免伪线
- **paint（StrokeBorderedRoundedRect）**：新增四边独立宽度 + 四角独立圆角的矩形内侧描边命令（SDF kind=5）：
  - `DrawCmdKind::StrokeBorderedRoundedRect`：新绘制命令
  - `DrawCmd::border_radii`（`math::CornerRadii`）：四角独立圆角半径字段
  - `Canvas::stroke_bordered_rounded_rect(rect, brush, widths, radii)`：录制对应命令
  - `SdfVertex` 扩展至 80 字节：新增 TEXCOORD3（e0-e3，存放四角圆角半径）
  - SDF 管线 `create_sdf_pipeline()` 扩展至 6 个顶点元素（element_count=6，stride=80）
  - SDF 着色器 kind=5：外轮廓用 `complex_rounded_box_sdf`，各边独立遮罩（同 kind=4），联集后与外轮廓 alpha 相乘实现圆角自然裁剪
  - `push_sdf_quad_vertices` 添加 e0-e3 参数（默认 0），已有调用无需修改
- **samples/00-hello-rect（2×5 网格演示）**：第5行更新为 StrokeBorderedRoundedRect 效果演示：
  - 行5 左（红色）：四边不等宽（上4/右2/下16/左8）+ 四角独立圆角（左上30/右上10/右下20/左下直角）
  - 行5 右（青色）：均匀圆角（16px）+ 仅上下各12px 边框（左右宽度为 0）


### Fixed
- **gfx.d3d11**：修复 D3D11 交换链格式错误导致程序启动即崩溃（退出码 1）的问题：
  - `swapchain_typeless_format`：DXGI FLIP_DISCARD 交换链不接受 TYPELESS 格式，改为返回 BGRA8_UNORM/RGBA8_UNORM
  - `swapchain_rtv_format`：为保证 D3D11 标准兼容性（完全类型资源不可使用不同格式视图），RTV 格式与交换链格式统一为 UNORM，不再强制 sRGB
- **samples/00-blank-window**：错误报告改用 `MessageBoxA`（GUI 子系统无控制台，`fprintf(stderr,...)` 无效）；补充命令队列/命令列表空指针检查；修复 WIN32_LEAN_AND_MEAN/NOMINMAX 宏重定义警告

### Added
- 初始化顶层 xmake 构建入口、构建规则与基础工程配置
- 初始化 M0 基础模块、样例、工具、测试与脚本目录骨架
- **core**：实现 `mine.core` 基础模块，包含以下全套基础类型（无 RTTI、无 STL 容器头、无 C++ 异常）：
  - `Assert.h`：`MINE_ASSERT`/`MINE_CHECK`/`MINE_UNREACHABLE`/`MINE_ASSUME` 断言宏
  - `Status.h`：轻量操作状态 `Status` 与 `StatusCode` 枚举，含 `MINE_TRY` 宏
  - `Result.h`：无异常 `Result<T,E>` 判别联合，支持 `OkTag`/`ErrTag` 工厂
  - `StringView.h`：非拥有 UTF-8 字符串视图，含 `substr`/`find`/`starts_with`/`ends_with`
  - `Span.h`：非拥有连续内存视图 `Span<T>`，支持 C 数组/指针/容器隐式推导
  - `TypeId.h`：无 RTTI 编译期类型标识符，O(1) 比较，cv/引用透明
  - `Allocator.h`：`IAllocator` 接口与 `default_allocator()`/`set_default_allocator()` 全局入口
  - `Memory.h`：`OwnedPtr<T>`（跨 DLL 安全）、`make_owned`、`MINE_NEW`/`MINE_DELETE` 宏
  - `Pimpl.h`：PIMPL 惯用法辅助 `Pimpl<Impl>`/`make_pimpl`
  - `Variant.h`：无 RTTI 类型擦除值容器，16 字节 SBO，支持 `has<T>`/`get<T>`/`emplace<T>`/`any_cast`
  - `Core.h`：伞形头文件，一次 include 引入全部 core 子头
  - `DefaultAllocator.cpp`：`MallocAllocator`（Windows/Linux 对齐分配）、`assertion_failed` 实现
  - `CoreTest.cpp`：61 个测试用例，142 个断言，全部通过
- **containers**：实现 `mine.containers` 高性能容器模块（无 RTTI、无 STL 容器头、使用 IAllocator）：
  - `Hash.h`：`Hash<T>`/`Equal<T>` 模板，含整数/指针/float/double/StringView 特化，FNV-1a 哈希
  - `Vector.h`：`Vector<T>` 动态数组，2 倍增长策略，平凡类型 memcpy/memmove 优化
  - `SmallVector.h`：`SmallVector<T,N>` 内联缓冲区优化数组，超出 N 自动切换堆分配
  - `HashMap.h`：`HashMap<K,V>` Robin Hood 开放寻址哈希表，负载因子 0.75
  - `InlineString.h`：`InlineString` 带 SSO（23 字节内联）的拥有式 UTF-8 字符串
  - `IntrusiveList.h`：`IntrusiveList<T>`/`IntrusiveListNode<T>` 侵入式双向链表
  - `Containers.h`：伞形头文件，含 `Hash<InlineString>` 特化
  - `ContainersTest.cpp`：86 个测试用例，418 断言，全部通过（doctest）
- **math**：实现 `mine.math` 数学基础模块，覆盖二维/三维向量、矩阵、几何、颜色与二维变换：
  - `Common.h`：浮点容差、`clamp`/`clamp01`/`lerp`、角度弧度转换
  - `Vec2.h` / `Vec3.h` / `Vec4.h`：浮点向量，支持算术、点积、叉积、归一化
  - `Mat3.h` / `Mat4.h`：行主序矩阵，支持平移/缩放/旋转、矩阵乘法与点/向量变换
  - `Point.h` / `Size.h` / `Rect.h` / `RoundedRect.h`：二维几何基础类型与包含/相交/并集等操作
  - `Color.h`：线性空间 RGBA 颜色，支持 8 位打包转换、预乘与 Alpha 混合
  - `Transform2D.h`：二维仿射变换封装，支持组合、逆变换与矩形包围盒映射
  - `Math.h`：伞形头文件
  - `MathTest.cpp`：25 个测试用例，68 个断言，全部通过（doctest）
- **math（扩展）**：新增 `CornerRadii`、`ComplexRoundedRect`、`Thickness` 三个补充几何类型：
  - `CornerRadii.h`：四角独立圆角半径，支持统一构造、加减法与均匀缩放
  - `ComplexRoundedRect.h`：带 `CornerRadii` 的圆角矩形，继承 `RoundedRect` 语义
  - `Thickness.h`：四边内边距/外边距，`deflate`/`inflate` 操作对应 CSS box-model
  - `MathTest.cpp` 扩展：新增 33 个测试用例（114 断言），全部通过
- **platform.abi**：新增 `mine.platform.abi` 平台抽象接口层（纯头文件，无实现，依赖 mine.core + mine.math）：
  - `Api.h`：`MINE_PLATFORM_ABI_API` 宏（接口层无 DLL 导出，留作占位）
  - `ModuleTag.h`：模块名常量 `kModuleName = "mine.platform.abi"`
  - `NativeHandle.h`：平台原生句柄联合体，含 `ptr`/`value` 两种访问方式
  - `WindowKind.h`：`WindowKind` 枚举（Normal/Tool/Dialog/Splash/Popup）
  - `WindowEvent.h`：扁平 `WindowEvent` 结构体 + `WindowEventKind` 枚举（9 种事件）
  - `IWindowEventSink.h`：`on_window_event(WindowEvent&)` 纯虚接口
  - `IWindowEventSource.h`：`subscribe` / `unsubscribe` 观察者注册接口
  - `ScreenInfo.h`：`ScreenInfo` 显示器信息结构体（bounds、work_area、dpi、scale、is_primary）
  - `IScreenManager.h`：多显示器查询接口（screen_count / screen_at / primary_screen / screen_for_point）
  - `IClipboard.h`：系统剪贴板接口（has_text / get_text / set_text / clear）
  - `IMEService.h`：输入法服务接口（M0 阶段占位）
  - `WindowDesc.h`：窗口创建参数结构体（title、size、position、resizable、frameless 等）
  - `IWindow.h`：窗口操作接口（show / hide / close / set_title / set_size / set_position / native_handle / events）
  - `IApplicationHost.h`：应用宿主接口（run / quit / create_window / clipboard / screens / ime）
  - `PlatformAbi.h`：伞形头文件，一次 include 引入全部平台接口
- **platform.win32**：新增 `mine.platform.win32` Win32 窗口后端实现（仅 Windows，依赖 mine.platform.abi）：
  - 开启 per-monitor DPI v2 感知（`DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2`）
  - `Win32WindowClass`：WNDCLASSEX 单例注册/注销管理
  - `Win32WindowEventSource`：IWindowEventSource 的 vector 实现，支持快照迭代防重入
  - `Win32Window`：IWindow 实现，HWND 管理、GWLP_USERDATA 关联 this，处理 WM_SIZE/WM_MOVE/WM_CLOSE/WM_DESTROY/WM_SETFOCUS/WM_KILLFOCUS/WM_ACTIVATE/WM_DPICHANGED 等核心消息
  - `Win32ScreenManager`：IScreenManager 实现，基于 `EnumDisplayMonitors` + `GetDpiForMonitor`
  - `Win32Clipboard`：IClipboard 实现，基于 `OpenClipboard`/`GetClipboardData`/`SetClipboardData`，UTF-16 与 UTF-8 自动互转
  - `Win32IMEService`：IMEService M0 存根实现（所有方法为空操作）
  - `Win32ApplicationHostImpl`：IApplicationHost 实现，GetMessage 消息循环，最后一个窗口关闭时自动 PostQuitMessage(0)
  - 工厂函数：`mine::platform::win32::create_application_host()` 返回 `OwnedPtr<IApplicationHost>`
- **samples/00-blank-window**：新增最小化 Win32 GUI 示例，演示创建窗口 → 显示 → 消息循环 → 退出全流程，编译并运行通过
- **gfx.rhi**：新增 `mine.gfx.rhi` 图形渲染接口抽象层（纯头文件，无实现，依赖 mine.core + mine.math）：
  - `GfxTypes.h`：核心枚举与值类型（Backend / QueueType / PixelFormat / Vsync / TextureBindFlags / BufferBindFlags / Color4f / Viewport / ScissorRect / TextureDesc / BufferDesc / SwapchainDesc）
  - `ITexture.h` / `IBuffer.h`：GPU 资源接口（desc 查询 + 宽/高/格式/大小内联访问器）
  - `ICommandList.h`：命令录制接口（begin/end / set_render_target / clear / set_viewport / set_scissor / draw / draw_indexed 等）
  - `IQueue.h`：命令提交接口（submit / wait_idle / type）
  - `ISwapchain.h`：交换链接口（resize / present / current_render_target / width/height/format/image_count/vsync）
  - `IPipeline.h`：管线状态对象接口（M0 占位，type 查询）
  - `IFence.h`：栅栏/同步接口（signal / wait / completed_value）
  - `IDevice.h`：设备接口（create_queue / create_swapchain / create_buffer / create_texture / create_command_list / create_fence / backend / adapter_name）
  - `GfxHost.h`：工厂函数声明 `mine::gfx::create_device(Backend)`（实现由链接的后端库提供）
  - `Gfx.h`：伞形头文件，一次 include 引入全部 RHI 接口
- **gfx.d3d11**：新增 `mine.gfx.d3d11` Direct3D 11 图形后端实现（仅 Windows，依赖 mine.gfx.rhi）：
  - `D3D11Helpers.h`：内部辅助头（HRESULT 检查宏、RHI↔DXGI 格式转换、交换链 TYPELESS/RTV 格式工具）
  - `D3D11Texture`：ITexture 实现，支持交换链后缓冲专用构造（传入已有 ID3D11Texture2D+RTV）与通用构造（按 bind_flags 自动创建 SRV/RTV/DSV）
  - `D3D11Buffer`：IBuffer 实现，按 bind_flags 创建 D3D11 缓冲（VB/IB/CB/SRV/UAV），常量缓冲自动 16 字节对齐
  - `D3D11CommandList`：ICommandList 实现，基于 D3D11 延迟上下文（每实例独占），begin→ClearState，end→FinishCommandList，M0 绘制方法已接线 Draw*/DrawIndexed*
  - `D3D11Queue`：IQueue 实现，封装立即上下文，submit→ExecuteCommandList，wait_idle→Flush
  - `D3D11Swapchain`：ISwapchain 实现，DXGI FLIP_DISCARD 模型，TYPELESS 基础格式+sRGB RTV，支持 DXGI_FEATURE_PRESENT_ALLOW_TEARING，resize 正确释放后缓冲再重建
  - `D3D11Fence`：IFence 实现，基于 D3D11_QUERY_EVENT 轮询等待，支持超时
  - `D3D11DeviceImpl`：IDevice 实现，D3D11CreateDevice（Feature Level 11.0/11.1，硬件不可用时回退 WARP），查询 IDXGIFactory2 用于交换链创建，适配器名称 UTF-8 输出
  - `D3D11Backend.cpp`：实现 `mine::gfx::create_device()` 工厂函数（链接时替换模式）
- **samples/00-blank-window（更新）**：集成 D3D11 RHI，演示"蓝色清屏"验收场景：
  - 创建 D3D11 设备 + 交换链 + Graphics 队列 + 命令列表
  - 首帧清屏为纯蓝色（R=0, G=0.4, B=1, A=1）并呈现
  - 订阅 Resized 事件自动 resize 交换链并重新渲染，窗口大小变化后蓝色持续保持
- **paint**：新增 `mine.paint` 2D 绘制模块骨架（M0.3 任务 #13）：
  - `PaintFwd.h`：前向声明伞形头（Brush / Pen / Path / PathBuilder / DrawCmd / DisplayList / Canvas / IRenderer）
  - `Brush.h`：画刷类型，支持 `SolidColor`（M0 实现）、`LinearGradient`/`RadialGradient`/`ImageBrush`（占位），静态工厂 `solid()` / `solid_rgb()` / `solid_rgba()`
  - `Pen.h`：描边样式结构体（width / LineJoin / LineCap / miter_limit）
  - `Path.h`：不可变路径，持有 `PathCmd`（MoveTo / LineTo / CubicTo / QuadTo / Close）序列
  - `PathBuilder.h` + `PathBuilder.cpp`：流式路径构造器，支持 `add_rect()` / `add_rounded_rect()` / `add_ellipse()`（贝塞尔圆角近似，κ ≈ 0.5523）
  - `DrawCmd.h`：扁平绘制命令结构体（13 种命令：FillRect~ClipPop~TransformPush 等）
  - `DisplayList.h`：不可变绘制命令序列，持有 DrawCmd + Path 数组，可移动
  - `Canvas.h` + `Canvas.cpp`：录制模式绘制上下文，fill_* / stroke_* / clip_rect / translate / scale / rotate / save / restore / end()
  - `IRenderer.h`：2D 渲染后端抽象接口（begin_frame / end_frame / render），附工厂函数 `create_renderer()`
  - `Paint.h`：模块伞形头文件，一次 include 引入全部子头
- **core（修复）**：`Result.h` 补充 `#include <memory>`，修复 MSVC 2026 中 `std::construct_at`/`std::destroy_at` 不再通过 `<new>` 间接引入的编译错误
- **gfx.rhi（扩展）**：扩展 RHI 抽象层以支持着色器图形管线（M0.3 任务 #14）：
  - `GfxTypes.h`：新增 `ShaderLanguage`/`ShaderDesc`/`VertexSemantic`/`VertexElementFormat`/`VertexElement`/`PipelineDesc` 等着色器与管线描述类型
  - `IDevice.h`：新增 `create_pipeline(const PipelineDesc&)` 纯虚接口；`create_buffer()` 添加 `initial_data` 参数支持含初始数据的不可变缓冲
  - `ICommandList.h`：新增 `set_constant_buffer(uint32_t slot, IBuffer*)` 接口，同时绑定 VS/PS 的相同槽位
- **gfx.d3d11（扩展）**：实现 RHI 着色器管线扩展（M0.3 任务 #14）：
  - `D3D11Pipeline`：新建类，使用 D3DCompile 运行时编译 HLSL（Debug 含调试信息，Release 启用 O2），创建 InputLayout / BlendState（预乘 Alpha）/ RasterizerState（CULL_NONE, FILL_SOLID）/ DepthStencilState（2D 关闭深度测试）
  - `D3D11Device`：实现 `create_pipeline()` 接口，并更新 `create_buffer()` 以透传初始数据
  - `D3D11CommandList`：完整实现 `set_pipeline()`（绑定 VS/PS/InputLayout/BlendState 等），新增 `set_constant_buffer()` 实现
  - `xmake.lua`：添加 `d3dcompiler` 系统链接库
- **paint（RHI 渲染器）**：实现 `mine.paint.IRenderer` 基于 RHI 的真实渲染后端（M0.3 任务 #14）：
  - `RhiRenderer.cpp`：内嵌 HLSL 着色器源码（VS 做像素→NDC 变换，Y 轴翻转；PS 直接输出插值颜色）
  - 顶点格式：`PaintVertex{x, y, r, g, b, a}` = 24 字节；常量缓冲：`ViewportCB{width, height, pad, pad}` = 16 字节（D3D11 对齐要求）
  - `render(dl, target)`：将 FillRect 命令转换为三角形顶点（每矩形 6 顶点），通过 `create_buffer(desc, initial_data)` 创建不可变顶点缓冲，录制绘制命令后提交
  - 工厂函数 `mine::paint::create_renderer(IDevice*)` 在 `namespace mine::paint` 中实现
  - 架构决策：paint 模块直接依赖 `mine.gfx.rhi`，不依赖任何具体图形 API，无 mine.paint.d3d11 独立后端
- **samples/00-hello-rect（完成）**：实现 M0.3 端到端样例（M0.3 任务 #14），演示 Canvas → Paint 渲染器 → 交换链完整链路：
  - 创建 D3D11 设备 + `mine::paint::create_renderer()` 2D 渲染器 + 交换链
  - 每帧：Canvas 录制深灰色背景矩形 + 居中红色矩形 → `begin_frame` / `render` / `end_frame` → `present`
  - 订阅 Resized 事件自动更新交换链尺寸并重新渲染