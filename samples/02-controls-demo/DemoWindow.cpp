/**
 * @file DemoWindow.cpp
 * @brief DemoWindow code-behind 实现（用户手写）。
 *
 * 实现 DemoWindowBase 声明的 method 纯虚函数（对应 MML `method` 关键字）。
 * 通过 protected 引用直接访问 #id 元素（status_label_），
 * 等效于 WPF code-behind 中通过 x:Name 访问命名控件。
 * render() 继承自 mine::ui::Window，直接调用（无 win_. 前缀）。
 *
 * 参见 docs/04-precompiler.md §4.4.2 和 docs/07-windowing.md §7.9。
 */
#include "DemoWindow.h"

namespace app {

// ── method 实现（对应 MML `method on_count_clicked()`）──────────────────────

void DemoWindow::on_count_clicked()
{
    ++click_count_;
    update_status_();
}

// ── method 实现（对应 MML `method on_reset_clicked()`）──────────────────────

void DemoWindow::on_reset_clicked()
{
    click_count_ = 0;
    update_status_();
}

// ── 内部辅助（code-behind 私有）─────────────────────────────────────────────

void DemoWindow::update_status_()
{
    char buf[128];
    std::snprintf(buf, sizeof(buf), "当前计数：%d 次", click_count_);

    // 直接访问 protected 的 #id 元素（DemoWindowBase 暴露的 status_label_ 引用）
    // 等效于 WPF code-behind 中访问 x:Name 命名控件
    status_label_.set_text(buf);

    // render() 继承自 mine::ui::Window，直接调用（无 win_. 前缀）
    render();
}

} // namespace app