# MineFramework CMake 构建指南

本项目支持 xmake 和 CMake 双构建系统。本文档介绍如何使用 CMake 构建 MineFramework。

## 系统要求

- **CMake**: >= 3.25
- **编译器**:
  - Windows: MSVC 2022 或 Clang-cl
  - macOS: Apple Clang (Xcode 15+)
  - Linux: GCC 13+ 或 Clang 17+
- **C++ 标准**: C++23

## 快速开始

### Windows (MSVC)

```powershell
# 1. 创建构建目录
New-Item -ItemType Directory -Force build; cd build

# 2. 配置项目
cmake .. -G "Visual Studio 17 2022" -A x64

# 3. 构建
cmake --build . --config Release

# 4. 运行示例
.\bin\Release\sample.01-mvvm-binding.exe
```

### Windows (Ninja + MSVC)

```powershell
# 使用 Developer PowerShell for VS 2022

# 1. 创建构建目录
mkdir build; cd build

# 2. 配置项目
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release

# 3. 构建
cmake --build .

# 4. 运行示例
.\bin\sample.01-mvvm-binding.exe
```

### Linux / macOS

```bash
# 1. 创建构建目录
mkdir build && cd build

# 2. 配置项目
cmake .. -DCMAKE_BUILD_TYPE=Release

# 3. 构建
cmake --build . -j$(nproc)

# 4. 运行示例
./bin/sample.01-mvvm-binding
```

## 构建选项

CMake 提供了丰富的配置选项，对应 xmake 的选项系统：

### 基础选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `MINE_PROFILE` | `std` | 体积与功能裁剪预设 (`min`/`std`/`full`) |
| `MINE_BUILD_SHARED` | `OFF` | 输出共享库而不是静态库 |
| `MINE_ENABLE_LTO` | `OFF` | 启用链接时优化 (Release 推荐) |
| `MINE_ENABLE_EXCEPTIONS` | `OFF` | 启用 C++ 异常 |
| `MINE_ENABLE_RTTI` | `OFF` | 启用 RTTI |

### 构建目标

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `MINE_BUILD_SAMPLES` | `ON` | 构建示例程序 |
| `MINE_BUILD_TESTS` | `ON` | 构建单元测试 |
| `MINE_BUILD_TOOLS` | `ON` | 构建工具程序 (mmlc/mmlfmt/mmlls) |

### 后端选择

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `MINE_GFX_BACKENDS` | 平台相关 | 图形后端列表（分号分隔） |
| `MINE_TLS_BACKEND` | `mbedtls` | TLS 后端选择 (`mbedtls`/`openssl`) |

### 示例：自定义配置

```powershell
# 最小体积配置
cmake .. -DMINE_PROFILE=min `
         -DMINE_BUILD_SAMPLES=OFF `
         -DMINE_BUILD_TESTS=OFF `
         -DMINE_BUILD_TOOLS=OFF

# 完整功能 + LTO 优化
cmake .. -DMINE_PROFILE=full `
         -DMINE_ENABLE_LTO=ON `
         -DCMAKE_BUILD_TYPE=Release

# 共享库构建
cmake .. -DMINE_BUILD_SHARED=ON

# 仅构建核心库（禁用所有可选模块）
cmake .. -DMINE_MODULE_MVVM=OFF `
         -DMINE_MODULE_NAV=OFF `
         -DMINE_MODULE_CONFIG=OFF
```

## 国内镜像配置

MineFramework 的 CMake 构建系统已默认配置国内镜像源，优先使用 Gitee、清华大学、阿里云等镜像站。

### 自动镜像选择

`cmake/MineDependencies.cmake` 自动按以下顺序尝试：

1. **Gitee 镜像** - 优先
2. **清华大学镜像** - SQLite 等归档包
3. **GitHub** - 回退方案

### 手动配置镜像源

如果需要强制使用特定镜像，可以设置环境变量：

```powershell
# PowerShell
$env:FETCHCONTENT_SOURCE_DIR_FREETYPE="C:\deps\freetype"
cmake ..

# 或使用 Git 配置代理
git config --global url."https://gitee.com/mirrors/".insteadOf https://github.com/
```

### 离线构建

对于完全离线环境，可以预先下载所有依赖：

1. 在有网络的机器上运行：
   ```powershell
   mkdir build && cd build
   cmake .. -DFETCHCONTENT_FULLY_POPULATED=ON
   ```

2. 将 `build/_deps/` 目录打包

3. 在离线机器上解压到相同位置，然后构建

### 镜像源列表

当前使用的国内镜像：

| 依赖 | Gitee 镜像 | 备用镜像 |
|------|-----------|----------|
| FreeType | `gitee.com/mirrors/freetype` | GitHub |
| HarfBuzz | `gitee.com/mirrors/harfbuzz` | GitHub |
| doctest | `gitee.com/mirrors/doctest` | GitHub |
| stb | `gitee.com/mirrors/stb` | GitHub |
| libpng | `gitee.com/mirrors/libpng` | GitHub |
| mbedTLS | `gitee.com/mirrors/mbedtls` | GitHub |
| utf8cpp | `gitee.com/mirrors/utfcpp` | GitHub |
| SQLite | 清华大学镜像 | sqlite.org |

## 第三方依赖说明

MineFramework 通过 CMake FetchContent 自动下载和构建依赖：

### 核心依赖（必需）

- **FreeType** 2.13.2 - 字体渲染
- **HarfBuzz** 8.5.0 - 文本整形
- **utf8cpp** 4.0.5 - UTF-8 处理

### 图形依赖

- **libpng** 1.6.43 - PNG 图像支持
- **stb** (单头文件) - 图像加载

### 数据与网络

- **SQLite** 3.46.1 - 数据库（amalgamation）
- **mbedTLS** 3.6.0 - TLS/加密（可选 OpenSSL）

### 测试工具

- **doctest** 2.4.11 - 单元测试框架

所有依赖在首次配置时自动下载，后续构建会复用缓存。

## 构建目标

CMake 会生成以下目标：

### 库目标

- `mine.core`, `mine.containers`, `mine.math` - L0 基础层
- `mine.platform.abi`, `mine.platform.win32` - L1 平台层
- `mine.gfx.rhi`, `mine.gfx.d3d11` - L2 图形层
- `mine.paint` - L3 2D 绘图
- `mine.ui.*` - L4 UI 核心
- `mine.mvvm`, `mine.nav`, `mine.config` - L5 应用服务

每个库还会生成对应的 `.api` 接口库（纯头文件）。

### 示例目标

- `sample.00-hello-rect` - 基础矩形绘制
- `sample.00-blank-window` - 空白窗口
- `sample.01-mvvm-binding` - MVVM 数据绑定
- `sample.02-controls-demo` - 控件演示
- `sample.03-custom-chrome` - 自定义窗口样式

### 测试目标

每个模块的单元测试：

```bash
# 运行所有测试
ctest

# 运行特定测试
ctest -R mine.core.test
```

## 集成到项目

### 方式一：FetchContent（推荐）

```cmake
include(FetchContent)

FetchContent_Declare(
    MineFramework
    GIT_REPOSITORY https://gitee.com/your-org/MineFramework.git
    GIT_TAG v0.1.0
)

FetchContent_MakeAvailable(MineFramework)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE mine.ui.app mine.ui.controls)
```

### 方式二：安装后使用

```bash
# 安装到系统
cmake --install . --prefix /usr/local

# 在其他项目中
find_package(MineFramework REQUIRED)
target_link_libraries(my_app PRIVATE mine.ui.app)
```

### 方式三：子目录

```cmake
add_subdirectory(third_party/MineFramework)
target_link_libraries(my_app PRIVATE mine.ui.app)
```

## IDE 集成

### Visual Studio

直接打开 `CMakeLists.txt` 或生成 .sln 文件：

```powershell
cmake .. -G "Visual Studio 17 2022" -A x64
start MineFramework.sln
```

### CLion

CLion 原生支持 CMake，直接打开项目根目录即可。

### VS Code

安装 CMake Tools 扩展，然后：

1. `Ctrl+Shift+P` → `CMake: Configure`
2. `Ctrl+Shift+P` → `CMake: Build`
3. `F5` 调试

配置 `.vscode/settings.json`：

```json
{
  "cmake.configureArgs": [
    "-DMINE_BUILD_SAMPLES=ON"
  ],
  "cmake.buildDirectory": "${workspaceFolder}/build"
}
```

## 构建问题排查

### 问题：依赖下载失败

**原因**：网络连接问题或镜像不可用

**解决**：

1. 检查网络连接
2. 尝试使用 VPN
3. 手动下载依赖到 `build/_deps/` 目录
4. 设置 Git 代理：
   ```bash
   git config --global http.proxy http://127.0.0.1:7890
   ```

### 问题：编译错误 C2039

**原因**：C++ 标准设置不正确

**解决**：

```powershell
cmake .. -DCMAKE_CXX_STANDARD=23
```

### 问题：链接错误 LNK2019

**原因**：依赖顺序或库类型不匹配

**解决**：

1. 清理构建目录：`rm -rf build`
2. 确保所有依赖使用相同的库类型（静态/共享）

### 问题：找不到 Windows SDK

**原因**：未安装或路径配置错误

**解决**：

1. 使用 Visual Studio Installer 安装 Windows 11 SDK
2. 使用 Developer PowerShell for VS 2022

## 性能优化建议

### Release 构建

```powershell
cmake .. -DCMAKE_BUILD_TYPE=Release -DMINE_ENABLE_LTO=ON
```

### 并行构建

```powershell
# Windows
cmake --build . --config Release --parallel 8

# Linux/macOS
cmake --build . -j$(nproc)
```

### 函数级裁剪

默认已启用，Release 模式下自动应用：

- MSVC: `/Gy /Gw /GL /LTCG /OPT:REF,ICF`
- Clang/GCC: `-ffunction-sections -fdata-sections -Wl,--gc-sections`

## 与 xmake 对比

| 特性 | xmake | CMake |
|------|-------|-------|
| 配置语法 | Lua | CMake 语言 |
| 依赖管理 | xmake-repo | FetchContent |
| 国内镜像 | 内置支持 | 手动配置（已内置） |
| IDE 集成 | 有限 | 广泛支持 |
| 跨平台 | 原生支持 | 原生支持 |
| 构建速度 | 较快 | 中等 |
| 生态系统 | 较小 | 非常大 |

**推荐**：

- **开发阶段**：使用 xmake（配置简单，构建快）
- **发布/集成**：使用 CMake（生态好，兼容性强）

两个构建系统可以并存，互不干扰。

## 参考资源

- [CMake 官方文档](https://cmake.org/documentation/)
- [FetchContent 文档](https://cmake.org/cmake/help/latest/module/FetchContent.html)
- [MineFramework 设计文档](../docs/)

## 贡献

欢迎提交 PR 改进 CMake 构建系统！主要关注：

- 更多平台支持（macOS、Linux、Android、iOS）
- 依赖版本更新
- 构建性能优化
- 更多国内镜像源
