/**
 * @file main.cpp
 * @brief 01-mvvm-binding 应用入口（薄壳 CounterApp）。
 *
 * 本文件演示 MVVM + 数据绑定场景下的标准 Application 接入形态：
 *   - CounterApp 仅继承 mine::ui::app::Application
 *   - 持有 CounterWindow（IS-A Window）作为值成员
 *   - on_startup() 仅负责资源注入、信号订阅、调用 show()
 *
 * CounterWindow 内部通过 BindingExpression 将 CounterViewModel 的
 * 可观察属性自动同步到 TextBlock 控件，本文件无需了解绑定细节。
 *
 * 参见 docs/09-property-binding.md 和 docs/10-mvvm.md。
 */

// Win32 控制台 UTF-8（NOMINMAX 等已由 xmake add_defines 注入）
#include <windows.h>

#include <mine/ui/app/AppAll.h>  // Application + MINE_APPLICATION_MAIN
#include "CounterWindow.h"

// ── 应用主类（薄壳）──────────────────────────────────────────────────────────

/**
 * @brief MVVM 计数器演示应用（薄壳）。
 *
 * 所有 UI 逻辑封装在 CounterWindow 中；CounterApp 仅负责：
 *   1. 注入外部资源（字体）
 *   2. 订阅 CounterWindow 的关闭信号
 *   3. 调用 main_win_.show() 启动窗口
 */
struct CounterApp : public mine::ui::app::Application {

    /// 主窗口（持有 ViewModel 和全部 UI 控件）
    app::CounterWindow main_win_;

    void on_startup(int /*argc*/, char** /*argv*/) override
    {
        // 设置控制台输出为 UTF-8，避免日志中文乱码
        SetConsoleOutputCP(CP_UTF8);

        // 注入字体资源并完成视觉树构建与绑定激活
        // 注：待 ResourceDictionary 实现后，此步骤由框架自动完成
        main_win_.setup(default_font());

        // 订阅关闭信号 → 请求应用退出
        main_win_.set_on_close_requested([this] { quit(0); });

        // 首次 show()：懒初始化原生窗口，自动注册为主窗口
        main_win_.show();
    }
};

// ── 进程入口 ──────────────────────────────────────────────────────────────────

MINE_APPLICATION_MAIN(CounterApp)
