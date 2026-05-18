/**
 * @file main.cpp
 * @brief 00-blank-window 示例：使用 mine.platform.win32 创建一个空白窗口。
 *
 * 演示 IApplicationHost + IWindow 基本用法：
 *   1. 创建 Win32 应用宿主
 *   2. 描述并创建一个 800×600 普通窗口
 *   3. 显示窗口并进入消息循环
 *   4. 窗口关闭后消息循环退出，程序返回退出码
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <mine/platform/PlatformAbi.h>
#include <mine/platform/win32/Win32ApplicationHost.h>

int WINAPI WinMain(
    HINSTANCE /*hInstance*/,
    HINSTANCE /*hPrevInstance*/,
    LPSTR     /*lpCmdLine*/,
    int       /*nCmdShow*/)
{
    // 创建 Win32 应用宿主（内部开启 per-monitor DPI v2 感知）
    auto host = mine::platform::win32::create_application_host();

    // 描述窗口
    mine::platform::WindowDesc desc{};
    desc.title         = "MineFramework - 空白窗口";
    desc.size          = {800.0f, 600.0f};
    desc.auto_position = true;
    desc.resizable     = true;
    desc.kind          = mine::platform::WindowKind::Normal;

    // 创建窗口
    auto window = host->create_window(desc);
    if (!window) {
        MessageBoxW(nullptr, L"窗口创建失败", L"错误", MB_ICONERROR | MB_OK);
        return 1;
    }

    // 显示窗口
    window->show();

    // 进入消息循环，直到最后一个窗口关闭
    return host->run();
}
