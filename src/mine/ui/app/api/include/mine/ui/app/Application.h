/**
 * @file Application.h
 * @brief UI 应用程序基类。
 *
 * Application 是 mine.ui.app 模块的核心类，职责：
 *   1. 初始化平台宿主（IApplicationHost）、图形设备（IDevice）、渲染器（IRenderer）。
 *   2. 创建与管理 UI 窗口（ui::Window），持有其完整生命周期。
 *   3. 驱动主消息循环（委托 IApplicationHost::run()），并管理退出码。
 *   4. 提供 on_startup / on_exit 扩展点，供子类实现应用逻辑。
 *   5. 支持"主窗口"语义：主窗口关闭时自动退出主循环。
 *
 * 典型用法（与 MINE_APPLICATION_MAIN 宏配合）：
 * @code
 *   class MyApp : public mine::ui::app::Application {
 *   protected:
 *       void on_startup(int argc, char** argv) override {
 *           mine::platform::WindowDesc desc;
 *           desc.title = "My Application";
 *           desc.size  = {1280.0f, 720.0f};
 *           auto* win = create_window(desc);
 *           set_main_window(win);
 *           win->show();
 *       }
 *   };
 *   MINE_APPLICATION_MAIN(MyApp)
 * @endcode
 *
 * 初始化顺序（run() 内部）：
 *   on_create_host() → on_create_device() → on_create_renderer()
 *     → on_startup(argc, argv) → IApplicationHost::run()
 *     → on_exit(exit_code) → 析构所有资源
 *
 * 线程安全：所有方法须在调用 run() 的线程（即主线程）调用。
 *
 * 依赖：mine.platform.abi / mine.gfx.rhi / mine.paint / mine.ui.window
 */

#pragma once

#include <mine/ui/app/Api.h>
#include <mine/core/Pimpl.h>

// 前向声明，避免将大型头文件拉入公共接口
namespace mine::platform { class IApplicationHost; struct WindowDesc; }
namespace mine::gfx       { class IDevice; }
namespace mine::paint      { class IRenderer; }
namespace mine::ui         { class Window; }
namespace mine::core       { template<typename T> class OwnedPtr; }

namespace mine::ui::app {

/**
 * @brief UI 应用程序基类。
 *
 * 不可拷贝，不可移动。每个进程只应存在一个 Application 实例。
 * 通常不直接实例化，而是通过 MINE_APPLICATION_MAIN 宏与子类配合使用。
 */
class MINE_UI_APP_API Application {
public:
    Application();
    virtual ~Application();

    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;

    // ── 生命周期入口 ─────────────────────────────────────────────────────────

    /**
     * @brief 启动应用程序主循环，直至退出。
     *
     * 内部依次执行：
     *   1. 调用 on_create_host() 初始化平台宿主
     *   2. 调用 on_create_device() 初始化图形设备
     *   3. 调用 on_create_renderer() 初始化 2D 渲染器
     *   4. 调用 on_startup(argc, argv)（子类重写入口）
     *   5. 进入主消息循环（IApplicationHost::run()）
     *   6. 调用 on_exit(exit_code)（子类重写出口）
     *   7. 销毁所有资源并返回退出码
     *
     * @param argc 命令行参数数量
     * @param argv 命令行参数数组
     * @return 退出码（0 表示正常退出）
     */
    int run(int argc = 0, char** argv = nullptr);

    /**
     * @brief 请求退出主循环。
     *
     * 等价于调用 IApplicationHost::quit(exit_code)。
     * 可在任意线程调用（线程安全，委托给平台宿主实现）。
     *
     * @param exit_code 退出码（默认 0）
     */
    void quit(int exit_code = 0);

    // ── 窗口管理 ─────────────────────────────────────────────────────────────

    /**
     * @brief 创建 UI 窗口（Application 拥有其生命周期）。
     *
     * 内部调用平台宿主的 create_window() 创建原生窗口，
     * 再包装为 ui::Window（自动创建交换链、订阅平台事件）。
     *
     * @param desc 窗口创建参数（尺寸、标题、类型等）
     * @return 指向创建的窗口（不可为 nullptr），由 Application 拥有生命周期
     *
     * @pre run() 已经开始执行（即在 on_startup 或之后调用）
     */
    [[nodiscard]] ui::Window* create_window(const platform::WindowDesc& desc);

    /**
     * @brief 设置主窗口。
     *
     * 主窗口关闭（Closed 事件触发）时，Application 自动调用 quit(0)，
     * 结束主消息循环。
     *
     * @param window 由 create_window() 返回的窗口指针，传入 nullptr 可清除主窗口
     *
     * @pre run() 已开始执行
     */
    void set_main_window(ui::Window* window);

    // ── 基础设施访问 ─────────────────────────────────────────────────────────

    /**
     * @brief 获取平台应用宿主引用（run() 后有效）。
     */
    [[nodiscard]] platform::IApplicationHost& host();

    /**
     * @brief 获取图形设备引用（run() 后有效）。
     */
    [[nodiscard]] gfx::IDevice& device();

    /**
     * @brief 获取 2D 渲染器引用（run() 后有效）。
     */
    [[nodiscard]] paint::IRenderer& renderer();

protected:
    // ── 生命周期扩展点 ───────────────────────────────────────────────────────

    /**
     * @brief 主循环开始前调用（用于创建主窗口、初始化服务等）。
     *
     * 默认实现为空操作。子类应在此创建至少一个窗口并调用 set_main_window()，
     * 否则应用不会因窗口关闭而自动退出，需手动调用 quit()。
     *
     * @param argc 命令行参数数量
     * @param argv 命令行参数数组
     */
    virtual void on_startup(int argc, char** argv);

    /**
     * @brief 主循环结束后调用（可用于保存状态、释放非 RAII 资源等）。
     *
     * 默认实现为空操作。此时所有窗口仍然存在（析构在 run() 末尾）。
     *
     * @param exit_code 主循环返回的退出码
     */
    virtual void on_exit(int exit_code);

    // ── 工厂扩展点（测试注入用）──────────────────────────────────────────────

    /**
     * @brief 创建平台应用宿主（可在测试中覆盖以注入 Mock）。
     *
     * 默认实现调用 mine::platform::create_application_host()。
     */
    virtual core::OwnedPtr<platform::IApplicationHost> on_create_host();

    /**
     * @brief 创建图形设备（可在测试中覆盖以注入 Mock）。
     *
     * 默认实现调用 mine::gfx::create_device(Backend::Auto)。
     */
    virtual core::OwnedPtr<gfx::IDevice> on_create_device();

    /**
     * @brief 创建 2D 渲染器（可在测试中覆盖以注入 Mock）。
     *
     * 默认实现调用 mine::paint::create_renderer(device)。
     *
     * @param device 图形设备（由 on_create_device() 创建的实例）
     */
    virtual core::OwnedPtr<paint::IRenderer> on_create_renderer(gfx::IDevice* device);

private:
    struct Impl;
    core::Pimpl<Impl> p_;
};

} // namespace mine::ui::app
