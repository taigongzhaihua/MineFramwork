# 21 — 控件系统架构设计规范

> 本文档是 MineFramework 控件层（`mine.ui.controls`）的完整架构指南。  
> 控件系统是 **属性系统 / 样式系统 / 动画系统 / 事件系统 / 渲染系统 / 布局系统** 的集成点，  
> 本文统一定义各子系统的职责边界、数据存储规范、调用链路和开发接口规范。

---

## 21.1 设计目标与核心原则

### 目标

1. **各司其职**：每个系统有清晰的职责边界，控件只调用正确层次的 API。
2. **DependencyProperty 是唯一真相源**：所有可视属性（颜色、字体、边距等）必须经由 DP 系统读写，绝不允许绕过。
3. **控件不感知动画时序**：控件负责"声明动画意图"（启动 Storyboard），由框架层统一调度 tick。
4. **输入状态与视觉属性分离**：`is_pressed_`、`is_hovered_` 是输入层的纯 bool 标志；屏幕上的颜色、尺寸等视觉表现完全由 DP 决定。
5. **控件不感知渲染调度**：控件只调 `invalidate_render()`，不持有定时器，不调用 `render()`。

### 核心约束

| 约束 | 说明 |
|------|------|
| 禁止双写 | 不允许同时用 `background_`（plain member）和 `BackgroundProperty`（DP）独立表达同一视觉属性 |
| 禁止控件持有定时器 | 动画 tick 由 `AnimationClock`（F3）或窗口渲染调度（当前 F2 临时方案）统一管理 |
| 禁止控件调 `render()` | 控件只能调 `invalidate_render()` 通知失效，渲染由 UI 系统调度 |
| 禁止控件感知优先级细节 | `set_xxx()` 方法内部可调 `set_value(..., ValuePriority::Local)`，但不必关心 Animation / StyleTrigger 等优先级的存在 |
| 模板子元素通过 bind_template 同步 | 不允许在 `on_measure`/`on_render` 中手动 push 数据到子元素 |

---

## 21.2 七大子系统集成总图

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         mine.ui.controls                                │
│                                                                         │
│   Control (基类，来自 mine.ui.visual)                                   │
│   ┌──────────────────────────────────────────────────────────────┐      │
│   │  输入状态层（is_pressed_, is_hovered_, is_focused_...）       │      │
│   │    ← 由 mine.ui.input 路由事件驱动                            │      │
│   ├──────────────────────────────────────────────────────────────┤      │
│   │  视觉状态机（ControlVisualState + VisualStateManager）        │      │
│   │    ← compute_visual_state()  →  update_visual_state()        │      │
│   │    → on_visual_state_changed()  →  VSM.go_to_state()         │      │
│   ├──────────────────────────────────────────────────────────────┤      │
│   │  DependencyProperty 层（所有可视属性的单一真相源）            │      │
│   │    ← set_value(prop, val, priority)                           │      │
│   │    → get_value(prop)  ← on_render 读取                       │      │
│   ├──────────────────────────────────────────────────────────────┤      │
│   │  布局层（on_measure / on_arrange）                            │      │
│   │    ← mine.ui.layout 调度；委托给 template_root               │      │
│   ├──────────────────────────────────────────────────────────────┤      │
│   │  渲染层（on_render）                                          │      │
│   │    ← mine.ui.visual 调度；完全从 DP 读取视觉属性              │      │
│   └──────────────────────────────────────────────────────────────┘      │
└─────────────────────────────────────────────────────────────────────────┘
         │                │               │               │
         ▼                ▼               ▼               ▼
  mine.ui.input   mine.ui.style   mine.ui.animation   mine.paint
  (事件路由/     (Style/Template/   (Storyboard/        (Canvas API)
   命中测试)      VSM/资源字典)      EasingFunction)
```

---

## 21.3 子系统职责边界定义

### 21.3.1 属性系统（mine.ui.property）

**职责**：为任意可读写、可通知、可动画、可样式化的属性提供统一容器。

| 角色 | 说明 |
|------|------|
| `DependencyProperty` | 属性的元数据令牌（名称、所有者类型、默认值、影响标志、changed 回调） |
| `DependencyObject` | 持有 DP 存储槽（`ValueStore`），所有控件的最终基类 |
| `set_value(prop, val, priority)` | 向指定优先级槽写入值；写完后触发 changed 回调和 affect 标志 |
| `get_value(prop)` | 从优先级最高的有效槽返回值 |
| `clear_value(prop, priority)` | 清除指定优先级槽；值自动退回到下一级 |

**优先级链（高→低）**：

```
Animation(60) > Local(50) > TemplateBind(40) > StyleTrigger(30) > StyleSetter(20) > Inherited(10) > Default(0)
```

- `Local`：`set_xxx()` 系列公共 API 写入此级
- `StyleSetter`：`Style::apply()` 写入此级（F2 完成）
- `StyleTrigger`：`VSM::apply_state()` 写入此级（状态 setter，不经过动画）
- `Animation`：`Storyboard::tick()` 写入此级（动画帧间插值）

**控件使用规范**：

```cpp
// ✅ 正确：set_xxx 内部调 set_value
void Button::set_background(math::Color color) {
    background_cache_ = color;                            // 可选：更新缓存以便非-DP 路径查询
    set_value(BackgroundProperty,
              core::Variant{color},
              ValuePriority::Local);                      // DP 是真相源
}

// ✅ 正确：on_render 读 DP
math::Color fill = get_value(BackgroundProperty)
                       .get_or<math::Color>(math::Color::Transparent);

// ❌ 错误：绕过 DP 直接写 plain member，on_render 读 DP → 永远看到 Default 值
void Button::set_background(math::Color color) {
    background_ = color;      // 写了 plain member
    invalidate_render();      // 但 on_render 读的是 DP……紫色 Bug 的根源
}
```

---

### 21.3.2 样式 / 模板系统（mine.ui.style）

**职责**：提供控件视觉树结构（ControlTemplate）、属性默认外观（Style）和状态驱动动画（VisualStateManager）。

#### ControlTemplate 职责

| 阶段 | 时机 | 操作 |
|------|------|------|
| 注册 | 静态初始化 | `TemplateRegistry::register_template(name, type_id, build_fn)` |
| 构建 | 首次 `on_measure` | `Control::on_measure` 检测 `template_root_ == nullptr`，调用 `build_fn` |
| 绑定 | `build_fn` 内部 | `bind_template(child, child_prop, host_prop)` 建立 TemplateBind 优先级的单向同步 |
| 访问子元素 | 模板构建后 | `find_template_child("name")` |

```
build_fn 调用时机：首次 on_measure（懒构建，避免无用控件的开销）
bind_template 写入优先级：ValuePriority::TemplateBind (40)
```

#### VisualStateManager 职责

VSM 是**状态机 + 动画调度器**：

```
on_visual_state_changed(old, new)
    └─► vsm()->go_to_state(new_state_name)
            └─► 找到 new_state 的 Storyboard
            └─► stop 上一个 Storyboard（清除 Animation 槽，值退回 StyleTrigger/Local）
            └─► capture_from_values（读当前 DP 值作为动画起点）
            └─► resolve_and_begin（写 Animation 起始值，开始 tick）
```

VSM 管理 Storyboard 的全生命周期：启动、tick、停止、清理。  
**控件不应自己持有 Storyboard（F2 当前 Button 是临时过渡方案，F3 迁移到 VSM）。**

#### Style 职责

Style 在 Control 挂载或样式变更时调用 `apply()`，将 setter 以 `StyleSetter(20)` 写入 DP：

```
Style::apply(control)
    └─► 遍历 setters_：set_value(prop, val, ValuePriority::StyleSetter)
Style::apply_state(control, "Hovered")
    └─► 遍历 state_setters_["Hovered"]：set_value(prop, val, ValuePriority::StyleTrigger)
```

---

### 21.3.3 事件系统（mine.ui.event + mine.ui.input）

**职责**：将平台原始事件（Win32 WM_*）转换为可路由的强类型事件，并派发到视觉树。

#### 调用链路

```
平台窗口事件（WindowEvent）
    └─► InputRouter::dispatch_mouse_event
            └─► UIElement::hit_test(pos) → 目标元素
            └─► 检测 prev_mouse_over_ 切换 → 合成 MouseLeave/MouseEnter（Direct 策略）
            └─► EventManager::raise(target, args) → 路由到目标树
                    └─► 处理器回调（static router fn → 实例方法）
```

#### 控件事件处理规范

```cpp
// ① 构造函数：注册静态路由函数（捕获 this）
Button::Button() {
    add_handler(input::MouseDownEvent(),  &Button::on_mouse_down_router,  this);
    add_handler(input::MouseUpEvent(),    &Button::on_mouse_up_router,    this);
    add_handler(input::MouseEnterEvent(), &Button::on_mouse_enter_router, this);
    add_handler(input::MouseLeaveEvent(), &Button::on_mouse_leave_router, this);
}

// ② 静态路由函数：类型转换 + 委托（固定模式，无逻辑）
static void Button::on_mouse_down_router(void*, RoutedEventArgs& args, void* user_data) {
    static_cast<Button*>(user_data)->on_mouse_down(
        static_cast<input::MouseEventArgs&>(args));
}

// ③ 实例方法：更新输入状态标志，调用 update_visual_state()
void Button::on_mouse_down(input::MouseEventArgs& args) {
    if (!is_enabled_ || args.button() != input::MouseButton::Left) return;
    is_pressed_ = true;
    update_visual_state();    // ← 触发视觉状态机
}
```

**输入状态标志规范**（只允许 bool，不允许与视觉属性混用）：

| 标志 | 含义 | 更新时机 |
|------|------|----------|
| `is_pressed_` | 鼠标左键按下中 | MouseDown(true) / MouseUp、MouseLeave(false) |
| `is_hovered_` | 鼠标悬停在元素上 | MouseEnter(true) / MouseLeave(false) |
| `is_focused_` | 拥有键盘焦点 | GotFocus(true) / LostFocus(false) |
| `is_enabled_` | 控件可交互 | set_enabled() |

---

### 21.3.4 动画系统（mine.ui.animation）

**职责**：提供基于 DP 属性的时间插值能力（Storyboard + PropertyAnimation）。

#### Storyboard 生命周期

```
创建：make_owned<Storyboard>()
配置：sb->animate_dp_to(target, prop, to_val, duration, easing)
采样：sb->capture_from_values()    ← 读当前 DP 值作为 "from"
启动：sb->resolve_and_begin()      ← 写 Animation 起始值
驱动：sb->tick(dt)                 ← 每帧调用，写 Animation 插值值
完成：sb->is_complete() == true
停止：sb->stop()                   ← clear_value(Animation)，值退回 Local/StyleTrigger
```

#### 写入优先级

| 时机 | 写入优先级 | 效果 |
|------|-----------|------|
| `resolve_and_begin()` | Animation(60) | 动画期间遮盖 Local/StyleTrigger 的值 |
| `tick()` 每帧 | Animation(60) | 覆写插值中间值，on_render 读到实时颜色 |
| `stop()` 之后 | 无写入 | 值退回 Local/StyleTrigger（应为 to_color） |

#### F2 当前 Button 的临时方案 vs F3 目标

| 当次（F2 临时方案） | F3 目标 |
|---------|---------|
| Button 自持 `bg_storyboard_` | VSM 管理所有 Storyboard |
| App 调 `tick_bg_animation(dt)` | `AnimationClock` 在帧调度时统一 tick |
| `has_active_animation()` 暴露给 App | App 层不感知 Storyboard 状态 |

> **F3 迁移路径**：Button 删除 `bg_storyboard_`、`tick_bg_animation()`、`has_active_animation()`；  
> 在 `build_fn` 中配置 VSM（注册 Normal/Hovered/Pressed 状态及对应 Storyboard 描述）；  
> `on_visual_state_changed` 改为单行：`vsm()->go_to_state(compute_state_name())`。

---

### 21.3.5 渲染系统（mine.paint + mine.ui.visual）

**职责**：将 DP 所描述的视觉属性绘制到屏幕。

#### `on_render` 规范

```cpp
void MyControl::on_render(paint::Canvas& canvas) {
    const math::Rect rect = bounds_rect();
    if (rect.empty()) return;                   // ① 边界检查，空元素不绘制

    // ② 从 DP 读取所有视觉属性（不读 plain member）
    const math::Color bg = get_value(BackgroundProperty)
                               .get_or<math::Color>(math::Color::Transparent);
    const math::Color fg = get_value(ForegroundProperty)
                               .get_or<math::Color>(math::Color::Black);

    // ③ 使用 Canvas API 绘制（不访问 RHI）
    canvas.fill_rounded_rect(
        math::RoundedRect{rect, rect.height * 0.5f},
        paint::Brush::solid(bg));

    // ④ 如有子视觉效果（ripple 等）使用 canvas.save/restore 隔离状态
    if (ripple_.active) {
        canvas.save();
        canvas.clip_rounded_rect(math::RoundedRect{rect, rect.height * 0.5f});
        canvas.fill_circle(center, ripple_radius, paint::Brush::solid(ripple_color));
        canvas.restore();
    }
}
```

**`on_render` 禁止事项**：
- ❌ 不访问外部系统（InputRouter、Application 等）
- ❌ 不修改 DP 值（`on_render` 是只读的）
- ❌ 不调用 `invalidate_render()`（不需要，渲染系统会根据需要重新调度）
- ❌ 不读 plain member 来决定颜色、大小等视觉属性（应读 DP）

#### `invalidate_render` 触发时机

| 触发源 | 机制 |
|--------|------|
| DP 变更（`affects_render = true`） | `set_value()` 内部自动调用 |
| 手动标脏 | 只用于 DP 之外的视觉状态（如 ripple 的 `active` 标志） |
| `update_visual_state()` | `on_visual_state_changed` 基类默认实现调用 |

---

### 21.3.6 布局系统（mine.ui.layout + mine.ui.visual）

**职责**：两遍协议（Measure/Arrange）确定每个元素的期望尺寸和最终矩形。

#### `on_measure` 规范

```cpp
void MyControl::on_measure(math::Size available) {
    // ① 调基类：触发模板懒构建，并把 available 传入 template_root
    Control::on_measure(available);

    // ② 若有模板根，采用其期望尺寸（基类已设置）
    if (template_root()) return;

    // ③ 无模板时的回退：手工估算尺寸
    const float w = /* 基于内容宽度 + padding */ 0.0f;
    const float h = /* 基于内容高度 + padding */ 0.0f;
    set_desired_size({w, h});
}
```

#### `on_arrange` 规范

```cpp
// 基类 Control::on_arrange 已处理模板根的 arrange，通常无需重写。
// 仅在控件有自定义布局逻辑（非模板路径）时重写。
```

#### 关于 DP 与布局

- DP 注册时带 `affects_measure = true` → `set_value` 后自动调 `invalidate_measure()`
- DP 注册时带 `affects_render = true` → `set_value` 后自动调 `invalidate_render()`
- 不需要在 `set_xxx()` 手动调 `invalidate_measure()`/`invalidate_render()`——**让 DP 系统处理**

---

## 21.4 数据存储规范

### 21.4.1 何时用 DependencyProperty

满足以下任一条件，属性**必须**注册为 DP：

| 条件 | 示例 |
|------|------|
| 需要被 Style 的 setter 覆写 | background, foreground, padding, font-size |
| 需要被动画（Storyboard）插值 | background（动画切换颜色） |
| 需要通过 TemplateBind 传递到模板子元素 | Content → ContentPresenter.Content |
| 需要支持数据绑定（OneWay/TwoWay） | text（绑定 ViewModel） |
| 其变化需要通知布局或渲染失效 | 任何影响外观/尺寸的属性 |

### 21.4.2 何时用 Plain Member

| 场景 | 说明 | 示例 |
|------|------|------|
| 输入状态标志 | 不是视觉属性，只影响 compute_visual_state() 的返回值 | `is_pressed_`, `is_hovered_`, `is_focused_` |
| DP 值缓存 | 通过 changed 回调从 DP 同步，供非-DP 路径快速访问 | `text_`（从 ContentProperty changed 回调同步） |
| 非可视私有状态 | 与外观无关的实现细节 | `ripple_.center_x`, `ripple_.start` |

### 21.4.3 DP 值缓存的正确写法

```cpp
// ① DP 注册时绑定 changed 回调
const DependencyProperty& Button::ContentProperty =
    register_property<Button>(
        "Content", core::Variant{},
        PropertyMetadata{ .changed = &Button::on_content_changed });

// ② changed 回调：同步到 plain member 缓存
void Button::on_content_changed(DependencyObject* sender, ..., const Variant& new_v) {
    auto* self = static_cast<Button*>(sender);
    self->text_ = new_v.has<InlineString>()
                      ? new_v.get<InlineString>()
                      : InlineString{};          // text_ 是缓存，不是真相源
}

// ③ set_text 通过 DP 写入（DP 触发回调 → 缓存同步 → 自动 invalidate）
void Button::set_text(StringView text) {
    set_value(ContentProperty, Variant{ InlineString{text} });
}

// ④ text() 读缓存（快速只读路径）
StringView Button::text() const noexcept {
    return { text_.data(), text_.size() };
}
```

### 21.4.4 禁止双写反模式

```cpp
// ❌ 反模式：background_ 和 BackgroundProperty 各自独立存储颜色
void Button::set_background(math::Color color) {
    background_ = color;           // 写了 plain member
    invalidate_render();
    // 忘了写 DP → on_render 读 DP 拿到 Default 值 → Bug
}

// ✅ 正确：DP 是唯一真相源，plain member 只作为可选缓存
void Button::set_background(math::Color color) {
    // 若有进行中动画先停止（防止 Animation 槽遮盖 Local）
    if (bg_storyboard_ && !bg_storyboard_->is_complete()) {
        bg_storyboard_->stop();
        bg_storyboard_ = nullptr;
    }
    set_value(BackgroundProperty, Variant{color}, ValuePriority::Local);
    // DP 的 affects_render=true → 自动触发 invalidate_render，无需手动调
}
```

---

## 21.5 完整调用链路图

### 21.5.1 属性写入链

```
用户调用 set_background(blue)
    └─► stop active storyboard（清 Animation 槽）
    └─► set_value(BackgroundProperty, blue, Local)
            └─► DependencyObject 写入 Local 槽
            └─► PropertyMetadata.affects_render → invalidate_render()
            └─► PropertyMetadata.changed → on_background_changed()（若有）
                    └─► 同步缓存成员
    └─► 下一帧调度 → on_render → get_value(BackgroundProperty) → 返回 Local=blue → 绘制蓝色
```

### 21.5.2 视觉状态切换链

```
MouseEnter 路由事件送达 Button
    └─► on_mouse_enter_router → on_mouse_enter()
            └─► is_hovered_ = true
            └─► update_visual_state()
                    └─► new_state = compute_visual_state() → Hovered
                    └─► if (new_state != current_state_):
                            └─► on_visual_state_changed(Normal, Hovered)
                                    └─► [F2] 直接创建 Storyboard，animate BackgroundProperty
                                    └─► [F3] vsm()->go_to_state("Hovered")
                                                └─► VSM 管理 Storyboard 全生命周期
```

### 21.5.3 动画驱动链（F2 临时方案）

```
帧定时器（App 层 SetTimer）
    └─► tick_bg_animation(dt)
            └─► storyboard->tick(dt)
                    └─► 插值计算：val = lerp(from, to, eased_t)
                    └─► set_value(BackgroundProperty, val, Animation)
                            └─► affects_render → invalidate_render()
            └─► if done:
                    └─► set_value(BackgroundProperty, final_val, Local)  // 持久化
                    └─► storyboard->stop()                               // 清 Animation 槽
    └─► if !has_active_animation(): KillTimer
```

### 21.5.4 动画驱动链（F3 目标方案）

```
Storyboard 内部注册到 AnimationClock
    └─► AnimationClock 在 UI 帧调度中自动 tick 所有活跃 Storyboard
    └─► 完成后自动 stop + 持久化 → 控件无感
App 层完全不参与动画时序管理
```

### 21.5.5 渲染链

```
invalidate_render()
    └─► 标记脏标志
    └─► 触发窗口重绘请求（当前：App 层 render()；F3：RenderScheduler）
        └─► 布局 pass（若有 invalidate_measure）
        └─► 渲染 pass：遍历视觉树调 on_render(canvas)
                └─► on_render 读 get_value(BackgroundProperty) → Animation/Local/Default
                └─► canvas.fill_rounded_rect(...) → paint pipeline → GPU
```

### 21.5.6 模板构建链

```
首次 measure（父布局调 control.measure(available)）
    └─► Control::on_measure(available)
            └─► template_root_ == nullptr && template_slot_ 非空
            └─► TemplateRegistry::find(template_slot_) → ControlTemplate
            └─► template.build_fn(*this)                // 调用构建函数
                    └─► 创建 ContentPresenter 等子元素
                    └─► bind_template(child, child_prop, host_prop)
                            └─► 立即同步一次：child.set_value(child_prop, get_value(host_prop), TemplateBind)
                            └─► 订阅 host_prop changed → 后续自动同步
                    └─► set_template_root(std::move(presenter))
            └─► template_root_->measure(available)      // 委托子树测量
            └─► set_desired_size(template_root_->desired_size())
```

---

## 21.6 控件开发规范

### 21.6.1 文件结构

```
api/include/mine/ui/controls/MyControl.h    ← 公开头文件
src/MyControl.cpp                           ← 实现文件
test/MyControlTest.cpp                      ← 单元测试
```

### 21.6.2 头文件结构模板

```cpp
// MyControl.h
#pragma once
#include <mine/ui/visual/Control.h>
#include <mine/ui/property/DependencyProperty.h>
/* ... 其他必要头文件 ... */

namespace mine::ui {

class MINE_UI_CONTROLS_API MyControl : public Control {
public:
    // ── 路由事件（若有）─────────────────────────────────────────────────────
    static const RoutedEvent& ClickEvent();   // Meyer's 单例

    // ── 依赖属性（必须按字母序排列）────────────────────────────────────────
    static const DependencyProperty& BackgroundProperty;
    static const DependencyProperty& ContentProperty;
    static const DependencyProperty& PaddingProperty;

    // ── 生命周期 ────────────────────────────────────────────────────────────
    MyControl();
    ~MyControl() override;
    MyControl(const MyControl&) = delete;
    MyControl& operator=(const MyControl&) = delete;

    // ── 公开属性访问（每个 DP 对应 getter + setter）────────────────────────
    [[nodiscard]] math::Color background() const noexcept;
    void set_background(math::Color color);

    // ── 动画辅助（F2 临时；F3 删除）────────────────────────────────────────
    [[nodiscard]] bool has_active_animation() const noexcept;
    void tick_animation(float dt);

protected:
    // ── 布局 ────────────────────────────────────────────────────────────────
    void on_measure(math::Size available) override;

    // ── 渲染 ────────────────────────────────────────────────────────────────
    void on_render(paint::Canvas& canvas) override;

    // ── 视觉状态 ─────────────────────────────────────────────────────────────
    [[nodiscard]] ControlVisualState compute_visual_state() const override;
    void on_visual_state_changed(ControlVisualState old_s, ControlVisualState new_s) override;

private:
    // ── DP 变更回调 ─────────────────────────────────────────────────────────
    static void on_content_changed(DependencyObject*, const DependencyProperty&,
                                   const core::Variant&, const core::Variant&) noexcept;

    // ── 事件路由（静态 + 实例，成对出现）──────────────────────────────────
    static void on_mouse_down_router(void*, RoutedEventArgs&, void*);
    static void on_mouse_enter_router(void*, RoutedEventArgs&, void*);
    static void on_mouse_leave_router(void*, RoutedEventArgs&, void*);
    void on_mouse_down(input::MouseEventArgs&);
    void on_mouse_enter();
    void on_mouse_leave();

    // ── 成员变量 ─────────────────────────────────────────────────────────────
    // [输入状态标志] ← 不涉及视觉属性
    bool is_enabled_ = true;
    bool is_hovered_ = false;
    bool is_pressed_ = false;

    // [DP 值缓存] ← 通过 changed 回调同步，不独立写入
    containers::InlineString text_;       // 从 ContentProperty 同步

    // [非可视私有状态]
    RippleState ripple_;

    // [F2 临时：动画管理] ← F3 迁移到 VSM
    core::OwnedPtr<animation::Storyboard> storyboard_;
    math::Color background_hover_ = /* ... */;   // F3 改为 StyleTrigger DP
    math::Color background_press_ = /* ... */;   // F3 改为 StyleTrigger DP
};

} // namespace mine::ui
```

### 21.6.3 实现文件结构模板

```cpp
// MyControl.cpp — 固定段落顺序

// ① DP 静态注册（文件顶部，在任何函数定义之前）
const DependencyProperty& MyControl::BackgroundProperty = register_property<MyControl>(
    "Background",
    core::Variant{ math::Color::Transparent },   // Default 值
    PropertyMetadata{
        .affects_render = true,                  // 变更自动触发 invalidate_render
        .changed        = nullptr,               // 仅影响渲染，无需同步缓存
    });

// ② ControlTemplate 注册（静态初始化）
static void s_build_template(DependencyObject& target) { /* ... bind_template ... */ }
static const style::ControlTemplate& s_template =
    style::TemplateRegistry::instance().register_template(
        "DefaultMyControlTemplate", TypeId::of<MyControl>(), &s_build_template);

// ③ DP 变更回调（仅需要缓存同步时实现）

// ④ 路由事件注册
const RoutedEvent& MyControl::ClickEvent() {
    static const RoutedEvent& ev = register_event<MyControl>("Click", RoutingStrategy::Bubble);
    return ev;
}

// ⑤ 构造/析构
MyControl::MyControl() {
    set_style_slot("DefaultMyControl");
    set_template_slot("DefaultMyControlTemplate");
    add_handler(input::MouseDownEvent(), &MyControl::on_mouse_down_router, this);
    add_handler(input::MouseEnterEvent(), &MyControl::on_mouse_enter_router, this);
    add_handler(input::MouseLeaveEvent(), &MyControl::on_mouse_leave_router, this);
}

// ⑥ 属性 getter/setter（按字母序）

// ⑦ on_measure（布局）

// ⑧ on_render（渲染）—— 只读 DP，只用 Canvas API

// ⑨ 视觉状态相关（compute_visual_state / on_visual_state_changed）

// ⑩ 事件路由静态函数 + 实例方法
// ⑪ F2 临时动画辅助（tick / has_active_animation）
```

### 21.6.4 `set_xxx` 编写规范

```cpp
void MyControl::set_background(math::Color color) {
    // 规则1：若有进行中动画，先中断（防止 Animation 槽遮盖 Local）
    if (storyboard_ && !storyboard_->is_complete()) {
        storyboard_->stop();
        storyboard_ = nullptr;
    }
    // 规则2：通过 DP 写入（不写 plain member；DP changed 回调负责同步缓存）
    set_value(BackgroundProperty, core::Variant{color}, ValuePriority::Local);
    // 规则3：不手动调 invalidate_render()——affects_render=true 已经处理
}
```

### 21.6.5 `compute_visual_state` 编写规范

```cpp
ControlVisualState MyControl::compute_visual_state() const {
    // 优先级：Disabled > Pressed > Hovered > Focused > Normal
    if (!is_enabled_) return ControlVisualState::Disabled;
    if (is_pressed_)  return ControlVisualState::Pressed;
    if (is_hovered_)  return ControlVisualState::Hovered;
    if (is_focused_)  return ControlVisualState::Focused;
    return ControlVisualState::Normal;
}
```

### 21.6.6 `on_visual_state_changed` 编写规范（F2 临时方案）

```cpp
void MyControl::on_visual_state_changed(ControlVisualState, ControlVisualState new_state) {
    // Step1：采样当前可见色（在 stop 之前，Animation 槽可能还有插值中的颜色）
    math::Color from_color = get_value(BackgroundProperty)
                                 .get_or<math::Color>(background_normal_);

    // Step2：停止旧动画（清 Animation 槽）
    if (storyboard_) storyboard_->stop();

    // Step3：将 from_color 写入 Local（capture_from_values 会读它）
    set_value(BackgroundProperty, core::Variant{from_color}, ValuePriority::Local);

    // Step4：计算目标色
    math::Color to_color;
    switch (new_state) {
    case ControlVisualState::Hovered:  to_color = background_hover_; break;
    case ControlVisualState::Pressed:  to_color = background_press_; break;
    case ControlVisualState::Disabled: to_color = background_disabled_; break;
    default:                           to_color = background_normal_; break;
    }

    // Step5：创建并启动新 Storyboard
    storyboard_ = core::make_owned<animation::Storyboard>();
    storyboard_->animate_dp_to(*this, BackgroundProperty,
                               core::Variant{to_color},
                               animation::Duration::milliseconds(100.0f),
                               animation::QuadEaseOut);
    storyboard_->capture_from_values();
    storyboard_->resolve_and_begin();
}
```

---

## 21.7 各子系统接口汇总

### 21.7.1 控件可调用的基类 API

| API | 来源 | 使用场景 |
|-----|------|----------|
| `set_value(prop, val, priority)` | DependencyObject | 写属性（Local 优先级） |
| `get_value(prop)` | DependencyObject | 读属性（on_render 中必须用此） |
| `clear_value(prop, priority)` | DependencyObject | 清除某优先级的值 |
| `invalidate_render()` | UIElement | 非 DP 路径的手动脏标记 |
| `invalidate_measure()` | UIElement | 尺寸变化（通常由 affects_measure 自动触发） |
| `bounds_rect()` | UIElement | on_render 中获取绘制区域 |
| `add_handler(event, fn, data)` | UIElement | 注册事件处理器 |
| `raise_event(event, args)` | UIElement | 发射自定义路由事件 |
| `update_visual_state()` | Control | 输入状态变化后调用 |
| `set_template_root(...)` | Control | build_fn 中设置模板根 |
| `bind_template(child, cp, hp)` | Control | build_fn 中建立 TemplateBinding |
| `find_template_child(name)` | Control | 查找模板子元素 |
| `template_root()` | Control | 访问模板根（on_measure/on_render 中判断） |
| `vsm()` | Control | 访问 VisualStateManager（F3 使用） |

### 21.7.2 控件可调用的 Canvas API（on_render 中）

| API | 用途 |
|-----|------|
| `canvas.fill_rect(rect, brush)` | 填充矩形 |
| `canvas.fill_rounded_rect(rrect, brush)` | 填充圆角矩形（胶囊按钮） |
| `canvas.stroke_rect(rect, pen)` | 描边矩形 |
| `canvas.fill_circle(center, r, brush)` | 填充圆（涟漪） |
| `canvas.save()` / `canvas.restore()` | 保存/恢复绘制状态（裁剪等） |
| `canvas.clip_rect(rect)` | 矩形裁剪 |
| `canvas.clip_rounded_rect(rrect)` | 圆角矩形裁剪（防溢出） |
| `canvas.draw_text(layout, origin, brush)` | 绘制已塑形文字 |

### 21.7.3 DP 注册参数速查

```cpp
PropertyMetadata{
    .affects_measure = true,   // 值变更 → invalidate_measure()
    .affects_render  = true,   // 值变更 → invalidate_render()
    .inherits        = false,  // 值沿视觉树向下继承（如 FontSize）
    .changed         = &MyControl::on_xxx_changed,  // 值变更回调（可空）
};
```

---

## 21.8 当前 Button 实现的已知问题（F2 → F3 待修列表）

| 问题 | 现状 | 目标（F3） |
|------|------|-----------|
| 状态颜色存储在 plain member | `background_hover_`、`background_press_` 由 App 代码通过 `set_background_hovered()` 设置 | 改为 Style 的 StyleTrigger setter；DP 用于 Normal 态颜色；Hover/Press 色来自 Style |
| 动画由 App 驱动 | App 调 `tick_bg_animation(dt)` 和 `has_active_animation()` | `AnimationClock` 自动 tick；控件不暴露此接口 |
| Storyboard 由 Button 自管理 | `bg_storyboard_` 是 Button 成员 | VSM 管理，`on_visual_state_changed` 改为一行 `vsm()->go_to_state(...)` |
| `has_active_animation()` 公开 | App 用它判断是否启停定时器 | 框架层感知（RenderScheduler 直接观察 AnimationClock） |
| Ripple 用 chrono 手动计时 | 非动画系统管理 | 改为 Storyboard 驱动（`animate_dp_to(RippleRadiusProperty, ...)`） |
| 无 ForegroundProperty DP | `foreground_` 是 plain member，只在 ContentPresenter 绑定时使用 | 注册为 DP，StyleSetter/主题可控制 |
| 无 BorderColorProperty DP | `border_color_` 是 plain member | 同上 |

---

## 21.9 新控件开发 Checklist

开发一个新控件前，逐项确认：

### 设计阶段
- [ ] 确认该控件继承自 `Control`（或 `UIElement` 的哪一级）
- [ ] 列出所有**可视属性**（颜色、尺寸、字体等）→ 全部注册为 DP
- [ ] 列出所有**输入状态**（按下、悬停、焦点等）→ 全部用 bool 成员
- [ ] 列出需要**对外发射的路由事件**（Click、ValueChanged 等）
- [ ] 确认是否需要 ControlTemplate（有内部结构则需要）
- [ ] 确认视觉状态集合（Normal/Hovered/Pressed/Disabled + 自定义）

### 实现阶段
- [ ] `.h` 中 DP 按字母序声明
- [ ] `.cpp` 中 DP 注册在文件顶部，静态初始化
- [ ] 每个 DP 的 `affects_measure`/`affects_render` 设置正确
- [ ] `set_xxx()` 通过 `set_value(..., Local)` 写入 DP，不写 plain member（除非是缓存）
- [ ] `on_render()` 通过 `get_value()` 读 DP，不读 plain member 来决定颜色/大小
- [ ] `on_measure()` 调 `Control::on_measure(available)` 触发模板懒构建
- [ ] 构造函数注册所有事件处理器（MouseDown/Up/Enter/Leave 成对）
- [ ] `compute_visual_state()` 遵循 Disabled > Pressed > Hovered > Focused > Normal 顺序
- [ ] `on_visual_state_changed()` Step1-5 顺序正确（采样→停止→写Local→计算to→启动）
- [ ] `tick_animation()` 完成时写 Local 持久化后再 `stop()`
- [ ] 有 ControlTemplate 则注册 `build_fn`，在 `build_fn` 内 `bind_template`

### 验收阶段
- [ ] 初始颜色正确（不显示 Default 紫色等默认值）
- [ ] Hover 变色正常
- [ ] Press 变色正常
- [ ] Disabled 状态正确（鼠标无响应，外观灰暗）
- [ ] 重复快速切换状态（快速移入移出）无残影、无卡死
- [ ] 动画完成后 `set_background()` 可立即更新颜色
- [ ] 单元测试覆盖：DP 读写、模板绑定、视觉状态切换

---

## 21.10 未来架构演进路径（F3+）

```
F2 现状（临时方案）：
  App 层 → SetTimer → tick_bg_animation → Storyboard → DP(Animation)

F3 目标（框架管理）：
  InputEvent → on_visual_state_changed → vsm()->go_to_state()
                                          └─► VSM 管理 Storyboard
                                          └─► AnimationClock 自动 tick
                                          └─► RenderScheduler 按需调度
  App 层完全不感知动画时序

F4 目标（MML 驱动）：
  style DefaultButton for Button {
      background: {DynamicResource PrimaryColor};
      :hover { background: {DynamicResource PrimaryHover}; }   // → StyleTrigger
      :pressed { background: {DynamicResource PrimaryPress}; } // → StyleTrigger
  }
  // mmlc 生成 apply_DefaultButton() + 包含 VSM Storyboard 描述的 build_fn
  // 控件 .cpp 中不再有任何颜色常量
```
