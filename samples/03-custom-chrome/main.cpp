/**
 * @file main.cpp
 * @brief 03-custom-chrome 应用入口。
 *
 * 演示 WindowChrome 自定义无边框标题栏。
 * 窗口关闭时（Closed 事件），Application 自动调用 quit(0)。
 */

#include <windows.h>
#include <mine/ui/app/AppAll.h>
#include "CustomChromeWindow.h"

/**
 * @brief 03-custom-chrome 应用类（薄壳 Application 子类）。
 *
 * on_startup() 中完成所有初始化：字体注入 → 视觉树构建 → 窗口显示。
 * 窗口关闭后 Application 自动 quit(0)（由内置 MainWindowCloseSink 触发）。
 */
struct CustomChromeApp final : public mine::ui::app::Application {

    app::CustomChromeWindow main_win_;

    void on_startup(int /*argc*/, char** /*argv*/) override
    {
        // 确保控制台以 UTF-8 输出（调试辅助）
        SetConsoleOutputCP(CP_UTF8);

        // 注入默认字体并构建视觉树
        main_win_.setup(default_font());

        // 显示窗口（触发懒初始化：创建 Win32 窗口、D3D11 交换链等）
        main_win_.show();
    }
};

MINE_APPLICATION_MAIN(CustomChromeApp)
