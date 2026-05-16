# 17 — 仓库目录布局

```
MineFramework/
├─ README.md
├─ LICENSE
├─ xmake.lua                          # 顶层入口
├─ xmake/
│  ├─ options.lua
│  ├─ toolchains.lua
│  ├─ rules/
│  │  ├─ mine_module.lua
│  │  ├─ mml_compile.lua
│  │  └─ pkg_export.lua
│  └─ packages/                       # 自定义/补丁的 xmake 包
├─ modules/
│  └─ mineui.lua                      # 第三方使用：add_requires("mineui")
│
├─ docs/                              # 设计文档（即本目录）
│  ├─ README.md
│  ├─ 00-overview.md
│  ├─ 01-architecture.md
│  ├─ ...
│  └─ diag/                           # 错误码索引（自动生成）
│
├─ src/
│  └─ mine/
│     ├─ core/
│     │  ├─ api/include/mine/core/...
│     │  ├─ src/
│     │  ├─ test/
│     │  └─ xmake.lua
│     ├─ containers/
│     ├─ diag/
│     ├─ async/
│     ├─ io/
│     ├─ text/
│     ├─ math/
│     ├─ reflect/
│     ├─ meta/
│     ├─ platform/
│     │  ├─ abi/
│     │  ├─ win32/
│     │  ├─ macos/
│     │  ├─ linux/
│     │  ├─ android/
│     │  ├─ ios/
│     │  └─ io.fs/                       # VFS 虚拟文件系统（依赖 platform.abi）
│     ├─ gfx/
│     │  ├─ rhi/
│     │  ├─ d3d11/
│     │  ├─ d3d12/
│     │  ├─ metal/
│     │  ├─ vulkan/
│     │  └─ gles/                         # shadercc 已移至 tools/shadercc/
│     ├─ paint/
│     ├─ compose/
│     ├─ image/
│     ├─ svg/
│     ├─ effects/
│     ├─ ui/
│     │  ├─ property/
│     │  ├─ binding/
│     │  ├─ event/
│     │  ├─ input/
│     │  ├─ layout/
│     │  ├─ visual/
│     │  ├─ style/
│     │  ├─ animation/
│     │  ├─ controls/
│     │  │  ├─ basic/
│     │  │  ├─ input/
│     │  │  ├─ collection/
│     │  │  ├─ nav/
│     │  │  ├─ dialog/
│     │  │  ├─ media/
│     │  │  ├─ chart/
│     │  │  └─ docking/
│     │  ├─ window/
│     │  └─ app/
│     ├─ di/
│     ├─ mvvm/
│     ├─ nav/
│     ├─ localization/
│     ├─ config/
│     ├─ logging/
│     ├─ data/
│     │  ├─ sqlite/
│     │  ├─ orm/
│     │  ├─ cache/
│     │  └─ migrate/
│     ├─ net/
│     │  ├─ socket/
│     │  ├─ tls/
│     │  ├─ dns/
│     │  ├─ http/
│     │  ├─ ws/
│     │  ├─ rest/
│     │  └─ rpc/
│     └─ mml/
│        ├─ lex/
│        ├─ parse/
│        ├─ ast/
│        ├─ sema/
│        ├─ diagnostics/
│        ├─ lower/
│        ├─ codegen/cpp/
│        ├─ runtime/
│        └─ hotreload/
│
├─ tools/
│  ├─ mmlc/
│  ├─ mmlfmt/
│  ├─ mmlls/
│  ├─ shadercc/                       # 构建期着色器跨编译（HLSL → DXIL/SPIRV/MSL）
│  ├─ minepack/
│  ├─ mineinst/                       # 自举安装程序（依赖整套 UI）
│  └─ ext.vscode/                     # TS 套壳
│     ├─ src/
│     ├─ syntaxes/
│     └─ package.json
│
├─ samples/
│  ├─ 00-hello-rect/              # M0：手写 C++ 创建窗口 + 渲染矩形
│  ├─ 01-hello-world/             # M1：MML 写出 Hello World
│  ├─ 02-mvvm-todo/               # M2：完整 MVVM Todo 应用
│  ├─ 03-sqlite-app/              # M3：SQLite 数据层示例
│  ├─ 04-network-feed/            # M3：网络请求示例
│  ├─ 05-multiwindow/             # M4：多窗口与多显示器
│  └─ 06-custom-control/          # M5+：自定义控件
│
├─ tests/                              # 集成/系统测试
│  ├─ ui-render/                       # 截图比对
│  ├─ binding/
│  └─ end_to_end/
│
├─ benchmarks/
├─ third_party/                        # 镜像/补丁第三方源（非主路径）
└─ scripts/
   ├─ bootstrap.ps1
   ├─ bootstrap.sh
   ├─ build-all.lua
   ├─ release.lua
   ├─ fmt.ps1
   ├─ lint.ps1
   ├─ pack-sdk.lua
   └─ gen-docs.lua
```

要点：

* 每个模块都有 `api/`、`src/`、`test/`、`xmake.lua`，结构同构 → 易于自动化。
* 公共头放 `api/include/mine/<layer>/<name>/*.h`，下游使用：`#include <mine/ui/visual/Visual.h>`。
* 测试与基准独立 target，CI 可单独跑。
* `samples/` 是真实工程，自身使用 xmake `add_requires("mineui")` 引入 SDK，**与外部用户体验一致**。
