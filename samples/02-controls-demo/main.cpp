/**
 * @file main.cpp
 * @brief 02-controls-demo 应用入口（薄壳 DemoApp）。
 *
 * 本文件演示 mmlc 工具链完成后手写 Application 子类的标准形态：
 *   - DemoApp 仅继承 mine::ui::app::Application
 *   - 持有 DemoWindow（IS-A Window，继承自 DemoWindowBase : public Window）作为值成员
 *   - on_startup() 仅负责资源注入、信号订阅、调用 show()
 *
 * 待 ResourceDictionary 实现后，setup(font) 步骤将移除，
 * DemoWindowBase 的构造函数将在内部自动解析字体资源。
 *
 * 参见 docs/04-precompiler.md §4.4.2 和 docs/07-windowing.md §7.9。
 */

// Win32 控制台 UTF-8 代码页设置（NOMINMAX 等已由 xmake 的 add_defines 注入）
#include <windows.h>

#include <mine/ui/app/AppAll.h>  // Application + MINE_APPLICATION_MAIN
#include "DemoWindow.h"

// ── 应用主类（薄壳）──────────────────────────────────────────────────────────

/**
 * @brief 控件演示应用（薄壳）。
 *
 * 所有 UI 逻辑封装在 DemoWindow 中；DemoApp 仅负责：
 *   1. 注入外部资源（字体）
 *   2. 订阅 DemoWindow 的信号
 *   3. 调用 main_win_.show() 启动窗口
 *
 * DemoWindow IS-A Window（继承链：DemoWindow : DemoWindowBase : public Window），
 * 可直接传给任何需要 Window& 的 API。
 */
struct DemoApp : public mine::ui::app::Application {

    // DemoWindow IS-A Window，多态链完整（DemoWindow : DemoWindowBase : public Window）
    app::DemoWindow main_win_;

    void on_startup(int /*argc*/, char** /*argv*/) override
    {
        // 设置控制台输出为 UTF-8，避免中文日志乱码
        SetConsoleOutputCP(CP_UTF8);

        // 注入字体资源并完成视觉树构建（_build / _bind / _states）
        // 注：待 ResourceDictionary 实现后，此步骤由框架自动完成，调用方无需处理
        main_win_.setup(default_font());

        // 订阅关闭信号 → 请求应用退出
        main_win_.set_on_close_requested([this] { quit(0); });

        // 首次 show()：通过 IWindowContext 懒初始化原生窗口，自动注册为主窗口
        main_win_.show();
    }
};

// ── 进程入口 ──────────────────────────────────────────────────────────────────

MINE_APPLICATION_MAIN(DemoApp)

