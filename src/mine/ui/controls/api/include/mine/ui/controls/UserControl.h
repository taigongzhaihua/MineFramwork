/**
 * @file UserControl.h
 * @brief UserControl —— 可复用 UI 组件基类（任务 17.2）。
 *
 * UserControl 在 ContentControl 的基础上引入：
 *   - 视觉树根管理：通过 ContentProperty 持有根 UIElement*，
 *     并自动在 on_content_changed 中将其加入/移除视觉子树；
 *   - DataContext 依赖属性：供 MVVM 绑定使用；
 *   - 生命周期虚函数：on_initialized / on_loaded / on_unloaded；
 *   - 数据上下文变更虚函数：on_data_context_changed。
 *
 * 继承关系：
 *   DependencyObject (mine.ui.property)
 *       └─ Visual (mine.ui.visual)
 *           └─ UIElement (mine.ui.visual)
 *               └─ FrameworkElement (mine.ui.visual)
 *                   └─ Control (mine.ui.visual)
 *                       └─ ContentControl (mine.ui.controls)
 *                           └─ UserControl (mine.ui.controls)
 *                               └─ Page (mine.ui.controls)
 *
 * 典型用法（mmlc 生成代码）：
 * @code
 *   class LoginView : public UserControl {
 *   public:
 *       LoginView() {
 *           // mmlc 在此构建 UI 树并调用 set_content(root_)
 *           auto grid = core::make_owned<Grid>();
 *           // ... 填充子元素 ...
 *           root_ = grid.get();
 *           add_child_owned(std::move(grid));  // 生命周期纳入自身
 *           set_content(root_);
 *       }
 *   protected:
 *       void on_initialized() noexcept override { ... }
 *   private:
 *       Grid* root_{};
 *   };
 * @endcode
 */

#pragma once

#include <mine/ui/controls/Api.h>
#include <mine/ui/controls/ContentControl.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/core/Variant.h>

namespace mine::ui {

/**
 * @brief 可复用 UI 组件基类。
 *
 * UserControl 是 MML 组件系统（component X : UserControl）的 C++ 对应基类，
 * 也是手写复合控件的推荐基类。
 *
 * 与 Button/TextBlock 等控件不同，UserControl 的视觉树根由**开发者自己**
 * 在构造函数中通过 set_content(UIElement*) 设置，而不是由框架通过 ControlTemplate 构建。
 *
 * **生命周期顺序**（保证顺序）：
 *   1. 构造函数（set_content 设置根元素）
 *   2. 首次 measure_override 调用完成后 → on_initialized()（仅一次）
 *   3. 加入父节点视觉树后 → on_loaded()
 *   4. 从父节点视觉树移除后 → on_unloaded()
 *
 * **DataContext**：
 *   set_data_context() 写入 DataContextProperty；
 *   变更时调用虚方法 on_data_context_changed(old, new)。
 */
class MINE_UI_CONTROLS_API UserControl : public ContentControl {
public:
    // ── 依赖属性 ──────────────────────────────────────────────────────────

    /**
     * @brief 数据上下文属性（Variant 存储任意 ViewModel 对象指针或值）。
     *
     * MVVM 绑定时，Binding 表达式从 DataContext 解析路径。
     * 变更时触发 on_data_context_changed() 虚方法。
     * 默认值为空 Variant。
     */
    static const DependencyProperty& DataContextProperty;

    // ── 生命周期 ──────────────────────────────────────────────────────────

    UserControl();
    ~UserControl() override;

    UserControl(const UserControl&)            = delete;
    UserControl& operator=(const UserControl&) = delete;
    UserControl(UserControl&&)                 = default;
    UserControl& operator=(UserControl&&)      = default;

    // ── 数据上下文接口 ────────────────────────────────────────────────────

    /**
     * @brief 设置数据上下文（写入 DataContextProperty Local 槽）。
     *
     * @param ctx 上下文值（通常是 ViewModel 的原始指针包装为 Variant）
     */
    void set_data_context(const core::Variant& ctx);

    /**
     * @brief 返回当前数据上下文的原始 Variant 值。
     */
    [[nodiscard]] const core::Variant& data_context() const noexcept;

protected:
    // ── 生命周期虚函数（子类可覆盖）──────────────────────────────────────

    /**
     * @brief 初始化完成钩子：首次 measure_override 调用完成后触发（仅一次）。
     *
     * 此时元素已完成第一次尺寸测量但尚未一定加入父树。
     * 子类可在此处初始化需要测量结果的状态（如固定尺寸的画刷缓存）。
     *
     * 默认实现为空。
     */
    virtual void on_initialized() noexcept;

    /**
     * @brief 加入视觉树钩子：当本元素被添加到父节点视觉树时触发。
     *
     * 此时 parent() != nullptr，可安全向上遍历视觉树。
     * 子类可在此处启动动画、订阅外部事件等。
     *
     * 默认实现为空。
     */
    virtual void on_loaded() noexcept;

    /**
     * @brief 离开视觉树钩子：当本元素从父节点视觉树移除时触发。
     *
     * 此时 parent() 已为 nullptr。
     * 子类应在此处停止动画、取消外部事件订阅等，防止悬空引用。
     *
     * 默认实现为空。
     */
    virtual void on_unloaded() noexcept;

    /**
     * @brief 数据上下文变更钩子：DataContextProperty 变更时触发。
     *
     * 子类可在此处重新绑定 ViewModel 或刷新 UI 数据。
     *
     * 默认实现为空。
     *
     * @param old_ctx 变更前的数据上下文
     * @param new_ctx 变更后的数据上下文
     */
    virtual void on_data_context_changed(const core::Variant& old_ctx,
                                         const core::Variant& new_ctx) noexcept;

    // ── 覆盖 ContentControl 的内容变更钩子 ───────────────────────────────

    /**
     * @brief 内容变更时自动管理视觉子树。
     *
     * 将旧内容元素从视觉子树移除，将新内容元素加入视觉子树。
     * 此方法保证内容元素的"parent 指针"始终正确，使测量/渲染/命中测试正常工作。
     *
     * @param old_v 变更前的 Variant（UIElement* 或空）
     * @param new_v 变更后的 Variant（UIElement* 或空）
     */
    void on_content_changed(const core::Variant& old_v,
                             const core::Variant& new_v) noexcept override;

    // ── 覆盖 Visual 的父节点变更钩子 ─────────────────────────────────────

    /**
     * @brief 父节点变更时触发 on_loaded / on_unloaded。
     *
     * old_parent == nullptr 且 new_parent != nullptr → on_loaded()
     * old_parent != nullptr 且 new_parent == nullptr → on_unloaded()
     *
     * @param old_parent 变更前的父节点（nullptr 表示原来是根）
     * @param new_parent 变更后的父节点（nullptr 表示成为根）
     */
    void on_parent_changed(Visual* old_parent, Visual* new_parent) noexcept override;

    // ── 覆盖布局虚方法 ────────────────────────────────────────────────────

    /**
     * @brief Measure 覆盖：委托给内容根元素，并在首次测量后触发 on_initialized。
     *
     * 若内容为 UIElement*，调用 content_element()->measure(available) 并返回其期望尺寸；
     * 否则返回零尺寸。
     *
     * @param available 经约束处理后的可用内容区域尺寸（已减去 Margin）
     * @return 内容期望尺寸
     */
    math::Size measure_override(math::Size available) override;

    /**
     * @brief Arrange 覆盖：将内容根元素排列至全部内容区域。
     *
     * @param final_size 内容区域最终尺寸（已减去 Margin）
     * @return 实际占用的内容尺寸
     */
    math::Size arrange_override(math::Size final_size) override;

private:
    /// 是否已完成首次初始化（on_initialized 只触发一次）
    bool initialized_{ false };

    /// DataContextProperty 静态变更回调，转发到虚方法 on_data_context_changed
    static void s_on_data_context_changed(DependencyObject*         sender,
                                          const DependencyProperty& prop,
                                          const core::Variant&      old_v,
                                          const core::Variant&      new_v) noexcept;
};

} // namespace mine::ui
