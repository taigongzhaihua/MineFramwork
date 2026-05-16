# 14 — 工具链

## 14.1 总览

| 工具 | 名称 | 形态 | 主要依赖 |
|------|------|------|----------|
| 预编译器 | `mmlc` | exe | `mml.lex/parse/sema/codegen` |
| 格式化器 | `mmlfmt` | exe | `mml.parse`（保留 trivia） |
| 语言服务 | `mmlls` | exe（LSP） | `mml.*` 全套 |
| 着色器跨编译 | `shadercc` | exe（构建期） | DXC，仅在构建图中执行，不进运行时 |
| VSCode 扩展 | `mineui-vscode` | npm/vsix | TS 套壳，调用 `mmlls` |
| 安装程序 | `mineinst` | exe | 用 MineUI 自举 |
| 打包工具 | `minepack` | exe | `core / io / data` |
| 脚本库 | `scripts/` | 模板 | xmake、ps1、bash |

## 14.2 LSP（mmlls）

完全用 C++ 实现，复用 `mml.*` 库；TypeScript 端不实现任何语言逻辑。

支持能力：

* 语法高亮（semantic tokens）
* 符号跳转（定义、声明、类型）
* 悬停提示（含 doc comment、推断类型、属性默认值）
* 引用查找 + 引用统计（`textDocument/references` 扩展返回 count）
* 自动完成（属性、组件、资源、绑定路径、ViewModel 属性）
* 重命名
* 代码格式化（保留注释、对齐属性）
* 诊断（错误/警告，复用编译器同一份）
* 代码动作（fix-it、自动 import）
* 代码折叠 / 大纲 / 文档符号
* 内联提示（inlay hints：推断类型、参数名）
* 调用层级 / 类型层级
* 工作区符号 + 多文件项目模型
* 增量分析（基于 `mml.parse` 的 incremental parser）

进程模型：

* 单实例 `mmlls`，跨工作区复用。
* 与 `mmlc` 共享磁盘缓存与依赖图。

## 14.3 VSCode 扩展（套壳）

仓库：`tools/ext.vscode/`

```
src/
  extension.ts         # 入口：启动/重启 mmlls，转发命令
  client.ts            # vscode-languageclient 接线
  commands.ts          # 自定义命令（构建、生成、清理缓存）
package.json           # contributes
syntaxes/mml.tmLanguage.json  # 仅做基础高亮兜底
```

* 不在 TS 中做任何 AST/语义工作。
* 提供命令：`MineUI: Build`, `MineUI: Format`, `MineUI: Generate Cpp`, `MineUI: Restart Language Server`。
* 可视化：错误装饰、状态栏（mmlls 状态、缓存命中率）。
* 调试：可附加调试到 mmlls。

## 14.4 安装程序（mineinst）

* **用 MineUI 框架自举**（自己吃自己的狗粮，确保 UI 体验）。
* 功能：
  * 选择安装路径
  * 选择组件（Runtime / Tools / Samples / Docs / VSCode 扩展）
  * 系统注册（PATH、文件关联 `.mml`）
  * 校验、签名、完整性检查
  * 卸载入口
* 构建产出：单文件 `mineinst-<version>-<arch>.exe`（Windows）、`.pkg`（macOS）、`.AppImage` / `.deb` / `.rpm`（Linux）。

## 14.5 打包工具（minepack）

* 资源打包：把 `assets/` 编译为 `.minepack`（zip/zstd 容器，含索引）。
* SDK 打包：从 `build/install` 收集 头文件 + 静态库 + 动态库 + 工具 + 文档，产出层次：

```
mineui-sdk-<ver>-<os>-<arch>/
  bin/        mmlc, mmlls, mmlfmt, minepack, mineinst
  include/    mine/...
  lib/        静态/动态库
  cmake/      MineUIConfig.cmake     # 兼容 cmake 的项目
  xmake/      modules/mineui.lua     # xmake 集成
  share/      模板、示例资源、字体
  doc/        生成的 API 文档
  samples/    示例工程
  LICENSE
  README.md
```

## 14.6 脚本库

`scripts/` 目录：

| 文件 | 作用 |
|------|------|
| `bootstrap.ps1` / `bootstrap.sh` | 准备依赖、首次构建 |
| `build-all.lua` | xmake 统一入口 |
| `release.lua` | 触发版本号、tag、签名、上传 |
| `fmt.ps1` | 全仓 clang-format + mmlfmt |
| `lint.ps1` | clang-tidy + 自定义规则 |
| `pack-sdk.lua` | 调 minepack 打包 SDK |
| `gen-docs.lua` | 文档生成（Doxygen + mdbook） |

## 14.7 文档生成

* 头文件用 Doxygen 风格注释 → Doxygen XML → 自研脚本转 mdbook，与 `docs/` 合并发布。
* MML 标准库文档由 `mmlc --emit doc` 直接产出（保证与代码同步）。
