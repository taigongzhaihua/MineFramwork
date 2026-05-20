/**
 * @file Visual.h
 * @brief Visual 视觉基类 —— 视觉树节点、变换、裁剪、渲染管线接入。
 *
 * Visual 是 MineFramework 所有可见 UI 元素的共同基类，提供：
 *   - 视觉树管理（父节点指针 + 子节点列表）
 *   - 局部仿射变换（RenderTransform，math::Transform2D）
 *   - 矩形裁剪区域（ClipRect，可选）
 *   - 可见性（Visibility 依赖属性）与不透明度（Opacity 依赖属性）
 *   - 渲染脏区追踪（invalidate_render / is_render_dirty）
 *   - 渲染管线接入 paint：render_to_canvas() 递归将自身及子树
 *     绘制到 paint::Canvas
 *   - 路由事件处理：实现 IRoutedEventTarget，支持 add/remove_handler
 *
 * 继承关系：
 *   DependencyObject  (mine.ui.property)
 *       └─ Visual     (mine.ui.visual)
 *           └─ UIElement
 *               └─ Control
 *
 * 线程安全：所有操作须在拥有此对象的 UI 线程执行。
 */

#pragma once

#include <cstdint>

#include <mine/ui/visual/Api.h>
#include <mine/ui/visual/Visibility.h>
#include <mine/core/Pimpl.h>
#include <mine/math/Transform2D.h>
#include <mine/math/Rect.h>
#include <mine/ui/property/DependencyObject.h>
#include <mine/ui/event/IRoutedEventTarget.h>
#include <mine/ui/event/RoutedEvent.h>

// 前向声明，避免将 Canvas.h 拉入公共头链
namespace mine::paint { class Canvas; }

namespace mine::ui {

// 前向声明，避免循环依赖（UIElement.h 包含 Visual.h）
class UIElement;

/**
 * @brief 视觉树基类。
 *
 * Visual 不直接持有子节点所有权（子节点生命周期由应用程序管理），
 * 仅维护非拥有的原始指针列表。父节点析构或移除子节点时，子节点的
 * parent 指针自动置空。
 */
class MINE_UI_VISUAL_API Visual : public DependencyObject,
                                   public IRoutedEventTarget {
public:
    // ── 依赖属性（静态描述符）────────────────────────────────────────────

    /**
     * @brief 不透明度属性，值类型 float，范围 [0.0, 1.0]，默认 1.0。
     *
     * 变更后触发 invalidate_render()。当前 M1.1 阶段 opacity 影响脏区标记；
     * 像素级混合由 mine.compose 在后续里程碑实现。
     */
    static const DependencyProperty& OpacityProperty;

    /**
     * @brief 可见性属性，值类型 Visibility，默认 Visibility::Visible。
     *
     * Collapsed 时跳过 render_to_canvas，也不参与布局（affects_measure/arrange）。
     * Hidden 时跳过渲染但仍占据布局空间。
     */
    static const DependencyProperty& VisibilityProperty;

    // ── 生命周期 ──────────────────────────────────────────────────────────

    Visual();
    ~Visual() override;

    // 禁止拷贝（Visual 具有身份，不可复制）
    Visual(const Visual&)            = delete;
    Visual& operator=(const Visual&) = delete;

    // 允许移动（仅供容器内部使用，移动后旧对象须处于"空"状态）
    Visual(Visual&&)            = default;
    Visual& operator=(Visual&&) = default;

    // ── 视觉树管理 ───────────────────────────────────────────────────────

    /**
     * @brief 返回父节点指针，若当前为根节点则返回 nullptr。
     */
    [[nodiscard]] Visual* parent() const noexcept;

    /**
     * @brief 返回直接子节点数量。
     */
    [[nodiscard]] uint32_t child_count() const noexcept;

    /**
     * @brief 按索引返回直接子节点指针（不做越界检查，Debug 模式下断言）。
     */
    [[nodiscard]] Visual* child_at(uint32_t index) const noexcept;

    /**
     * @brief 将 child 添加为当前节点的最后一个子节点。
     *
     * 前置条件：
     *   - child != nullptr
     *   - child != this（不能成为自身子节点）
     *   - child->parent() == nullptr（child 尚未挂载到其他父节点）
     */
    void add_child(Visual* child);

    /**
     * @brief 从子节点列表中移除 child。
     *
     * 若 child 不在列表中则为空操作。移除后 child->parent() 置为 nullptr。
     */
    void remove_child(Visual* child);

    /**
     * @brief 移除所有子节点，并将其父指针置为 nullptr。
     */
    void remove_all_children();

    // ── 局部变换 ─────────────────────────────────────────────────────────

    /**
     * @brief 返回当前局部仿射变换。
     *
     * 此变换在 render_to_canvas() 中通过 Canvas::transform() 应用，
     * 在 UIElement::hit_test() 中通过逆变换将屏幕坐标映射到局部坐标。
     */
    [[nodiscard]] const math::Transform2D& render_transform() const noexcept;

    /**
     * @brief 设置局部仿射变换并触发渲染失效。
     */
    void set_render_transform(const math::Transform2D& t);

    // ── 矩形裁剪 ─────────────────────────────────────────────────────────

    /**
     * @brief 返回当前是否有矩形裁剪区域。
     */
    [[nodiscard]] bool has_clip_rect() const noexcept;

    /**
     * @brief 返回当前矩形裁剪区域（仅 has_clip_rect() == true 时有效）。
     */
    [[nodiscard]] math::Rect clip_rect() const noexcept;

    /**
     * @brief 设置矩形裁剪区域并触发渲染失效。
     *
     * 与 Canvas::save/restore 结合使用：仅对本节点及子树有效。
     */
    void set_clip_rect(math::Rect rect);

    /**
     * @brief 清除矩形裁剪区域并触发渲染失效。
     */
    void clear_clip_rect();

    // ── 快捷属性访问器（依赖属性）────────────────────────────────────────

    /**
     * @brief 返回当前不透明度值（[0.0, 1.0]）。
     */
    [[nodiscard]] float opacity() const;

    /**
     * @brief 设置不透明度。值会被钳制到 [0.0, 1.0]。
     */
    void set_opacity(float v);

    /**
     * @brief 返回当前可见性状态。
     */
    [[nodiscard]] Visibility visibility() const;

    /**
     * @brief 设置可见性状态。
     */
    void set_visibility(Visibility v);

    // ── 脏区管理 ─────────────────────────────────────────────────────────

    /**
     * @brief 返回当前渲染脏标志。
     *
     * true 表示自上次渲染以来节点或子树有变化，需要重绘。
     */
    [[nodiscard]] bool is_render_dirty() const noexcept;

    /**
     * @brief 将渲染脏标志置为 true，并向上传播到根节点。
     *
     * 属性系统在 affects_render = true 的属性变更时自动调用此方法；
     * 也可由子类在自定义绘制数据变更时手动调用。
     */
    void invalidate_render() override;

    // ── 渲染管线 ─────────────────────────────────────────────────────────

    /**
     * @brief 将本节点及子树递归渲染到 Canvas。
     *
     * 渲染顺序：
     *   1. 若 Visibility == Collapsed，直接返回
     *   2. canvas.save()
     *   3. 若有变换，canvas.transform(render_transform_)
     *   4. 若有裁剪，canvas.clip_rect(clip_rect_)
     *   5. 若 Visibility == Visible，调用 on_render(canvas)
     *   6. 递归渲染所有子节点
     *   7. canvas.restore()
     *   8. 清除渲染脏标志
     *
     * @param canvas 目标 Canvas（录制模式，由窗口系统构造后传入）
     */
    void render_to_canvas(paint::Canvas& canvas);

    // ── 路由事件处理器管理 ────────────────────────────────────────────────

    /**
     * @brief 为指定路由事件注册处理器。
     *
     * 同一 (event, handler, user_data) 组合可多次注册，每次均会触发。
     *
     * @param event     目标路由事件描述符
     * @param fn        处理器函数指针（不可为 nullptr）
     * @param user_data 用户自定义数据，原样传回处理器
     */
    void add_handler(const RoutedEvent& event,
                     RoutedEventHandlerFn fn,
                     void* user_data = nullptr);

    /**
     * @brief 移除第一个匹配的路由事件处理器。
     *
     * 按 (event, handler, user_data) 三元组精确匹配，仅移除第一个。
     */
    void remove_handler(const RoutedEvent& event,
                        RoutedEventHandlerFn fn,
                        void* user_data = nullptr);

    // ── IRoutedEventTarget 接口实现 ───────────────────────────────────────

    /**
     * @brief 返回父节点作为路由事件目标（nullptr 表示根节点）。
     */
    [[nodiscard]] IRoutedEventTarget* parent_target() const noexcept override;

    /**
     * @brief 触发本节点上注册到指定事件的所有处理器。
     *
     * 按注册顺序调用；每次回调后检查 args.handled()，为 true 时停止本层处理。
     */
    void invoke_handlers(const RoutedEvent& event,
                          RoutedEventArgs&   args) noexcept override;

    // ── 无 RTTI 类型探查（替代 dynamic_cast）────────────────────────────

    /**
     * @brief 若此 Visual 实际上是 UIElement，返回 this 的 UIElement* 指针；
     *        否则返回 nullptr。
     *
     * 项目禁用 RTTI（/GR-），禁止使用 dynamic_cast。
     * UIElement 覆盖此方法返回 this，使命中测试得以在子树中递归。
     */
    [[nodiscard]] virtual UIElement* as_element() noexcept;

protected:
    /**
     * @brief 自身绘制虚方法，子类覆盖以实现自定义绘制逻辑。
     *
     * 此方法在 canvas 已完成 save/transform/clip 之后调用，
     * 坐标系处于本节点的局部坐标系中。
     *
     * 默认实现为空操作（Visual 本身不绘制任何内容）。
     *
     * @param canvas 当前绘制上下文
     */
    virtual void on_render(paint::Canvas& canvas);

    /**
     * @brief 覆盖 DependencyObject::on_property_changed，
     *        对 VisibilityProperty/OpacityProperty 的变更刷新内部缓存。
     */
    void on_property_changed(const DependencyProperty& prop,
                             const core::Variant&      old_value,
                             const core::Variant&      new_value) override;

private:
    struct Impl;
    core::Pimpl<Impl> p_;
};

} // namespace mine::ui
