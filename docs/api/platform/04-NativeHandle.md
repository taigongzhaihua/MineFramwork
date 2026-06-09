# NativeHandle 详细接口文档

## 概述

`NativeHandle` 是 `mine.platform.abi` 模块的**平台原生窗口句柄包装器**，提供 ABI 安全的不透明句柄存储。

**核心特性：**
- **不透明封装**：上层代码不得直接解引用，仅由平台后端解释
- **多形态存储**：`union { void* ptr; intptr_t value; }`，支持指针和整数句柄
- **跨平台兼容**：统一表示 HWND、NSWindow*、XID、wl_surface* 等
- **ABI 稳定**：`sizeof(NativeHandle) == sizeof(void*)`，POD 类型

---

## 文件位置

```
src/mine/platform/abi/api/include/mine/platform/NativeHandle.h
```

---

## 类型定义

```cpp
namespace mine::platform {

union NativeHandle {
    void*    ptr;    // 通用指针型句柄（HWND、NSWindow* 等）
    intptr_t value;  // 整数型句柄（XID 等）

    constexpr NativeHandle() noexcept;
    explicit constexpr NativeHandle(void* p) noexcept;
    explicit constexpr NativeHandle(intptr_t v) noexcept;

    [[nodiscard]] constexpr bool is_null() const noexcept;
    [[nodiscard]] constexpr bool operator==(const NativeHandle&) const noexcept;
    [[nodiscard]] constexpr bool operator!=(const NativeHandle&) const noexcept;
};

}
```

---

## 构造函数

### 默认构造

```cpp
constexpr NativeHandle() noexcept : value{0} {}
```

**描述**：创建空句柄（`value == 0`）。

**示例**：
```cpp
NativeHandle handle;  // 空句柄
assert(handle.is_null());
```

---

### 指针构造

```cpp
explicit constexpr NativeHandle(void* p) noexcept : ptr{p} {}
```

**描述**：从指针构造句柄（Windows HWND、macOS NSWindow*）。

**参数**：
- `p`：平台窗口指针

**示例**：
```cpp
// Windows
HWND hwnd = CreateWindowEx(...);
NativeHandle handle(hwnd);

// macOS
NSWindow* nswindow = [[NSWindow alloc] init...];
NativeHandle handle((__bridge void*)nswindow);
```

---

### 整数构造

```cpp
explicit constexpr NativeHandle(intptr_t v) noexcept : value{v} {}
```

**描述**：从整数构造句柄（Linux XID）。

**参数**：
- `v`：平台窗口整数标识符

**示例**：
```cpp
// Linux X11
Window xid = XCreateWindow(...);
NativeHandle handle(static_cast<intptr_t>(xid));
```

---

## 成员字段

### ptr

```cpp
void* ptr;
```

**描述**：通用指针型句柄，存储 HWND、NSWindow*、ANativeWindow*、UIWindow* 等。

**平台映射**：
- **Windows**：`HWND`（Win32 窗口句柄）
- **macOS**：`NSWindow*`（Cocoa 窗口对象）
- **Android**：`ANativeWindow*`（NDK 窗口）
- **iOS**：`UIWindow*`（UIKit 窗口）

**使用方式**：
```cpp
// Windows
HWND hwnd = static_cast<HWND>(handle.ptr);

// macOS
NSWindow* nswindow = (__bridge NSWindow*)handle.ptr;
```

---

### value

```cpp
intptr_t value;
```

**描述**：整数型句柄，存储 XID（X11 窗口 ID）或 wl_surface* 指针值。

**平台映射**：
- **Linux X11**：`Window`（`typedef unsigned long`，窗口 ID）
- **Linux Wayland**：`wl_surface*` 转换为 `intptr_t`

**使用方式**：
```cpp
// Linux X11
Window xid = static_cast<Window>(handle.value);

// Linux Wayland
wl_surface* surface = reinterpret_cast<wl_surface*>(handle.value);
```

---

## 成员方法

### is_null()

```cpp
[[nodiscard]] constexpr bool is_null() const noexcept {
    return value == 0;
}
```

**描述**：检查句柄是否为空。

**返回值**：
- `true`：句柄为空（`value == 0`）
- `false`：句柄有效

**示例**：
```cpp
NativeHandle handle;
if (handle.is_null()) {
    // 句柄为空
}

handle = NativeHandle(hwnd);
assert(!handle.is_null());
```

---

### operator==

```cpp
[[nodiscard]] constexpr bool operator==(const NativeHandle& other) const noexcept {
    return value == other.value;
}
```

**描述**：比较两个句柄是否相等。

**参数**：
- `other`：另一个句柄

**返回值**：
- `true`：两个句柄指向同一窗口
- `false`：不同窗口

**示例**：
```cpp
NativeHandle handle1(hwnd);
NativeHandle handle2(hwnd);
assert(handle1 == handle2);
```

---

### operator!=

```cpp
[[nodiscard]] constexpr bool operator!=(const NativeHandle& other) const noexcept {
    return value != other.value;
}
```

**描述**：比较两个句柄是否不相等。

**参数**：
- `other`：另一个句柄

**返回值**：
- `true`：不同窗口
- `false`：同一窗口

**示例**：
```cpp
NativeHandle handle1(hwnd1);
NativeHandle handle2(hwnd2);
if (handle1 != handle2) {
    // 不同窗口
}
```

---

## 平台映射

### Windows

**类型**：`HWND`（`typedef void*`，Win32 窗口句柄）

**存储字段**：`ptr`

**示例**：
```cpp
// 封装 HWND
HWND hwnd = CreateWindowEx(...);
NativeHandle handle(hwnd);

// 解封装
HWND hwnd = static_cast<HWND>(handle.ptr);
InvalidateRect(hwnd, nullptr, TRUE);
```

---

### macOS

**类型**：`NSWindow*`（Cocoa 窗口对象指针）

**存储字段**：`ptr`

**示例**：
```cpp
// 封装 NSWindow*
NSWindow* nswindow = [[NSWindow alloc] initWithContentRect:...];
NativeHandle handle((__bridge void*)nswindow);

// 解封装
NSWindow* nswindow = (__bridge NSWindow*)handle.ptr;
[nswindow setTitle:@"Title"];
```

---

### Linux (X11)

**类型**：`Window`（`typedef unsigned long`，窗口 XID）

**存储字段**：`value`

**示例**：
```cpp
// 封装 XID
Display* display = XOpenDisplay(nullptr);
Window xid = XCreateWindow(display, ...);
NativeHandle handle(static_cast<intptr_t>(xid));

// 解封装
Window xid = static_cast<Window>(handle.value);
XMapWindow(display, xid);
```

---

### Linux (Wayland)

**类型**：`wl_surface*`（Wayland 表面指针）

**存储字段**：`value`（指针转换为整数）

**示例**：
```cpp
// 封装 wl_surface*
wl_surface* surface = wl_compositor_create_surface(compositor);
NativeHandle handle(reinterpret_cast<intptr_t>(surface));

// 解封装
wl_surface* surface = reinterpret_cast<wl_surface*>(handle.value);
wl_surface_commit(surface);
```

---

### Android

**类型**：`ANativeWindow*`（NDK 窗口指针）

**存储字段**：`ptr`

**示例**：
```cpp
// 封装 ANativeWindow*
ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
NativeHandle handle(window);

// 解封装
ANativeWindow* window = static_cast<ANativeWindow*>(handle.ptr);
ANativeWindow_setBuffersGeometry(window, ...);
```

---

### iOS

**类型**：`UIWindow*`（UIKit 窗口对象指针）

**存储字段**：`ptr`

**示例**：
```cpp
// 封装 UIWindow*
UIWindow* window = [[UIWindow alloc] initWithFrame:...];
NativeHandle handle((__bridge void*)window);

// 解封装
UIWindow* window = (__bridge UIWindow*)handle.ptr;
[window makeKeyAndVisible];
```

---

## 使用场景

### 1. 获取窗口原生句柄

```cpp
#include <mine/platform/IWindow.h>
#include <mine/platform/NativeHandle.h>

void integrate_with_native_api(IWindow* window) {
    NativeHandle handle = window->native_handle();
    
    if (handle.is_null()) {
        // 窗口未创建或已销毁
        return;
    }
    
#ifdef _WIN32
    HWND hwnd = static_cast<HWND>(handle.ptr);
    // 调用 Win32 API
    SetWindowText(hwnd, L"New Title");
#elif defined(__APPLE__)
    NSWindow* nswindow = (__bridge NSWindow*)handle.ptr;
    // 调用 Cocoa API
    [nswindow setAlphaValue:0.9];
#elif defined(__linux__)
    Window xid = static_cast<Window>(handle.value);
    // 调用 X11 API
    XSetTransientForHint(display, xid, parent_xid);
#endif
}
```

---

### 2. 第三方库集成

```cpp
#include <mine/platform/IWindow.h>
#include <mine/platform/NativeHandle.h>
#include <vulkan/vulkan.h>

// 创建 Vulkan 表面
VkResult create_vulkan_surface(IWindow* window, VkInstance instance, VkSurfaceKHR* surface) {
    NativeHandle handle = window->native_handle();
    
#ifdef _WIN32
    VkWin32SurfaceCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    info.hwnd = static_cast<HWND>(handle.ptr);
    info.hinstance = GetModuleHandle(nullptr);
    return vkCreateWin32SurfaceKHR(instance, &info, nullptr, surface);
    
#elif defined(__APPLE__)
    VkMacOSSurfaceCreateInfoMVK info{};
    info.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
    info.pView = (__bridge void*)[(__bridge NSWindow*)handle.ptr contentView];
    return vkCreateMacOSSurfaceMVK(instance, &info, nullptr, surface);
    
#elif defined(__linux__)
    VkXlibSurfaceCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    info.dpy = display;
    info.window = static_cast<Window>(handle.value);
    return vkCreateXlibSurfaceKHR(instance, &info, nullptr, surface);
#endif
}
```

---

### 3. OpenGL 上下文创建

```cpp
#ifdef _WIN32
void create_opengl_context(IWindow* window) {
    NativeHandle handle = window->native_handle();
    HWND hwnd = static_cast<HWND>(handle.ptr);
    
    HDC hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = { ... };
    int pixel_format = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, pixel_format, &pfd);
    
    HGLRC hglrc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, hglrc);
}
#endif
```

---

### 4. 自定义窗口消息处理

```cpp
#ifdef _WIN32
class CustomWindowProc {
    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
        // 自定义消息处理
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }
    
public:
    void install_subclass(IWindow* window) {
        NativeHandle handle = window->native_handle();
        HWND hwnd = static_cast<HWND>(handle.ptr);
        
        // 子类化窗口
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&wnd_proc));
    }
};
#endif
```

---

## 最佳实践

### 1. 始终检查句柄有效性

```cpp
// ✅ 推荐：使用前检查句柄
NativeHandle handle = window->native_handle();
if (!handle.is_null()) {
    // 安全使用句柄
}

// ❌ 不推荐：直接使用可能为空的句柄
HWND hwnd = static_cast<HWND>(window->native_handle().ptr);  // 可能为 nullptr
```

---

### 2. 仅在平台特定代码中解引用

```cpp
// ✅ 推荐：使用条件编译隔离平台代码
#ifdef _WIN32
    HWND hwnd = static_cast<HWND>(handle.ptr);
    // Win32 代码
#elif defined(__APPLE__)
    NSWindow* nswindow = (__bridge NSWindow*)handle.ptr;
    // Cocoa 代码
#endif

// ❌ 不推荐：在跨平台代码中直接解引用
void* ptr = handle.ptr;  // 不知道如何解释
```

---

### 3. 不要存储原生句柄

```cpp
// ❌ 不推荐：存储原生句柄（窗口可能被销毁）
class MyClass {
    HWND hwnd_;  // 窗口销毁后悬空
};

// ✅ 推荐：存储 IWindow*，需要时获取句柄
class MyClass {
    IWindow* window_;
    
    void use_native_handle() {
        NativeHandle handle = window_->native_handle();
        if (!handle.is_null()) {
            HWND hwnd = static_cast<HWND>(handle.ptr);
            // 使用句柄
        }
    }
};
```

---

### 4. 注意 ABI 边界

```cpp
// ✅ 推荐：NativeHandle 跨 DLL 边界传递安全
// module_a.dll
NativeHandle get_handle() {
    return window->native_handle();
}

// module_b.dll
void use_handle(NativeHandle handle) {
    // 安全：NativeHandle 是 POD 类型
}

// ❌ 不推荐：直接传递平台类型
#ifdef _WIN32
HWND get_hwnd() {
    return static_cast<HWND>(window->native_handle().ptr);
}
// HWND 跨 DLL 传递可能 ABI 不兼容
#endif
```

---

## 常见陷阱

### 1. 混淆 ptr 和 value

```cpp
// ❌ 错误：Windows 使用 value 字段
#ifdef _WIN32
    intptr_t v = handle.value;  // 错误：应使用 ptr
    HWND hwnd = reinterpret_cast<HWND>(v);
#endif

// ✅ 正确：Windows 使用 ptr 字段
#ifdef _WIN32
    HWND hwnd = static_cast<HWND>(handle.ptr);
#endif
```

---

### 2. X11 句柄大小假设

```cpp
// ❌ 错误：假设 Window 是 32 位
Window xid = static_cast<uint32_t>(handle.value);  // 64 位系统上截断

// ✅ 正确：使用正确的类型
Window xid = static_cast<Window>(handle.value);  // Window 是 unsigned long
```

---

### 3. macOS ARC 问题

```cpp
// ❌ 错误：未使用 __bridge 转换
NSWindow* nswindow = (NSWindow*)handle.ptr;  // ARC 错误

// ✅ 正确：使用 __bridge
NSWindow* nswindow = (__bridge NSWindow*)handle.ptr;
```

---

## 完整示例

```cpp
#include <mine/platform/IWindow.h>
#include <mine/platform/IApplicationHost.h>
#include <mine/platform/NativeHandle.h>

class NativeIntegration {
    IWindow* window_;
    
public:
    void apply_custom_chrome() {
        NativeHandle handle = window_->native_handle();
        
        if (handle.is_null()) {
            log_error("窗口句柄为空");
            return;
        }
        
#ifdef _WIN32
        HWND hwnd = static_cast<HWND>(handle.ptr);
        
        // 移除标题栏
        LONG style = GetWindowLong(hwnd, GWL_STYLE);
        style &= ~WS_CAPTION;
        SetWindowLong(hwnd, GWL_STYLE, style);
        
        // 应用阴影
        MARGINS margins = {1, 1, 1, 1};
        DwmExtendFrameIntoClientArea(hwnd, &margins);
        
#elif defined(__APPLE__)
        NSWindow* nswindow = (__bridge NSWindow*)handle.ptr;
        
        // 隐藏标题栏但保留窗口按钮
        nswindow.titlebarAppearsTransparent = YES;
        nswindow.titleVisibility = NSWindowTitleHidden;
        nswindow.styleMask |= NSWindowStyleMaskFullSizeContentView;
        
#elif defined(__linux__)
        Window xid = static_cast<Window>(handle.value);
        
        // 请求窗口管理器移除装饰
        Atom motif_hints = XInternAtom(display, "_MOTIF_WM_HINTS", False);
        struct {
            uint32_t flags = 2;
            uint32_t functions = 0;
            uint32_t decorations = 0;
            int32_t input_mode = 0;
            uint32_t status = 0;
        } hints;
        XChangeProperty(display, xid, motif_hints, motif_hints, 32, 
                       PropModeReplace, (unsigned char*)&hints, 5);
#endif
    }
};
```

---

## 性能考虑

### 1. 零开销抽象

```cpp
// NativeHandle 是 union，sizeof(NativeHandle) == sizeof(void*)
static_assert(sizeof(NativeHandle) == sizeof(void*));
```

---

### 2. constexpr 构造

```cpp
// 编译期构造，无运行时开销
constexpr NativeHandle handle(nullptr);
```

---

### 3. 避免频繁获取句柄

```cpp
// ❌ 不推荐：循环中重复获取
for (int i = 0; i < 1000; ++i) {
    NativeHandle handle = window->native_handle();  // 虚函数调用
}

// ✅ 推荐：缓存句柄
NativeHandle handle = window->native_handle();
for (int i = 0; i < 1000; ++i) {
    // 使用缓存的句柄
}
```

---

## 总结

`NativeHandle` 是 `mine.platform.abi` 模块的平台原生窗口句柄包装器，具备：

1. **不透明封装**：上层代码不得直接解引用，仅由平台后端解释
2. **多形态存储**：`union { void* ptr; intptr_t value; }`，支持指针和整数句柄
3. **跨平台兼容**：统一表示 HWND、NSWindow*、XID、wl_surface* 等
4. **ABI 稳定**：`sizeof(NativeHandle) == sizeof(void*)`，POD 类型

**使用建议**：
- 始终检查 `is_null()` 确保句柄有效
- 仅在平台特定代码（`#ifdef`）中解引用
- Windows/macOS/Android/iOS 使用 `ptr` 字段
- Linux 使用 `value` 字段
- 不要存储原生句柄，始终通过 `IWindow::native_handle()` 获取
- 适用于第三方库集成（Vulkan、OpenGL、DirectX）
