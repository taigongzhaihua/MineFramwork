/**
 * @file DemoWindow.h
 * @brief DemoWindow —— code-behind 类（用户手写）。
 *
 * 继承 DemoWindowBase 并实现 MML 声明的 method 纯虚函数。
 * 对应 WPF 中的 .xaml.cpp（code-behind）文件。
 *
 * 可直接访问 DemoWindowBase 暴露的 protected #id 元素（status_label_）
 * 以及继承自 Window 的所有方法（render() 等），等效于 WPF code-behind
 * 中通过 x:Name 访问命名控件。
 *
 * 参见 docs/04-precompiler.md §4.4.2 和 docs/07-windowing.md §7.9。
 */
#pragma once
#include "DemoWindow.g.h"

namespace app {

/**
 * @brief 控件演示窗口 code-behind（用户手写）。
 *
 * 继承 DemoWindowBase 实现 MML 声明的纯虚 method。
 * 通过 protected 引用直接访问 #id 元素（status_label_），
 * 通过继承自 Window 的 render() 触发重绘。
 */
class DemoWindow final : public DemoWindowBase {
public:
    DemoWindow() = default;

    // 实现 MML 声明的 method（对应 DemoWindow.mml 中的 method 关键字）
    void on_count_clicked() override;
    void on_reset_clicked() override;

private:
    /** 根据 click_count_ 刷新状态标签文本并触发重绘。 */
    void update_status_();

    int click_count_ = 0;  ///< 当前点击计数（视图局部状态，不属于业务逻辑）
};

} // namespace app

