# 16 — SDK 交付与版本策略

## 16.1 SDK 形态

`mineui-sdk-<ver>-<os>-<arch>.zip`，目录结构见 [14-toolchain.md](14-toolchain.md#145-打包工具minepack)。

三种发行 profile：

| Profile | 二进制规模 | 包含 |
|---------|-----------|------|
| `min`   | 最小 | core / containers / async / io / math / paint / compose / gfx 一个后端 / ui.visual / ui.controls.basic |
| `std`   | 标准 | min + ui.controls.input/collection/nav/dialog + style/anim + binding/property/event/input + mvvm/di + nav + i18n + config + sqlite |
| `full`  | 完整 | std + 全部控件 + svg/effects/media + net.* + orm + 热重载 / mml runtime |

## 16.2 二进制矩阵

每个 profile 同时提供：

* `static-mt`（MSVC `/MT`）/ `static-md`（`/MD`）
* `shared`（DLL/.so/.dylib）
* `debug` / `release`

## 16.3 版本与兼容

* 语义化版本 `MAJOR.MINOR.PATCH`。
* `MAJOR` 内：公共头文件 ABI 兼容、链接兼容；
* `MINOR`：新增 API、不可删/不可改签名；
* `PATCH`：仅 bug 修复，无 API 变更。
* 弃用流程：标 `[[deprecated]]` → 至少 1 个 `MINOR` 后才能在下一个 `MAJOR` 删除。

## 16.4 C ABI 接口

`include/mine/capi/` 提供长期稳定 C 头：

* 仅 `int`、指针、`mine_*_t` 不透明句柄；
* 出错通过 `mine_status_t` + `mine_last_error_*`；
* 是绑定（C#/Python/Rust/Go）目标层。

## 16.5 升级与迁移

* 每个 `MAJOR` 更新提供 `migration/<from>-to-<to>.md` 自动化工具：`minepack migrate`。
* `mml` 语法变更附 `mmlfix`：批量改写。

## 16.6 安全与签名

* SDK 包附 `SHA256SUMS` + 数字签名（cosign / minisign）。
* 安装程序与可执行工具：Windows Authenticode、macOS notarization、Linux GPG。
* 供应链：CI 产物来自受控 Runner，依赖来源固定散列。

## 16.7 文档与许可

* 选用 **MIT 或 Apache-2.0**（待定，二选一）。
* 第三方 NOTICE 自动汇总：`scripts/gen-notices.lua`。
* 在线文档站：`docs.mineui.dev`（mdbook 自动部署）。

## 16.8 包管理器集成

* **xmake-repo**：第一公民。
* **vcpkg port**：维护一个 port，发版本同步。
* **CMake**：导出 `MineUIConfig.cmake` 供下游 `find_package(MineUI)`。
* **Conan**：可选 recipe。
