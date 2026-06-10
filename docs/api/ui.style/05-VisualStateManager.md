# VisualStateManager API 文档

## 概述

`VisualStateManager`（VSM）是 MineUI 控件视觉状态管理器，提供基于状态机的视觉状态切换和平滑过渡动画。它通过注册状态、定义过渡和关联样式，实现控件在不同交互状态（Normal、Hovered、Pressed、Disabled 等）之间的智能切换。

### 核心特性

1. **状态机管理**  
   通过 `define_state()` 注册可用状态，`go_to_state()` 驱动状态切换。状态名作为键关联到 `Style` 中的状态 setter 集合（通过 `apply_state()` 应用）。

2. **过渡配置**  
   - **立即跳变**：`add_transition(from, to)` 注册无动画过渡（直接应用目标状态 setter）
   - **动画过渡**：`add_transition(from, to, configure_fn)` 注册带 Storyboard 动画的平滑过渡
   - **通配符支持**：from 可设为 `"*"` 表示从任意状态触发过渡（简化配置）

3. **与 Style 关联**  
   通过 `set_style(style)` 关联样式对象。状态切换时自动调用 `style->apply_state(owner, state_name)`，将目标状态的属性 setter 以 `StyleTrigger(30)` 优先级写入宿主控件。

4. **动画生命周期管理**  
   - `go_to_state()` 创建 Storyboard 实例并加入活跃列表
   - `tick_animations(dt)` 每帧推进所有活跃动画，完成后自动清理
   - `stop_all_storyboards()` 立即终止并清理所有进行中动画

5. **与 Control 集成**  
   - `Control::set_visual_state_manager()` 存储 VSM 实例
   - `Control::update_visual_state()` 根据事件调用 `vsm()->go_to_state()`
   - 渲染循环每帧调用 `vsm()->tick_animations(dt)` 驱动属性动画

6. **循环依赖避免**  
   构造函数接受 `DependencyObject&`（而非 `Control&`），使 `mine.ui.style` 仅依赖 `mine.ui.property`，不依赖 `mine.ui.visual`。通过前向声明和指针解耦依赖。

### 状态切换流程（带动画）

```
go_to_state("Hovered", use_transitions=true) 执行流程：

1. 验证目标状态已注册且不同于当前状态
2. 停止并清理所有活跃 Storyboard（中断上次未完成动画）
3. 查找匹配过渡（"Normal" → "Hovered" 或 "*" → "Hovered"）
4. 若找到带 configure_fn 的过渡：
   a. 创建 Storyboard 实例
   b. 调用 configure_fn(sb) 注册要驱动的属性（Opacity、BackgroundColor 等）
   c. 采样 from 值（capture_from_values）
   d. 应用目标状态样式（style->apply_state，写入 StyleTrigger(P30)）
   e. 解析 to 值并开始动画（resolve_and_begin，写入 Animation(P60)）
   f. 将 Storyboard 加入 active_storyboards_
5. 若未找到或 use_transitions=false：直接 apply_state（立即跳变）
6. 更新 current_state_ = "Hovered"
7. 返回 true
```

### 动画优先级机制

```cpp
属性值优先级（由高到低）：
  P60 Animation      ← Storyboard 驱动的插值（由 tick_animations 更新）
  P30 StyleTrigger   ← 状态样式 setter（apply_state 写入）
  P20 StyleSetter    ← 基础样式 setter
  P10 Local          ← 代码直接赋值

过渡过程中的优先级变化：
  1. capture_from_values()：读取当前生效值作为动画起点
  2. apply_state()：写入目标状态 setter（P30 StyleTrigger）
  3. resolve_and_begin()：创建插值对象，写入 P60 Animation 槽
  4. tick_animations(dt)：每帧更新 P60 槽的插值结果
  5. 动画完成时 stop()：清除 P60 槽，属性回退到 P30（目标状态样式值）
```

---

## 文件位置

```
src/mine/ui/style/api/include/mine/ui/style/VisualStateManager.h
```

---

## 类定义

### VisualStateTransition（状态过渡描述）

```cpp
namespace mine::ui::style {

/**
 * @brief 状态过渡描述。
 *
 * 记录从 from 状态到 to 状态的过渡：
 *   - from 为 "*" 或空字符串时匹配任意源状态（通配符）
 *   - to   必须是已注册的状态名
 *   - configure 为可选的 Storyboard 配置函数；为空时立即跳变（无动画）
 *
 * Task #17：追加 configure 字段，使 VisualStateTransition 成为 move-only 类型。
 * SmallVector 通过 push_back(std::move(t)) 追加 move-only 元素。
 */
struct VisualStateTransition {
    containers::InlineString from;  ///< 源状态名（"*" = 通配）
    containers::InlineString to;    ///< 目标状态名

    /// Storyboard 配置函数（可为空；空时立即跳变，不播放动画）
    /// 签名：void(animation::Storyboard& sb)
    /// SBO 32 字节约束：捕获列表不超过 4 个指针（64 位）
    core::Function<void(animation::Storyboard&)> configure;

    // move-only：configure 字段不可拷贝
    VisualStateTransition()                                      = default;
    VisualStateTransition(VisualStateTransition&&)               = default;
    VisualStateTransition& operator=(VisualStateTransition&&)    = default;
    VisualStateTransition(const VisualStateTransition&)          = delete;
    VisualStateTransition& operator=(const VisualStateTransition&) = delete;
};

}  // namespace mine::ui::style
```

### VisualStateManager（视觉状态管理器）

```cpp
namespace mine::ui::style {

/**
 * @brief 控件视觉状态管理器。
 *
 * 典型用法（由 mmlc 生成的 ControlTemplate::BuildFn 调用）：
 * @code
 *   VisualStateManager vsm{button};        // 绑定宿主控件
 *   vsm.define_state("Normal");
 *   vsm.define_state("Hovered");
 *   vsm.define_state("Pressed");
 *   vsm.add_transition("*", "Hovered");   // 任意 → Hovered
 *   vsm.add_transition("*", "Pressed");   // 任意 → Pressed
 *   vsm.set_style(&my_style);             // 关联样式（状态 setter）
 *   button.set_visual_state_manager(std::move(vsm));
 *
 *   // 控件事件中
 *   button.vsm()->go_to_state("Hovered"); // 切换状态并应用 setter
 * @endcode
 */
class MINE_UI_STYLE_API VisualStateManager {
public:
    // ── 生命周期 ──────────────────────────────────────────────────────────

    /**
     * @brief 构造 VSM，绑定宿主控件（存储为指针以支持移动语义）。
     *
     * @param owner 宿主依赖对象（通常为 Control 实例，转换为 DependencyObject&）
     */
    explicit VisualStateManager(ui::DependencyObject& owner) noexcept;

    ~VisualStateManager() = default;

    /// 不可拷贝：持有对宿主的引用，拷贝语义不安全
    VisualStateManager(const VisualStateManager&)            = delete;
    VisualStateManager& operator=(const VisualStateManager&) = delete;

    /// 可移动：内部使用指针存储 owner，移动时 owner 指针随之转移
    VisualStateManager(VisualStateManager&&) noexcept            = default;
    VisualStateManager& operator=(VisualStateManager&&) noexcept = default;

    // ── 状态注册 ──────────────────────────────────────────────────────────

    /**
     * @brief 注册一个视觉状态名。
     *
     * 重复注册同名状态为空操作。
     *
     * @param name 状态名（如 "Normal"、"Hovered"、"Pressed"）
     */
    void define_state(core::StringView name) noexcept;

    /**
     * @brief 注册状态过渡（无动画，立即跳变）。
     *
     * @param from 源状态名（"*" 表示从任意状态触发过渡）
     * @param to   目标状态名（须已通过 define_state 注册）
     */
    void add_transition(core::StringView from, core::StringView to) noexcept;

    /**
     * @brief 注册状态过渡（附带 Storyboard 动画配置）。
     *
     * go_to_state() 触发此过渡时，将：
     *   1. 调用 configure_fn(sb) 向 sb 注册要驱动的属性及参数
     *   2. 采样 from 值（capture_from_values）
     *   3. 应用新状态样式（StyleTrigger 写入）
     *   4. 解析 to 值并开始动画（resolve_and_begin）
     *   5. 将 Storyboard 加入 active_storyboards_，由 tick_animations 逐帧推进
     *
     * @param from         源状态名（"*" 表示从任意状态触发）
     * @param to           目标状态名
     * @param configure_fn Storyboard 配置函数（rvalue，move 进过渡记录）
     *
     * @note configure_fn 的捕获列表须满足 SBO 32 字节约束（最多 4 个指针）。
     */
    void add_transition(core::StringView from,
                        core::StringView to,
                        core::Function<void(animation::Storyboard&)> configure_fn) noexcept;

    // ── 状态切换 ──────────────────────────────────────────────────────────

    /**
     * @brief 切换到指定视觉状态。
     *
     * 流程（有配置函数的过渡，use_transitions=true）：
     *   1. 若 state_name 未注册，返回 false（无副作用）
     *   2. 若 state_name 已是当前状态，返回 false
     *   3. 停止并清理当前所有活跃 Storyboard（中断未完成动画）
     *   4. 查找匹配的过渡（精确 from → 通配 "*"），取第一个有 configure 的过渡
     *      - 若找到带动画的过渡：创建 Storyboard，调用 configure，
     *        capture_from_values，apply_state，resolve_and_begin，
     *        加入 active_storyboards_
     *      - 若未找到或 use_transitions=false：直接 apply_state（立即跳变）
     *   5. 更新 current_state_ = state_name
     *   6. 返回 true
     *
     * @param state_name      目标状态名
     * @param use_transitions 是否使用过渡动画（false 时立即跳变，跳过 Storyboard）
     * @return true  = 状态实际切换成功
     * @return false = 目标状态未注册 / 已是当前状态
     */
    bool go_to_state(core::StringView state_name,
                     bool             use_transitions = true) noexcept;

    /**
     * @brief 推进所有活跃 Storyboard 一帧。
     *
     * 完成的 Storyboard 将自动调用 stop()（清除 Animation 优先级）并从列表移除。
     * 建议由渲染循环或更新循环每帧调用。
     *
     * @param dt 本帧时间步长（秒），须 >= 0
     * @return true  = 仍有活跃 Storyboard 正在运行
     * @return false = 无活跃 Storyboard（所有动画已完成或未开始）
     */
    bool tick_animations(float dt) noexcept;

    /**
     * @brief 立即停止并清理所有活跃 Storyboard。
     *
     * 每个 Storyboard 调用 stop() 清除其受管属性的 Animation(P60) 槽，
     * 使属性值回退到下一级优先级（通常为 Local 或 StyleTrigger）。
     * 用于多状态组场景中，一个状态组切换时主动终止另一组的进行中动画。
     */
    void stop_all_storyboards() noexcept;

    // ── 查询 ──────────────────────────────────────────────────────────────

    /**
     * @brief 返回当前视觉状态名（初始为空字符串，切换后为最近一次成功切换的状态）。
     */
    [[nodiscard]] core::StringView current_state() const noexcept;

    /**
     * @brief 检查指定状态名是否已注册。
     */
    [[nodiscard]] bool has_state(core::StringView name) const noexcept;

    /**
     * @brief 检查从 from 到 to 是否存在已注册的过渡（含通配符 "*" 匹配）。
     */
    [[nodiscard]] bool has_transition(core::StringView from,
                                      core::StringView to) const noexcept;

    // ── 关联样式 ──────────────────────────────────────────────────────────

    /**
     * @brief 关联样式对象，供 go_to_state() 应用状态 setter。
     *
     * 设置为 nullptr 时，go_to_state() 仅更新 current_state_，
     * 不调用任何样式 setter。
     *
     * @param style 样式对象指针（非拥有；调用方负责生命周期）
     */
    void set_style(const Style* style) noexcept;

    /**
     * @brief 返回当前关联的样式对象（可为 nullptr）。
     */
    [[nodiscard]] const Style* attached_style() const noexcept;

private:
    /// 宿主控件（存为指针，支持移动；初始化后不应为 nullptr）
    ui::DependencyObject* owner_{nullptr};

    /// 关联样式（可选；nullptr = 不应用状态 setter）
    const Style* style_{nullptr};

    /// 已注册的状态名列表（define_state 时追加）
    containers::SmallVector<containers::InlineString, 8> states_;

    /// 已注册的过渡列表（add_transition 时追加；VisualStateTransition 为 move-only）
    containers::SmallVector<VisualStateTransition, 16> transitions_;

    /// 当前状态名（初始为空；go_to_state 成功后更新）
    containers::InlineString current_state_;

    /// 最近一次通过即时路径（无动画）以 Animation(P60) 写入的状态名。
    /// 用于下次 go_to_state 时清理该状态遗留的 P60 槽：
    ///   - Disabled → Normal（动画路径）：先读 P60 作为 from，再清 Disabled 的 P60
    ///   - Disabled → Disabled（同状态，被过滤）：无需清理
    ///   - 任意 → 任意（即时路径）：先清上一 instant P60，再写新 instant P60
    containers::InlineString instant_p60_state_;

    /// 当前活跃的 Storyboard 列表（go_to_state 创建，tick_animations 推进，完成后移除）
    /// 使用 OwnedPtr 因 Storyboard 含 SmallVector，为 move-only 类型
    containers::SmallVector<core::OwnedPtr<animation::Storyboard>, 4> active_storyboards_;
};

}  // namespace mine::ui::style
```

---

## 成员方法详解

### 生命周期

#### VisualStateManager(DependencyObject& owner)

**功能**：构造 VSM 实例，绑定宿主控件（存储为指针，支持移动语义）。

**参数**：
- `owner`：宿主依赖对象（通常为 `Control` 实例，转型为 `DependencyObject&`）

**注意事项**：
- 构造后 `owner_` 指针不应为 `nullptr`（由调用方保证传入有效引用）
- 使用指针存储 `owner` 而非引用，使 VSM 可移动（移动构造/赋值时指针随之转移）

**示例**：
```cpp
Button button;
VisualStateManager vsm{button};  // 绑定到 button
button.set_visual_state_manager(std::move(vsm));
```

---

### 状态注册

#### define_state(StringView name)

**功能**：注册一个视觉状态名。重复注册同名状态为空操作。

**参数**：
- `name`：状态名（如 `"Normal"`、`"Hovered"`、`"Pressed"`）

**实现细节**：
- 将状态名追加到 `states_` 向量（`SmallVector<InlineString, 8>`）
- 重复调用不会产生副作用（但会浪费查找时间，建议避免）

**示例**：
```cpp
vsm.define_state("Normal");
vsm.define_state("Hovered");
vsm.define_state("Pressed");
vsm.define_state("Disabled");
vsm.define_state("Focused");
```

---

#### add_transition(StringView from, StringView to)

**功能**：注册无动画的状态过渡（立即跳变）。

**参数**：
- `from`：源状态名（`"*"` 表示从任意状态触发）
- `to`：目标状态名（须已通过 `define_state` 注册）

**实现细节**：
- 创建 `VisualStateTransition{from, to, {}}` 并 move 进 `transitions_` 向量
- `configure` 为空时，`go_to_state()` 直接调用 `apply_state()`，无动画

**示例**：
```cpp
vsm.add_transition("*", "Disabled");  // 从任意状态可立即跳转到 Disabled
vsm.add_transition("Disabled", "Normal");  // Disabled → Normal 立即跳变
```

---

#### add_transition(StringView from, StringView to, Function<void(Storyboard&)> configure_fn)

**功能**：注册带 Storyboard 动画配置的状态过渡。

**参数**：
- `from`：源状态名（`"*"` 表示从任意状态触发）
- `to`：目标状态名
- `configure_fn`：Storyboard 配置函数（rvalue，move 进过渡记录）

**配置函数签名**：
```cpp
void(animation::Storyboard& sb)
```

**配置函数内部操作**：
```cpp
// 典型 configure_fn 内容：
[&](animation::Storyboard& sb) {
    sb.add_animation(ButtonProperty::BackgroundColor, 0.2f, EasingFunction::CubicInOut);
    sb.add_animation(ButtonProperty::Opacity, 0.15f, EasingFunction::Linear);
};
```

**SBO 约束**：
- `core::Function<T>` 使用 32 字节 SBO（Small Buffer Optimization）
- 捕获列表不超过 4 个指针（64 位平台），否则堆分配（性能下降）

**动画执行流程**（`go_to_state` 触发时）：
1. 调用 `configure_fn(sb)` 注册属性及参数
2. 采样 from 值（`capture_from_values()`）
3. 应用目标状态样式（`apply_state()`，写入 P30 StyleTrigger）
4. 解析 to 值并开始动画（`resolve_and_begin()`，写入 P60 Animation）
5. 将 Storyboard 加入 `active_storyboards_`

**示例**：
```cpp
vsm.add_transition("Normal", "Hovered", [&](animation::Storyboard& sb) {
    sb.add_animation(ButtonProperty::BackgroundColor, 0.2f, EasingFunction::CubicInOut);
    sb.add_animation(BorderProperty::BorderThickness, 0.15f);
});

vsm.add_transition("Hovered", "Pressed", [&](animation::Storyboard& sb) {
    sb.add_animation(ButtonProperty::Opacity, 0.1f, EasingFunction::QuadOut);
});
```

---

### 状态切换

#### go_to_state(StringView state_name, bool use_transitions)

**功能**：切换到指定视觉状态，可选择启用过渡动画。

**参数**：
- `state_name`：目标状态名
- `use_transitions`：是否使用过渡动画（默认 `true`；`false` 时立即跳变）

**返回值**：
- `true`：状态实际切换成功
- `false`：目标状态未注册 / 已是当前状态

**完整流程（`use_transitions=true` 且找到带动画过渡）**：
```
1. 验证 state_name 在 states_ 中（未注册返回 false）
2. 检查 state_name 是否等于 current_state_（相同返回 false）
3. 停止并清理所有活跃 Storyboard（调用 stop_all_storyboards()）
4. 查找匹配过渡（精确匹配优先，通配符 "*" 次之）：
   a. 遍历 transitions_，匹配 from == current_state_ && to == state_name
   b. 若无精确匹配，查找 from == "*" && to == state_name
   c. 取第一个 configure 非空的过渡
5. 若找到带动画过渡：
   a. 创建 Storyboard(owner_)
   b. 调用 configure_fn(sb)
   c. sb.capture_from_values()
   d. apply_state_to_owner(state_name)  // 写入 P30
   e. sb.resolve_and_begin()            // 写入 P60
   f. active_storyboards_.push_back(move(sb))
6. 若未找到或 use_transitions=false：
   a. 直接调用 apply_state_to_owner(state_name)（立即跳变）
7. 更新 current_state_ = state_name
8. 返回 true
```

**apply_state_to_owner 内部逻辑**：
```cpp
if (style_) {
    style_->apply_state(owner_, state_name);  // 调用 Style::apply_state()
}
// apply_state 遍历状态 setter 集合，逐个写入 P30 StyleTrigger
```

**示例**：
```cpp
// 带动画切换
vsm.go_to_state("Hovered", true);   // 播放 Normal → Hovered 动画

// 立即跳变
vsm.go_to_state("Disabled", false);  // 直接应用 Disabled setter，无动画

// 返回值检查
if (!vsm.go_to_state("InvalidState", true)) {
    // 处理切换失败（状态未注册）
}
```

---

#### tick_animations(float dt)

**功能**：推进所有活跃 Storyboard 一帧，完成的动画自动清理。

**参数**：
- `dt`：本帧时间步长（秒），须 `>= 0`

**返回值**：
- `true`：仍有活跃 Storyboard 正在运行
- `false`：无活跃 Storyboard（所有动画已完成或未开始）

**实现逻辑**：
```cpp
bool has_active = false;
for (auto it = active_storyboards_.begin(); it != active_storyboards_.end();) {
    auto& sb = *it;
    sb->tick(dt);  // 推进一帧（内部更新 P60 插值）
    if (sb->is_finished()) {
        sb->stop();  // 清除 P60 槽，属性回退到 P30
        it = active_storyboards_.erase(it);  // 移除完成的 Storyboard
    } else {
        has_active = true;
        ++it;
    }
}
return has_active;
```

**调用时机**：
- 渲染循环每帧调用：`vsm->tick_animations(delta_time)`
- 更新循环（独立于渲染）：`vsm->tick_animations(fixed_dt)`

**示例**：
```cpp
// 渲染循环中
void Button::on_render(float dt) {
    if (auto* vsm = visual_state_manager()) {
        if (vsm->tick_animations(dt)) {
            request_repaint();  // 仍有动画运行，请求下一帧重绘
        }
    }
    // ... 渲染逻辑
}
```

---

#### stop_all_storyboards()

**功能**：立即停止并清理所有活跃 Storyboard。

**实现逻辑**：
```cpp
for (auto& sb : active_storyboards_) {
    sb->stop();  // 清除受管属性的 P60 Animation 槽
}
active_storyboards_.clear();  // 清空列表
```

**用途**：
- 多状态组场景：一个状态组切换时，主动终止另一组的进行中动画
- 状态立即跳变：`go_to_state()` 内部在切换前调用此方法

**示例**：
```cpp
// 场景：按钮有两个独立状态组（MouseState + FocusState）
// MouseState 切换时，终止 FocusState 的进行中动画
vsm.stop_all_storyboards();
vsm.go_to_state("Hovered", true);  // 重新开始新动画
```

---

### 查询

#### current_state()

**功能**：返回当前视觉状态名（初始为空字符串，切换后为最近一次成功切换的状态）。

**返回值**：
- `StringView`：当前状态名（可能为空）

**示例**：
```cpp
if (vsm.current_state() == "Hovered") {
    // 当前处于 Hovered 状态
}
```

---

#### has_state(StringView name)

**功能**：检查指定状态名是否已注册。

**参数**：
- `name`：待检查的状态名

**返回值**：
- `true`：状态已通过 `define_state()` 注册
- `false`：状态未注册

**示例**：
```cpp
if (vsm.has_state("Pressed")) {
    vsm.go_to_state("Pressed");
}
```

---

#### has_transition(StringView from, StringView to)

**功能**：检查从 `from` 到 `to` 是否存在已注册的过渡（含通配符 `"*"` 匹配）。

**参数**：
- `from`：源状态名
- `to`：目标状态名

**返回值**：
- `true`：存在精确匹配 `from → to` 或通配符 `"*" → to`
- `false`：无匹配过渡

**示例**：
```cpp
if (vsm.has_transition("Normal", "Hovered")) {
    // 存在 Normal → Hovered 过渡
}

if (vsm.has_transition("AnyState", "Disabled")) {
    // 若注册了 "*" → "Disabled"，返回 true
}
```

---

### 关联样式

#### set_style(const Style* style)

**功能**：关联样式对象，供 `go_to_state()` 应用状态 setter。

**参数**：
- `style`：样式对象指针（非拥有；传 `nullptr` 解除关联）

**注意事项**：
- 调用方负责样式对象的生命周期（VSM 不持有所有权）
- 设置为 `nullptr` 时，`go_to_state()` 仅更新 `current_state_`，不调用 `apply_state()`

**示例**：
```cpp
Style my_style;
my_style.add_state_setter("Hovered", ButtonProperty::BackgroundColor, Color::LightBlue);

vsm.set_style(&my_style);  // 关联样式
vsm.go_to_state("Hovered");  // 应用 Hovered 状态 setter
```

---

#### attached_style()

**功能**：返回当前关联的样式对象（可为 `nullptr`）。

**返回值**：
- `const Style*`：关联的样式对象指针

**示例**：
```cpp
if (auto* style = vsm.attached_style()) {
    // 检查样式是否包含特定状态
    if (style->has_state("Pressed")) {
        // ...
    }
}
```

---

## 使用场景

### 场景 1：基础状态切换（无动画）

**需求**：按钮支持 Normal、Hovered、Pressed 三种状态，点击时立即切换到 Pressed。

```cpp
// 初始化 VSM
VisualStateManager vsm{button};
vsm.define_state("Normal");
vsm.define_state("Hovered");
vsm.define_state("Pressed");

// 注册立即跳变过渡
vsm.add_transition("*", "Normal");
vsm.add_transition("*", "Hovered");
vsm.add_transition("*", "Pressed");

// 关联样式
Style style;
style.add_state_setter("Normal", ButtonProperty::BackgroundColor, Color::Gray);
style.add_state_setter("Hovered", ButtonProperty::BackgroundColor, Color::LightGray);
style.add_state_setter("Pressed", ButtonProperty::BackgroundColor, Color::DarkGray);
vsm.set_style(&style);

// 事件驱动状态切换
button.on_mouse_enter([&]() { vsm.go_to_state("Hovered", false); });
button.on_mouse_leave([&]() { vsm.go_to_state("Normal", false); });
button.on_mouse_down([&]() { vsm.go_to_state("Pressed", false); });
```

---

### 场景 2：通配符过渡（简化配置）

**需求**：所有状态都可切换到 Disabled，且 Disabled 切回 Normal 时立即跳变。

```cpp
vsm.define_state("Normal");
vsm.define_state("Hovered");
vsm.define_state("Pressed");
vsm.define_state("Disabled");

// 通配符：从任意状态可切换到 Disabled
vsm.add_transition("*", "Disabled");

// Disabled → Normal 立即恢复
vsm.add_transition("Disabled", "Normal");

// 禁用/启用按钮
button.on_enabled_changed([&](bool enabled) {
    vsm.go_to_state(enabled ? "Normal" : "Disabled", false);
});
```

---

### 场景 3：动画过渡（平滑淡入淡出）

**需求**：Normal ↔ Hovered 切换时背景色平滑过渡 0.2 秒。

```cpp
vsm.define_state("Normal");
vsm.define_state("Hovered");

// Normal → Hovered：背景色淡入动画
vsm.add_transition("Normal", "Hovered", [&](animation::Storyboard& sb) {
    sb.add_animation(ButtonProperty::BackgroundColor, 0.2f, EasingFunction::CubicInOut);
    sb.add_animation(ButtonProperty::Opacity, 0.2f);
});

// Hovered → Normal：背景色淡出动画
vsm.add_transition("Hovered", "Normal", [&](animation::Storyboard& sb) {
    sb.add_animation(ButtonProperty::BackgroundColor, 0.2f, EasingFunction::CubicInOut);
});

// 渲染循环推进动画
void Button::on_render(float dt) {
    if (vsm->tick_animations(dt)) {
        request_repaint();
    }
}
```

---

### 场景 4：多状态组（MouseState + FocusState）

**需求**：按钮同时支持鼠标状态（Normal/Hovered/Pressed）和焦点状态（Focused/Unfocused）。

**方案 1：单 VSM 实例（状态名组合）**

```cpp
// 状态名组合：MouseState_FocusState
vsm.define_state("Normal_Unfocused");
vsm.define_state("Normal_Focused");
vsm.define_state("Hovered_Unfocused");
vsm.define_state("Hovered_Focused");
vsm.define_state("Pressed_Unfocused");
vsm.define_state("Pressed_Focused");

// 事件驱动
button.on_mouse_enter([&]() {
    auto focus = button.is_focused() ? "Focused" : "Unfocused";
    vsm.go_to_state("Hovered_" + focus);
});

button.on_focus_changed([&](bool focused) {
    auto mouse = vsm.current_state().starts_with("Hovered") ? "Hovered" : "Normal";
    vsm.go_to_state(mouse + (focused ? "_Focused" : "_Unfocused"));
});
```

**方案 2：双 VSM 实例（独立管理）**

```cpp
// MouseState VSM
VisualStateManager mouse_vsm{button};
mouse_vsm.define_state("Normal");
mouse_vsm.define_state("Hovered");
mouse_vsm.define_state("Pressed");

// FocusState VSM
VisualStateManager focus_vsm{button};
focus_vsm.define_state("Unfocused");
focus_vsm.define_state("Focused");

// 独立切换
button.on_mouse_enter([&]() { mouse_vsm.go_to_state("Hovered"); });
button.on_focus_changed([&](bool f) { focus_vsm.go_to_state(f ? "Focused" : "Unfocused"); });
```

---

### 场景 5：即时状态查询（条件渲染）

**需求**：根据当前状态决定是否渲染阴影效果。

```cpp
void Button::on_paint(PaintContext& ctx) {
    // 仅在 Hovered/Pressed 状态渲染阴影
    if (vsm->current_state() == "Hovered" || vsm->current_state() == "Pressed") {
        ctx.draw_shadow(bounds(), shadow_color_);
    }
    
    // 渲染按钮背景（由 Style 状态 setter 控制颜色）
    ctx.draw_rect(bounds(), background());
}
```

---

### 场景 6：状态过渡检查（防止无效切换）

**需求**：仅允许从 Normal 切换到 Hovered，禁止从 Disabled 直接切换。

```cpp
void Button::on_mouse_enter() {
    if (vsm->has_transition(vsm->current_state(), "Hovered")) {
        vsm->go_to_state("Hovered");
    } else {
        // 当前状态不允许切换到 Hovered（如 Disabled → Hovered 未注册）
        log_warning("Cannot transition to Hovered from " + vsm->current_state());
    }
}
```

---

### 场景 7：样式关联与动态切换

**需求**：运行时切换主题时，重新关联样式并刷新状态。

```cpp
void Button::apply_theme(const Theme& theme) {
    // 切换到新主题样式
    auto* new_style = theme.get_button_style();
    vsm->set_style(new_style);
    
    // 重新应用当前状态（刷新样式 setter）
    auto current = vsm->current_state();
    if (!current.empty()) {
        vsm->go_to_state(current, false);  // 立即应用新样式，无动画
    }
}
```

---

## 最佳实践

### ✅ 1. 使用通配符简化过渡配置

**推荐**：
```cpp
// 所有状态均可切换到 Disabled（无动画）
vsm.add_transition("*", "Disabled");
```

**不推荐**：
```cpp
// ❌ 为每个源状态单独注册过渡（冗余）
vsm.add_transition("Normal", "Disabled");
vsm.add_transition("Hovered", "Disabled");
vsm.add_transition("Pressed", "Disabled");
vsm.add_transition("Focused", "Disabled");
```

---

### ✅ 2. 动画配置捕获列表控制在 SBO 范围内

**推荐**：
```cpp
// 捕获列表 <= 4 个指针（64 位），满足 32 字节 SBO
vsm.add_transition("Normal", "Hovered", [&](animation::Storyboard& sb) {
    sb.add_animation(ButtonProperty::BackgroundColor, 0.2f);
});
```

**不推荐**：
```cpp
// ❌ 捕获大量变量，超出 SBO 限制，导致堆分配
auto data1 = ...;
auto data2 = ...;
auto data3 = ...;
auto data4 = ...;
auto data5 = ...;  // 超过 SBO 限制
vsm.add_transition("Normal", "Hovered", [&, data1, data2, data3, data4, data5](auto& sb) {
    // 堆分配，性能下降
});
```

---

### ✅ 3. 在渲染循环每帧调用 tick_animations

**推荐**：
```cpp
void Button::on_render(float dt) {
    if (auto* vsm = visual_state_manager()) {
        if (vsm->tick_animations(dt)) {
            request_repaint();  // 仍有动画运行，请求下一帧
        }
    }
}
```

**不推荐**：
```cpp
// ❌ 仅在状态切换时调用 tick_animations（动画卡顿）
vsm.go_to_state("Hovered");
vsm.tick_animations(0.016f);  // 仅推进一帧，后续无驱动
```

---

### ✅ 4. 状态切换失败时检查返回值

**推荐**：
```cpp
if (!vsm.go_to_state("Pressed")) {
    // 处理切换失败（状态未注册 / 已是当前状态）
    log_warning("Failed to switch to Pressed state");
}
```

**不推荐**：
```cpp
// ❌ 忽略返回值，切换失败时无感知
vsm.go_to_state("InvalidState");  // 静默失败
```

---

### ✅ 5. 样式与 VSM 生命周期管理

**推荐**：
```cpp
class Button : public Control {
    Style button_style_;         // 样式对象拥有权
    VisualStateManager vsm_;     // VSM 拥有权
    
    Button() : vsm_{*this} {
        vsm_.set_style(&button_style_);  // VSM 弱引用样式
    }
};
```

**不推荐**：
```cpp
// ❌ 样式对象提前销毁，VSM 持有悬空指针
void init_button(Button& button) {
    Style temp_style;
    temp_style.add_state_setter(...);
    button.vsm()->set_style(&temp_style);  // temp_style 离开作用域后失效
}
```

---

## 常见陷阱

### ❌ 1. 忘记调用 tick_animations 导致动画卡死

**错误**：
```cpp
vsm.add_transition("Normal", "Hovered", [&](auto& sb) {
    sb.add_animation(ButtonProperty::Opacity, 0.2f);
});

vsm.go_to_state("Hovered");  // 动画开始，但无驱动
// ❌ 渲染循环未调用 tick_animations，Opacity 停留在起点值
```

**正确**：
```cpp
void Button::on_render(float dt) {
    vsm->tick_animations(dt);  // 每帧推进动画
}
```

---

### ❌ 2. 未注册状态直接调用 go_to_state

**错误**：
```cpp
VisualStateManager vsm{button};
vsm.define_state("Normal");
vsm.go_to_state("Hovered");  // ❌ "Hovered" 未注册，返回 false
```

**正确**：
```cpp
vsm.define_state("Normal");
vsm.define_state("Hovered");  // 先注册状态
vsm.go_to_state("Hovered");   // ✅ 切换成功
```

---

### ❌ 3. 动画配置函数捕获列表过大触发堆分配

**错误**：
```cpp
struct LargeStruct {
    std::array<int, 100> data;
};

LargeStruct large;
vsm.add_transition("Normal", "Hovered", [large](auto& sb) {
    // ❌ 捕获 large（400 字节），超出 32 字节 SBO，触发堆分配
    sb.add_animation(ButtonProperty::Opacity, 0.2f);
});
```

**正确**：
```cpp
LargeStruct* large_ptr = &large;
vsm.add_transition("Normal", "Hovered", [large_ptr](auto& sb) {
    // ✅ 仅捕获指针（8 字节），满足 SBO
    sb.add_animation(ButtonProperty::Opacity, 0.2f);
});
```

---

### ❌ 4. 多状态组场景下活跃动画冲突

**错误**：
```cpp
// MouseState 和 FocusState 使用同一 VSM 实例
vsm.go_to_state("Hovered");   // 开始 Hovered 动画
vsm.go_to_state("Focused");   // ❌ 终止 Hovered 动画，仅保留 Focused 动画
```

**正确方案 1**：状态名组合
```cpp
vsm.define_state("Hovered_Focused");
vsm.go_to_state("Hovered_Focused");  // 单一状态，无冲突
```

**正确方案 2**：双 VSM 实例
```cpp
VisualStateManager mouse_vsm{button};
VisualStateManager focus_vsm{button};

mouse_vsm.go_to_state("Hovered");  // 独立管理，互不干扰
focus_vsm.go_to_state("Focused");
```

---

### ❌ 5. 样式对象生命周期短于 VSM

**错误**：
```cpp
void setup_button(Button& button) {
    Style temp_style;
    temp_style.add_state_setter("Hovered", ButtonProperty::BackgroundColor, Color::Blue);
    button.vsm()->set_style(&temp_style);  // ❌ temp_style 离开作用域后失效
}

// 后续调用 go_to_state 时，style_ 为悬空指针
button.vsm()->go_to_state("Hovered");  // ⚠️ UB
```

**正确**：
```cpp
class Button : public Control {
    Style button_style_;  // 样式对象与 Button 生命周期一致
    VisualStateManager vsm_;
    
    Button() : vsm_{*this} {
        button_style_.add_state_setter("Hovered", ButtonProperty::BackgroundColor, Color::Blue);
        vsm_.set_style(&button_style_);  // ✅ 样式对象持续有效
    }
};
```

---

### ❌ 6. 过渡配置中调用阻塞操作

**错误**：
```cpp
vsm.add_transition("Normal", "Hovered", [&](auto& sb) {
    std::this_thread::sleep_for(100ms);  // ❌ 阻塞 UI 线程
    sb.add_animation(ButtonProperty::Opacity, 0.2f);
});
```

**正确**：
```cpp
vsm.add_transition("Normal", "Hovered", [&](auto& sb) {
    // ✅ 仅配置动画参数，无阻塞操作
    sb.add_animation(ButtonProperty::Opacity, 0.2f);
    sb.set_duration(0.2f);
});
```

---

## 完整示例：按钮状态机

### 场景描述

实现一个完整的按钮控件视觉状态机，支持以下特性：
- **5 种状态**：Normal、Hovered、Pressed、Focused、Disabled
- **动画过渡**：Normal ↔ Hovered、Hovered ↔ Pressed（背景色 + 不透明度）
- **立即跳变**：Disabled ↔ Normal（无动画）
- **渲染驱动**：每帧调用 `tick_animations(dt)` 推进动画
- **事件响应**：鼠标进入/离开、按下/释放、焦点变化、启用/禁用

### 完整代码（~400 行）

```cpp
// Button.h
#pragma once

#include <mine/ui/controls/Control.h>
#include <mine/ui/style/VisualStateManager.h>
#include <mine/ui/style/Style.h>
#include <mine/ui/animation/Storyboard.h>

namespace mine::ui::controls {

class Button : public Control {
public:
    Button();
    ~Button() override = default;

    // 启用/禁用按钮
    void set_enabled(bool enabled);
    [[nodiscard]] bool is_enabled() const noexcept { return enabled_; }

protected:
    // 事件处理
    void on_mouse_enter(const input::MouseEventArgs& e) override;
    void on_mouse_leave(const input::MouseEventArgs& e) override;
    void on_mouse_down(const input::MouseButtonEventArgs& e) override;
    void on_mouse_up(const input::MouseButtonEventArgs& e) override;
    void on_focus_gained(const input::FocusEventArgs& e) override;
    void on_focus_lost(const input::FocusEventArgs& e) override;

    // 渲染
    void on_render(PaintContext& ctx, float dt) override;

private:
    void setup_visual_state_manager();
    void setup_button_style();
    void update_visual_state();

    bool enabled_{true};
    bool is_mouse_over_{false};
    bool is_pressed_{false};
    bool is_focused_{false};

    style::Style button_style_;
    style::VisualStateManager vsm_;
};

}  // namespace mine::ui::controls

// Button.cpp
#include "Button.h"

namespace mine::ui::controls {

Button::Button()
    : vsm_{*this}  // 绑定宿主控件
{
    setup_button_style();
    setup_visual_state_manager();
    
    // 初始状态
    vsm_.go_to_state("Normal", false);
}

void Button::setup_button_style() {
    // ── Normal 状态 ──
    button_style_.add_state_setter("Normal", 
        ButtonProperty::BackgroundColor, 
        Color::FromRgb(0xE0E0E0));
    button_style_.add_state_setter("Normal", 
        ButtonProperty::Opacity, 
        1.0f);
    button_style_.add_state_setter("Normal", 
        BorderProperty::BorderThickness, 
        Thickness{1});

    // ── Hovered 状态 ──
    button_style_.add_state_setter("Hovered", 
        ButtonProperty::BackgroundColor, 
        Color::FromRgb(0xD0D0D0));
    button_style_.add_state_setter("Hovered", 
        ButtonProperty::Opacity, 
        1.0f);
    button_style_.add_state_setter("Hovered", 
        BorderProperty::BorderThickness, 
        Thickness{2});

    // ── Pressed 状态 ──
    button_style_.add_state_setter("Pressed", 
        ButtonProperty::BackgroundColor, 
        Color::FromRgb(0xB0B0B0));
    button_style_.add_state_setter("Pressed", 
        ButtonProperty::Opacity, 
        0.9f);
    button_style_.add_state_setter("Pressed", 
        BorderProperty::BorderThickness, 
        Thickness{2});

    // ── Focused 状态 ──
    button_style_.add_state_setter("Focused", 
        BorderProperty::BorderColor, 
        Color::FromRgb(0x0078D7));  // 蓝色边框
    button_style_.add_state_setter("Focused", 
        BorderProperty::BorderThickness, 
        Thickness{2});

    // ── Disabled 状态 ──
    button_style_.add_state_setter("Disabled", 
        ButtonProperty::BackgroundColor, 
        Color::FromRgb(0xF0F0F0));
    button_style_.add_state_setter("Disabled", 
        ButtonProperty::Opacity, 
        0.5f);
    button_style_.add_state_setter("Disabled", 
        TextProperty::ForegroundColor, 
        Color::FromRgb(0x808080));  // 灰色文本
}

void Button::setup_visual_state_manager() {
    // ── 注册状态 ──
    vsm_.define_state("Normal");
    vsm_.define_state("Hovered");
    vsm_.define_state("Pressed");
    vsm_.define_state("Focused");
    vsm_.define_state("Disabled");

    // ── 注册过渡（Normal ↔ Hovered，带动画）──
    vsm_.add_transition("Normal", "Hovered", [this](animation::Storyboard& sb) {
        // 背景色淡入动画（0.2 秒）
        sb.add_animation(ButtonProperty::BackgroundColor, 
                         0.2f, 
                         animation::EasingFunction::CubicInOut);
        
        // 边框厚度动画（0.15 秒）
        sb.add_animation(BorderProperty::BorderThickness, 
                         0.15f, 
                         animation::EasingFunction::QuadOut);
    });

    vsm_.add_transition("Hovered", "Normal", [this](animation::Storyboard& sb) {
        // 背景色淡出动画（0.2 秒）
        sb.add_animation(ButtonProperty::BackgroundColor, 
                         0.2f, 
                         animation::EasingFunction::CubicInOut);
        
        // 边框厚度动画（0.15 秒）
        sb.add_animation(BorderProperty::BorderThickness, 
                         0.15f, 
                         animation::EasingFunction::QuadOut);
    });

    // ── 注册过渡（Hovered ↔ Pressed，带动画）──
    vsm_.add_transition("Hovered", "Pressed", [this](animation::Storyboard& sb) {
        // 背景色快速变暗（0.1 秒）
        sb.add_animation(ButtonProperty::BackgroundColor, 
                         0.1f, 
                         animation::EasingFunction::QuadOut);
        
        // 不透明度降低（0.1 秒）
        sb.add_animation(ButtonProperty::Opacity, 
                         0.1f, 
                         animation::EasingFunction::Linear);
    });

    vsm_.add_transition("Pressed", "Hovered", [this](animation::Storyboard& sb) {
        // 背景色快速恢复（0.1 秒）
        sb.add_animation(ButtonProperty::BackgroundColor, 
                         0.1f, 
                         animation::EasingFunction::QuadOut);
        
        // 不透明度恢复（0.1 秒）
        sb.add_animation(ButtonProperty::Opacity, 
                         0.1f, 
                         animation::EasingFunction::Linear);
    });

    // ── 通配符过渡（任意状态 → Focused，无动画）──
    vsm_.add_transition("*", "Focused");

    // ── 通配符过渡（任意状态 → Disabled，立即跳变）──
    vsm_.add_transition("*", "Disabled");

    // ── Disabled → Normal（立即恢复）──
    vsm_.add_transition("Disabled", "Normal");

    // 关联样式
    vsm_.set_style(&button_style_);
}

void Button::update_visual_state() {
    if (!enabled_) {
        vsm_.go_to_state("Disabled", false);  // 立即跳变
        return;
    }

    if (is_pressed_) {
        vsm_.go_to_state("Pressed", true);   // 带动画
    } else if (is_mouse_over_) {
        vsm_.go_to_state("Hovered", true);   // 带动画
    } else if (is_focused_) {
        vsm_.go_to_state("Focused", false);  // 立即跳变
    } else {
        vsm_.go_to_state("Normal", true);    // 带动画
    }
}

void Button::set_enabled(bool enabled) {
    if (enabled_ == enabled) {
        return;
    }

    enabled_ = enabled;
    update_visual_state();
}

void Button::on_mouse_enter(const input::MouseEventArgs& e) {
    Control::on_mouse_enter(e);
    
    is_mouse_over_ = true;
    update_visual_state();
}

void Button::on_mouse_leave(const input::MouseEventArgs& e) {
    Control::on_mouse_leave(e);
    
    is_mouse_over_ = false;
    is_pressed_ = false;  // 离开时重置按下状态
    update_visual_state();
}

void Button::on_mouse_down(const input::MouseButtonEventArgs& e) {
    Control::on_mouse_down(e);
    
    if (e.button() == input::MouseButton::Left && enabled_) {
        is_pressed_ = true;
        update_visual_state();
        
        // 捕获鼠标，确保 mouse_up 在按钮外也能收到
        capture_mouse();
    }
}

void Button::on_mouse_up(const input::MouseButtonEventArgs& e) {
    Control::on_mouse_up(e);
    
    if (e.button() == input::MouseButton::Left && is_pressed_) {
        is_pressed_ = false;
        update_visual_state();
        
        // 释放鼠标捕获
        release_mouse_capture();
        
        // 若鼠标仍在按钮上，触发 Click 事件
        if (is_mouse_over_) {
            on_click();
        }
    }
}

void Button::on_focus_gained(const input::FocusEventArgs& e) {
    Control::on_focus_gained(e);
    
    is_focused_ = true;
    update_visual_state();
}

void Button::on_focus_lost(const input::FocusEventArgs& e) {
    Control::on_focus_lost(e);
    
    is_focused_ = false;
    update_visual_state();
}

void Button::on_render(PaintContext& ctx, float dt) {
    // 推进视觉状态动画
    if (vsm_.tick_animations(dt)) {
        request_repaint();  // 仍有动画运行，请求下一帧重绘
    }

    // 渲染按钮背景（由 Style 状态 setter 控制颜色和不透明度）
    auto bg_color = get_value<Color>(ButtonProperty::BackgroundColor);
    auto opacity = get_value<float>(ButtonProperty::Opacity);
    ctx.set_opacity(opacity);
    ctx.draw_rounded_rect(bounds(), 4.0f, bg_color);

    // 渲染边框（由 Style 状态 setter 控制厚度和颜色）
    auto border_color = get_value<Color>(BorderProperty::BorderColor);
    auto border_thickness = get_value<Thickness>(BorderProperty::BorderThickness);
    ctx.draw_rounded_rect_border(bounds(), 4.0f, border_thickness, border_color);

    // 渲染焦点指示器（Focused 状态）
    if (vsm_.current_state() == "Focused") {
        auto focus_rect = bounds().deflate(2.0f);
        ctx.draw_dashed_rect(focus_rect, Color::FromRgb(0x0078D7), 1.0f);
    }

    // 渲染按钮文本
    Control::on_render(ctx, dt);
}

}  // namespace mine::ui::controls

// 使用示例
int main() {
    using namespace mine::ui;

    // 创建按钮
    auto button = std::make_shared<controls::Button>();
    button->set_text("Click Me");
    button->set_size({120, 40});

    // 测试状态切换
    button->set_enabled(false);  // Disabled 状态（立即跳变）
    std::this_thread::sleep_for(1s);
    
    button->set_enabled(true);   // Normal 状态（立即恢复）
    
    // 模拟鼠标交互
    input::MouseEventArgs enter_args{};
    button->on_mouse_enter(enter_args);  // Normal → Hovered（动画过渡）
    
    // 渲染循环推进动画
    for (float t = 0.0f; t < 0.2f; t += 0.016f) {
        PaintContext ctx;
        button->on_render(ctx, 0.016f);  // 推进动画
    }

    input::MouseButtonEventArgs down_args{input::MouseButton::Left};
    button->on_mouse_down(down_args);   // Hovered → Pressed（动画过渡）
    
    for (float t = 0.0f; t < 0.1f; t += 0.016f) {
        PaintContext ctx;
        button->on_render(ctx, 0.016f);
    }

    input::MouseButtonEventArgs up_args{input::MouseButton::Left};
    button->on_mouse_up(up_args);       // Pressed → Hovered（动画过渡）
    
    for (float t = 0.0f; t < 0.1f; t += 0.016f) {
        PaintContext ctx;
        button->on_render(ctx, 0.016f);
    }

    input::MouseEventArgs leave_args{};
    button->on_mouse_leave(leave_args);  // Hovered → Normal（动画过渡）
    
    for (float t = 0.0f; t < 0.2f; t += 0.016f) {
        PaintContext ctx;
        button->on_render(ctx, 0.016f);
    }

    return 0;
}
```

### 运行效果

1. **初始状态**：Normal（灰色背景，边框厚度 1px）
2. **鼠标进入**：Normal → Hovered（背景色淡入浅灰，边框厚度 1px → 2px，动画 0.2 秒）
3. **鼠标按下**：Hovered → Pressed（背景色快速变暗，不透明度降低，动画 0.1 秒）
4. **鼠标释放**：Pressed → Hovered（背景色和不透明度恢复，动画 0.1 秒）
5. **鼠标离开**：Hovered → Normal（背景色淡出，边框厚度 2px → 1px，动画 0.2 秒）
6. **焦点获取**：Normal → Focused（蓝色边框，立即跳变）
7. **禁用按钮**：任意状态 → Disabled（浅灰色背景，不透明度 0.5，立即跳变）

### 关键实现细节

1. **动画驱动**：`on_render(ctx, dt)` 每帧调用 `vsm_.tick_animations(dt)`，推进所有活跃 Storyboard。
2. **状态优先级**：`update_visual_state()` 按优先级决定目标状态（Disabled > Pressed > Hovered > Focused > Normal）。
3. **通配符过渡**：`"*" → "Disabled"` 和 `"*" → "Focused"` 简化配置，避免重复注册。
4. **生命周期管理**：`button_style_` 与 `Button` 生命周期一致，`vsm_.set_style(&button_style_)` 安全无悬空指针。
5. **事件处理**：鼠标/焦点事件更新状态标志后调用 `update_visual_state()` 触发状态切换。

---

## 总结

`VisualStateManager` 是 MineUI 控件视觉状态管理的核心组件，通过状态注册、过渡配置和动画驱动，实现控件在不同交互状态之间的智能切换。其关键特性包括：

1. **状态机管理**：通过 `define_state()` 和 `go_to_state()` 实现声明式状态管理
2. **动画过渡**：支持立即跳变和 Storyboard 驱动的平滑过渡
3. **通配符支持**：简化过渡配置，避免重复注册
4. **与 Style 集成**：自动应用状态 setter，无需手动管理属性值
5. **生命周期管理**：`tick_animations(dt)` 每帧推进动画，完成后自动清理

正确使用 VSM 可显著简化控件状态管理逻辑，提升代码可读性和可维护性。关键注意事项：
- **每帧调用 `tick_animations(dt)`**：确保动画平滑运行
- **先注册状态再调用 `go_to_state()`**：避免切换失败
- **控制动画配置函数捕获列表大小**：满足 32 字节 SBO 约束
- **样式对象生命周期**：确保样式对象在 VSM 使用期间持续有效

遵循最佳实践和避开常见陷阱，可充分发挥 VSM 在视觉状态管理中的强大能力。
