# 00 — 总体概览

## 0.1 项目愿景

MineUI 是一套面向桌面与移动端的现代声明式 UI 框架，使用 C++20 实现，通过自研标记语言 **MML** 描述界面与数据契约，并在构建期将其编译为 C++ 源码，最终产出**纯静态/动态 SDK**。

定位关键词：

- **AOT**：界面、样式、绑定都在编译期落地为 C++，运行期不需要解析器。
- **MVVM**：以依赖属性 + INotify 风格的属性系统驱动视图与视图模型解耦。
- **多后端**：Windows 默认 D3D12（回退 D3D11），macOS Metal，Linux Vulkan，Android Vulkan/GLES，iOS Metal。
- **多窗口**：天然支持多窗口、多显示器、多 DPI、多线程渲染。
- **可裁剪**：按模块按函数级裁剪，最小可执行体积可下探到单二进制 < 2 MB（仅核心 + 简单窗口）。

## 0.2 目标用户

| 用户角色 | 期望 |
|----------|------|
| 应用开发者 | 写 MML + ViewModel，零底层代码即可完成产品 |
| 控件作者 | 继承基类、用 C++ 实现自定义控件，可直接在 MML 中使用 |
| 引擎层贡献者 | 替换/新增 RHI 后端、合成器、文本引擎 |
| 工具链开发者 | 借助稳定的 AST/Sema API 构建编辑器、设计器、皮肤工具 |

## 0.3 设计原则

1. **零成本抽象优先**：抽象不应在 release 中产生额外间接调用与堆分配。
2. **数据驱动**：依赖属性、绑定、样式、动画统一通过同一套属性系统驱动。
3. **静态优先 + 受控的动态**：以静态布局/类型为主；运行期反射限定在“热重载/设计器/序列化”场景，可整模块裁剪。
4. **错误显式化**：使用 `mine::Result<T,E>` 返回错误，内部禁用异常（除 OOM 或 PAL 边界），异常默认 `-fno-exceptions`。
5. **无隐藏全局状态**：所有运行时"全局"对象都是 IoC 容器中的服务，可被 mock。例外：`StyleRegistry`、`TemplateRegistry` 是编译期生成代码（`.g.cpp`）的**静态只写注册入口**，其唯一职责是在进程启动阶段将 mmlc 产生的元数据写入，不对外暴露可变状态，不参与业务逻辑，因此不适合纳入 IoC 容器；其余可被替换/mock 的运行时服务均须通过 IoC 注入。
6. **接口稳定 / 实现自由**：稳定的 `mine::` 头文件版本号化；实现细节随时重写。

## 0.4 技术选型

| 子系统 | 选型 | 备注 |
|--------|------|------|
| 语言/标准 | C++20，部分 C++23 | 模块化（header-only 接口 + .cpp 实现） |
| 构建 | xmake | 内置包管理 + 多目标 + 自带 LTO 配置 |
| 单元测试 | doctest | 头文件单依赖、可裁剪 |
| 基准测试 | nanobench | 体积小 |
| 文本整形 | HarfBuzz（封装在 `mine.text.shape`） | 通过适配器暴露 |
| 字体光栅 | FreeType（默认） / DirectWrite（Win 加速） | 二选一 |
| 图像解码 | stb_image / libjpeg-turbo / libpng / libwebp | 封装在 `mine.image` |
| ICU/Unicode | utf8cpp + 自研轻量层 | 不强依赖 ICU，按需开启 |
| SQL | SQLite amalgamation 静态嵌入 | 见 12 |
| TLS | mbedTLS 或 OpenSSL（开关） | 见 13 |
| HTTP | 自研基于 mine.async + mbedTLS | 不依赖 cURL |
| 协程 | C++20 stdcoro + 自研调度器 | 见 mine.async |

## 0.5 不在范围内（v1）

- 3D 引擎能力（仅保证有 3D 内容嵌入接口）。
- 浏览器/WebView 嵌入（提供平台原生 WebView 占位接口，但不内置）。
- 富文档排版（v1 仅富文本，文档级排版后续版本）。

## 0.6 版本与 SDK

- 语义化版本 `MAJOR.MINOR.PATCH`。
- ABI 兼容窗口：`MAJOR` 内保证 ABI 兼容；C 接口层（`mine_capi_*`）拥有更长兼容窗口。
- SDK 交付参见 [16-sdk-packaging.md](16-sdk-packaging.md)。
