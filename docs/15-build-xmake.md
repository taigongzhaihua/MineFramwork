# 15 — 构建（xmake）与函数级裁剪

## 15.1 顶层 xmake.lua 结构

```
xmake.lua                 # 顶层
xmake/
  options.lua             # 所有选项集中
  toolchains.lua          # MSVC/Clang/GCC/AppleClang 配置
  rules/
    mine_module.lua       # 自定义规则：mine 模块
    mml_compile.lua       # .mml -> .cpp 规则
    pkg_export.lua        # 安装/导出规则
modules/
  mineui.lua              # 第三方使用 add_requires 时引入
```

## 15.2 模块约定

每个模块目录形如：

```
src/mine/<layer>/<name>/
  api/                  # 接口头（INTERFACE target）
  src/                  # 实现
  test/                 # doctest 单元测试
  bench/                # 基准
  xmake.lua             # 模块自身
```

`src/mine/<layer>/<name>/xmake.lua` 内：

```lua
mine_module("mine.<layer>.<name>", {
    deps = { "mine.core", "mine.containers" },
    public_includes = { "api" },
    sources = "src/**.cpp",
    tests   = "test/**.cpp",
    options = { "MINE_ENABLE_<NAME>=1" },
})
```

`mine_module` 是自定义规则，统一处理：

* 编译选项（`/W4 /WX`、`-Wall -Wextra -Werror`）
* 函数级链接（`/Gy`、`-ffunction-sections`）
* 数据段切分（`/Gw`、`-fdata-sections`）
* 异常/RTTI 默认关闭（可被模块覆盖）
* 链接 GC（`/OPT:REF,ICF`、`-Wl,--gc-sections`）
* PCH（每模块）
* 生成 `mine.<x>.api` 子 target

## 15.3 选项（节选）

| option | 默认 | 描述 |
|--------|------|------|
| `mine.profile` | `std` | `min` / `std` / `full`，预设 |
| `mine.shared` | `false` | 输出 .dll/.so/.dylib |
| `mine.lto` | `auto` | release 自动开启 |
| `mine.exceptions` | `false` | 强制启用异常 |
| `mine.rtti` | `false` | 启用 RTTI |
| `mine.gfx.backends` | `d3d12,d3d11`(win) / `metal`(mac) / `vulkan`(linux) | 多选 |
| `mine.tls.backend` | `mbedtls` | `mbedtls` / `openssl` |
| `mine.module.<name>` | `auto` | 单独开关每个模块 |
| `mine.hot_reload` | `false` | 是否生成 mml runtime 反射 |

`min` 配置示例：仅核心 + UI 基础 + 一个简单窗口；移除 `controls.collection.datagrid`、`net.http2`、`svg`、`media`、`mml.runtime` 等。

## 15.4 函数级裁剪

正式版（release）默认启用：

| 编译器 | 关键参数 |
|--------|----------|
| MSVC | `/Gy /Gw /GL /O2 /Ob3 /Zc:inline` + `/LTCG /OPT:REF /OPT:ICF=4 /OPT:LBR` |
| Clang/LLD | `-ffunction-sections -fdata-sections -fmerge-all-constants -flto=thin` + `-Wl,--gc-sections,--icf=safe` |
| GCC | `-ffunction-sections -fdata-sections -flto=auto` + `-Wl,--gc-sections` |

附加策略：

1. **接口符号控制**：所有公共符号显式导出（`MINE_API`），其他默认 hidden（`-fvisibility=hidden`、MSVC `/d1FAH0` 不需要，但 `__declspec(dllexport)` 显式）。
2. **常量折叠表**：`mml` 生成的视觉树构造拆为大量小函数，便于链接器 ICF 折叠。
3. **`mml` 生成代码标注 `[[gnu::section("mineui_views")]]`**：方便整段裁剪。
4. **资源不混入符号**：图集/字体放进数据段或资源包，不影响代码裁剪。

## 15.5 编译时间优化

* PCH per module。
* 显式实例化常用模板（`Vector<int>` 等）。
* `mml` 生成代码尽量"窄"，避免巨型 cpp。
* 单元测试与基准编译目标可独立 (`xmake build mine.tests`)。

## 15.6 多平台 CI 矩阵

| 平台 | 编译器 | 后端 |
|------|--------|------|
| Windows x64 | MSVC 2022 / Clang-cl | D3D12, D3D11 |
| macOS arm64 | Apple Clang | Metal |
| Linux x64 | Clang 17, GCC 13 | Vulkan, GLES |
| Android arm64 | NDK r26 | Vulkan |
| iOS arm64 | Xcode toolchain | Metal |

`scripts/ci/*.yml` 提供 GitHub Actions / Azure Pipelines 模板。

## 15.7 第三方依赖

通过 xmake-repo / 自建仓库管理：

* `harfbuzz`, `freetype`, `mbedtls`, `nghttp2`, `sqlite3`(amalgamation), `stb`, `libpng`, `libjpeg-turbo`, `libwebp`, `utfcpp`, `dxc`, `doctest`, `nanobench`。

均通过 `add_requires("xxx", { configs = {...}, system = false })` 锁版本。
