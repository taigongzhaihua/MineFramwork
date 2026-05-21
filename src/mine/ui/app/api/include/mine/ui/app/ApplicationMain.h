/**
 * @file ApplicationMain.h
 * @brief MINE_APPLICATION_MAIN 入口宏定义。
 *
 * 使用方式：
 * @code
 *   // main.cpp
 *   #include <mine/ui/app/ApplicationMain.h>
 *   #include "MyApp.h"
 *
 *   MINE_APPLICATION_MAIN(MyApp)
 * @endcode
 *
 * 宏展开为平台对应的进程入口（Windows：int main / WinMain 根据子系统配置）。
 * 当前阶段统一使用标准 C++ `int main`，兼容控制台子系统与 GUI 子系统。
 *
 * 将来可扩展为 WinMain（/SUBSYSTEM:WINDOWS）和 UIApplicationMain（iOS）。
 */

#pragma once

/**
 * @def MINE_APPLICATION_MAIN(AppClass)
 * @brief 生成进程入口函数，实例化并运行指定的 Application 子类。
 *
 * @param AppClass  继承自 mine::ui::app::Application 的具体类型名称（不加引号）
 *
 * 展开后等价于：
 * @code
 *   int main(int argc, char** argv) {
 *       AppClass app;
 *       return app.run(argc, argv);
 *   }
 * @endcode
 *
 * 注意：
 *   - 该宏只能在一个翻译单元中使用一次。
 *   - AppClass 必须可默认构造。
 *   - AppClass 的析构函数将在 run() 返回后由编译器自动调用（栈展开）。
 */
#define MINE_APPLICATION_MAIN(AppClass)         \
    int main(int argc, char** argv) {           \
        AppClass _mine_app_;                    \
        return _mine_app_.run(argc, argv);      \
    }
