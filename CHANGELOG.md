# Changelog

本项目遵循 Keep a Changelog，版本号遵循语义化版本。

## [Unreleased]

### Fixed
- **mine.ui.animation / mine.ui.style：修复 VSM 颜色状态切换与 Local(P50) 属性共存时的失效问题**：

  **根本原因**：`VisualStateManager::go_to_state` 在 `capture_from_values()` 之前已执行
  `stop_all`（清除 Animation P60），导致 from 值读取的是 Local P50（用户 `set_background()` 颜色）
  而非当前可见颜色；同时 `resolve_and_begin()` 的 to 值也被 Local P50 遮盖，
  最终 from == to（均为 Local 色），动画存在但颜色始终不变。

  **修复一：`capture_from_values()` 时序修正**
  将 `capture_from_values()` 移到 `stop_all` 之前执行，此时旧的 Animation(P60)（由上一个
  retain_p60=true 完成的 Storyboard 保持）仍然存在，from 读到真实可见颜色。
  另外在旧 SB stop_all 之后，对新 SB 受管属性显式调用 `stop()`，清除残留的 P60
  （因为 retain_p60=true 的完成 SB 已从 active_storyboards_ 移除，stop_all 无法覆盖）。

  **修复二：`retain_p60` 机制**
  `PropertyAnimation` 新增 `retain_p60` 字段，由 `resolve_and_begin()` 根据目标属性在
  StyleTrigger(P30) 层是否有值自动决定：
  - **有 StyleTrigger 值**（Hovered / Pressed 等明确状态颜色）：`to = P30`，`retain_p60 = true`。
    动画完成时调用 `stop_not_retained()` **不清 P60**，保持 P60 覆盖 Local P50，
    始终显示目标颜色，直到下次 `go_to_state` 的 `stop()` 才清除。
  - **无 StyleTrigger 值**（Normal / Default 等回退状态）：`to = get_value()`（含 Local P50），
    `retain_p60 = false`。动画完成时 `stop_not_retained()` 清除 P60，
    Local P50 接管（颜色无跳变地恢复为用户 `set_background()` 值）。

  **修复三：首次切换跳过动画**
  `current_state_` 为空（控件刚应用模板）时首次 `go_to_state` 直接跳变，不建立 Storyboard，
  避免产生不必要的空动画（from == to）并留下永不 tick 的 active Storyboard。

  **Storyboard 新增 API：**
  - `stop_not_retained()`：仅清除 `retain_p60=false` 属性的 P60
  - `stop()` 语义保持不变：强制清除所有受管属性的 P60（用于状态切换中断）

  **DependencyObject 新增 API：**
  - `get_value(prop, max_priority)`：读取 priority ≤ max_priority 中最高优先级的值

- **mine.ui.controls：修复 Button 模板构建后 VSM current_state_ 未同步问题**：
  `on_apply_template()` 完成后 VSM 已挂载，但 `current_state_` 仍为空字符串，
  导致首次 `go_to_state("Normal")` 因"已是当前状态"幂等检查提前返回，
  状态机从未写入 P30 StyleSetter 基线、动画路径无法正常建立。
  修复：在 `Button::on_apply_template()` 末尾调用 `update_visual_state()`，
  强制 VSM 同步当前逻辑状态（`is_hovered_`、`is_pressed_` 等），
  确保 `current_state_` 与按钮实际状态对齐。

- **samples/02-controls-demo：修复 btn_switch_tmpl_ 颜色状态切换（Hover/Pressed）不可见问题**：
  原代码调用 `set_background(Brush::solid_rgb(0x37474F))` 写入 Local(P50)，
  `Storyboard::resolve_and_begin()` 调用 `get_value()` 取当前最高优先级值作为动画终止值（to），
  Local(P50) > StyleTrigger(P30)，导致 `to` 取到 Local 灰色而非 Hovered 颜色，from == to，
  Storyboard 在运行但颜色始终不变。
  修复：移除 `set_background` / `set_foreground` / `set_border_color` 的 Local 设置，
  新增 `switch_style_`（StyleSetter P20 + Hovered/Pressed StateSetters P30），
  通过 `btn_switch_tmpl_.set_vsm_style(&switch_style_)` 以 Style 层驱动颜色，
  确保 VSM 状态颜色（#37474F → #546E7A/Hovered，#263238/Pressed）完整生效。

- **mine.ui.controls：修复 Button 默认模板 Hovered/Pressed 颜色对比度过低（VSM 状态变化不可见）**：
  原 Hovered（#735BAC）与 Normal（#6750A4）亮度差仅约 5%，Pressed（#7A65AF）差距更小，
  用户在屏幕上几乎无法感知颜色变化。
  修复：Hovered 调整为 #8876C0（较 Normal 亮约 20%），Pressed 调整为 #5743A0（较 Normal 暗约 15%），
  确保状态切换视觉反馈清晰可见。

- **mine.ui.controls：`Button::on_render` 新增 `BorderColorProperty` 驱动的圆角边框绘制**：
  `BorderColorProperty` DP 已存在且有 `set_border_color()` 访问器，但 `on_render` 从未读取该属性，
  导致所有颜色设置无渲染效果。
  修复：在背景圆角矩形之后、Ripple 涟漪之前读取 `BorderColorProperty`；
  若画刷非透明则调用 `Canvas::stroke_rounded_rect` 用 `Pen{width=2}` 绘制与背景同形的圆角边框；
  默认画刷透明，不影响现有按钮外观；`set_border_color()` 写 Local(P2)，也可通过 Style P5 层驱动。

- **samples/02-controls-demo：修复绿色/橙色自定义模板 VSM 悬停/按下状态不可见问题**：
  两个模板均以 `ui::Border` 为模板根，`Border::on_render` 使用 `set_background()` 写入的不透明纯色
  填充整个控件区域，完全遮住了 `Button::on_render` 中由 VSM 驱动的 `BackgroundProperty` 圆角背景；
  悬停/按下动画存在但永远不可见，Ripple 涟漪同理被遮盖。
  修复：将两个模板改为直接以 `ContentPresenter` 为模板根（与默认模板结构一致）：
  - 移除 `core::make_owned<ui::Border>()`，`root->set_background/border_color/border_thickness/set_child` 等调用全部删除；
  - `s_green_style` / `s_orange_style` 新增 `BorderColorProperty` setter，
    由 `Button::on_render` 通过新增的圆角边框绘制逻辑完整展示；
  - VSM 背景动画（#1B5E20 → #2E7D32 / #388E3C，#BF360C → #E64A19 / #FF5722）现在完全可见。


  实现了注释中声明但代码中缺失的隐式协变转换能力（`OwnedPtr<Derived>` → `OwnedPtr<Base>`）；
  要求 `Derived*` 可隐式转换为 `T*`（`std::is_convertible_v<U*, T*>`），转移所有权和删除器；
  为 `BuildFn` 中 `OwnedPtr<ContentPresenter>` 向 `OwnedPtr<UIElement>` 的安全转型提供语言支持。

- **mine.ui.controls：`Border::set_child(OwnedPtr<UIElement>)` — 所有权转移重载**：
  新增接受 `core::OwnedPtr<UIElement>` 参数的 `set_child` 重载，允许 `BuildFn` 中动态创建的子元素
  （如 `ContentPresenter`）所有权完整转移到 `Border`；`Border` 析构时自动释放子元素，
  无需外部管理生命周期；旧裸指针重载在设置时同时清除可能存在的 `owned_child_`（防止双持）。

- **samples/02-controls-demo：WPF 风格 ControlTemplate 完整演示**：
  改造第 8 节（ControlTemplate 演示区），展示 BuildFn + Part 命名 + 动态切换的完整工作流：
  - 新增文件作用域 `s_build_green_template` / `s_build_orange_template` 两个 `BuildFn`；
    各 BuildFn 通过 `set_template_name("root"/"content")` 命名元素，
    通过 `bind_template()` 绑定 ContentPresenter 与宿主 Content/Padding 属性，
    并通过 `Border::set_child(OwnedPtr)` 完整转移 ContentPresenter 所有权；
  - `btn_tmpl_` 通过 `set_control_template(&tmpl_green_)` 使用 TemplateProperty DP（WPF 风格），
    初始呈现绿色圆角外观，点击计数功能正常；
  - `btn_switch_tmpl_`（切换模板按钮）点击后调用 `s_on_switch_tmpl`，
    通过 `set_control_template` 切换到 `tmpl_orange_`，`on_apply_template()` 自动重新
    `find_template_child("content")` 绑定同名 Part，计数功能透明延续，展示 WPF 换模板不换功能的设计。

### Changed
- **mine.ui.visual：`Control::TemplateProperty` DP（WPF 风格模板属性）**：
  新增 `Control::TemplateProperty`（存储 `const style::ControlTemplate*`），
  用户通过 `set_control_template(tmpl)` 写入 Local(P2) 槽以替换整个控件模板；
  `measure_override` 优先检查 TemplateProperty，回退到 `template_slot_` 注册表查找（向后兼容）；
  DP 变更时自动清除旧模板根并 `invalidate_measure()`，下次 measure 时重建视觉树。

- **mine.ui.visual：`UIElement::invalidate_measure() / invalidate_arrange()` 改为 public**：
  与 WPF `UIElement.InvalidateMeasure()` 保持一致，允许外部（非成员函数、工厂函数）
  触发布局失效；原 protected 语义通过文档约束（非布局协议内部勿滥用）替代。

- **mine.ui.controls：移除 `Button::on_arrange` 胶囊裁剪——裁剪完全由用户显式控制**：
  删除 `template_from_registry_` 标志和整个 `Button::on_arrange()` override；
  裁剪不再由框架自动设置——如需胶囊形裁剪，用户显式调用 `set_clip_rounded_rect()`；
  默认行为：模板根的矩形 `bounds_rect` 即为命中区域，无自动圆角截断。

- **mine.ui.controls：`Button::hit_test` 委托给模板根（WPF 风格命中边界）**：
  命中区域由 `template_root()->bounds_rect()` 决定，返回 `this` 确保 MouseEnter/Leave
  仍路由到 Button 自身；若用户显式设置了 `clip_rounded_rect`，则圆角外不可命中；
  无模板根时回退到自身 `bounds_rect()`（向后兼容）。

### Fixed
- **mine.ui.controls：修复 Button 自定义模板胶囊裁剪问题（Issue 1）**：
  `Button::on_arrange` 之前无条件对所有按钮设置胶囊形 `clip_rounded_rect`（radius = height / 2），
  导致用户通过 `set_template_root(OwnedPtr)` 直接设置的自定义矩形模板（如带边框的 `Border`）
  被裁成胶囊形，四角矩形外观被破坏。
  修复方案：新增 `template_from_registry_` 私有标志——仅当模板由注册表的 `build_fn_` 构建
  （即 `on_apply_template()` 被调用时）才设置此标志，`on_arrange` 只在 `template_from_registry_ == true`
  时应用胶囊裁剪；用户直接设置 `set_template_root()` 的按钮清除裁剪，命中测试回退为矩形 bounds_rect。

- **mine.ui.controls：修复 Button VSM 状态机样式固化为 default_button_style 问题（Issue 2）**：
  `s_build_default_button_template` 原始实现硬编码 `vsm.set_style(&default_button_style())`
  并调用 `default_button_style().apply(button)`，导致：
  ① 用户通过外部 `Style::apply()` 写入的 P5 基线色在模板构建时被 `default_button_style().apply()` 覆盖；
  ② VSM `go_to_state` 时调用 `default_button_style().apply_state()`，外部 `Style` 的 P4 状态色从未生效。
  修复：新增 `Button::set_vsm_style(Style*)` API，在首次 measure 构建模板时读取 `button.vsm_style()`
  以确定 `active_style`（用户自定义样式或回退到 default），VSM 和 `apply()` 均使用 `active_style`。

- **samples/02-controls-demo：更新绿色按钮使用 `set_vsm_style` 替代 `demo_style_.apply()`**：
  原先 `demo_style_.apply(btn_styled_)` 写入的 P5 绿色会被模板构建时的
  `default_button_style().apply()` 覆盖，悬停/按下颜色也走 `default_button_style` 的 P4 紫色。
  改用 `btn_styled_.set_vsm_style(&demo_style_)` 后，模板构建时 `active_style = demo_style_`，
  P5 基线（绿色）和 P4 状态色（悬停 #43A047，按下 #1B5E20）均来自 `demo_style_`，绿色主题完整生效。

### Added
- **mine.ui.controls：`Button::set_vsm_style(Style*)` — VSM 外部样式注入 API**：
  允许在模板构建前（`_build()` 阶段）为按钮绑定外部 `Style` 对象，
  `s_build_default_button_template` 在首次 measure 时自动将该样式用于 VSM 连接和 P5 apply。
  若在模板构建后调用，立即更新已有 VSM 的 `style_` 引用（热切换）。
  传入 `nullptr` 时回退到内置 `default_button_style()`（MD3 Primary 紫色配色）。

- **samples/01-mvvm-binding：删除已废弃的 `set_background_hovered / set_background_pressed` API 调用**：
  Button 三层分离重构后，`HoveredBackgroundProperty` / `PressedBackgroundProperty` 已删除，
  对应访问器 `set_background_hovered() / set_background_pressed()` 随之移除。
  `CounterWindow.cpp` 中四个按钮（`+1`、`-1`、重置、退出）各删除 2 处旧 API 调用（共 8 处），
  现由 P4 StyleTrigger（VSM 状态色）和 P5 StyleSetter（基线色）负责悬停/按下外观；
  `set_background()` 写 Local(P2)，优先级高于 P4，故按钮保持用户指定颜色不受状态影响（符合预期）。

- **samples/02-controls-demo：修复自定义模板按钮 MD3 紫色背景从 Border 背后漏出**：
  直接调用 `set_template_root(OwnedPtr)` 跳过了 `build_fn_` 路径，导致
  `default_button_style().apply()` 未执行，`BackgroundProperty` 保留 Default(P0)=`#6750A4`（MD3 Primary 紫）。
  Button 的 `on_render` 始终绘制背景圆角矩形，Purple 背景会从 Border 边缘/后方透出影响视觉。
  修复：在 `btn_tmpl_.set_template_root()` 之前添加
  `btn_tmpl_.set_background(paint::Brush::solid(math::Color::Transparent))`，
  写 Local(P2) 透明画刷覆盖 Default(P0)，使 Button 自身背景透明，Border 模板完整呈现。

### Added
- **mine.ui.window / mine.platform.abi / mine.platform.win32：`WindowStateProperty` DP 属性——窗口状态 WPF 风格控制**：
  新增 `Window::WindowStateProperty`（`int`/`WindowState` 枚举，默认 Normal），
  设置属性即可驱动平台层执行最小化/最大化/还原，无需直接调用任何 Win32 API：
  - `mine.platform.abi`：新增 `WindowState.h`（`enum class WindowState : int { Normal, Minimized, Maximized }`），
    `PlatformAbi.h` 伞形头文件新增导出，`IWindow` 增加 `set_state(WindowState)` 和
    `state() const` 虚方法（默认空实现）
  - `mine.platform.win32`：`Win32Window::set_state()` 使用 `ShowWindow(SW_MINIMIZE/SW_MAXIMIZE/SW_RESTORE)`；
    `Win32Window::state()` 使用 `IsIconic/IsZoomed` 判断当前状态
  - `mine.ui.window`：`Window::WindowStateProperty` DP 注册（int 存储枚举，遵循
    `CornerPreferenceProperty` 模式）；`set_window_state()` / `window_state()` 访问器；
    `s_on_state_changed` 变更回调自动调用 `native_window_->set_state()`
  - `samples/03-custom-chrome`：最小化/最大化按钮改用 `set_window_state()` + `window_state()`，
    移除对 `ShowWindow` / `IsZoomed` 及 `<windows.h>` 的直接依赖

- **mine.ui.window / mine.platform.abi / mine.platform.win32：`Window::drag()` — WPF DragMove() 等价接口**：
  新增 `Window::drag()` 公开方法及平台层 `IWindow::begin_drag()` 虚接口，
  实现鼠标左键在自定义标题栏区域按下时触发系统窗口拖拽：
  - `IWindow::begin_drag()`：默认空实现，Win32 后端重写为
    `ReleaseCapture() + PostMessageW(WM_NCLBUTTONDOWN, HTCAPTION)`
  - `Window::drag()`：检查初始化状态后委托 `native_window_->begin_drag()`；
    `03-custom-chrome` 示例中 `s_on_drag_mouse_down` 改用此接口，
    移除对 HWND 的直接访问，实现完全平台无关的拖拽调用

- **samples/03-custom-chrome：新增 WindowChrome 自定义无边框标题栏演示示例**：
  演示 `IsCustomChrome / CaptionHeight / ResizeBorderThickness / CornerPreference` 四个 DP 属性的协同工作：
  - 应用层自绘 48px 标题栏（图标 + 标题文字 + 最小化/最大化/关闭按钮）
  - `CaptionHeight = 0`：避免系统 `HTCAPTION` 阻挡标题栏按钮点击
  - 拖拽实现：在 `drag_region_`（TextBlock）的 `MouseDownEvent` 中通过
    `ReleaseCapture + PostMessageW(WM_NCLBUTTONDOWN, HTCAPTION)` 触发系统拖拽
  - `ResizeBorderThickness = 6px`：系统自动处理四边/四角 resize 命中测试
  - `CornerPreference = Round`：Windows 11 圆角窗口
  - 深色配色（`#1F1F1F` 标题栏 + `#191919` 内容区），关闭按钮悬停变 Windows 红（`#C42B1C`）

- **mine.ui.visual：Visual 圆角矩形裁剪（clip_rounded_rect）+ UIElement 命中测试感知圆角**：
  - `Visual` 扩展裁剪能力：新增 `set_clip_rounded_rect / has_clip_rounded_rect / clip_rounded_rect / clear_clip_rounded_rect`，
    内部以 `ClipKind` 枚举（None / Rect / RoundedRect）管理，与矩形裁剪互斥；
    `render_to_canvas()` 根据类型选择 `canvas.clip_rounded_rect` 或 `canvas.clip_rect`
  - `UIElement::hit_test()` 优先检查圆角矩形裁剪：点在圆角外时整棵子树不参与命中，
    解决"圆角控件外角可命中"问题
  - `UIElement::hit_test_local()` 默认实现升级：有圆角裁剪时以 `clip_rounded_rect().contains(p)`
    代替 `bounds_rect().contains(p)` 作为命中判断，无需控件覆写
- **mine.ui.controls：Button 命中/裁剪边界与视觉胶囊形状一致**：
  - `Button::on_arrange()` 在布局完成后自动调用 `set_clip_rounded_rect(RoundedRect{bounds_rect, height/2})`，
    将渲染裁剪（子元素不溢出圆角）与命中测试（外角不响应鼠标）统一为胶囊形状
  - `Button::hit_test()` 优先使用 `clip_rounded_rect()` 判断，外角点击不再触发 Hover/Press 状态
- **mine.ui.window / mine.platform.abi / mine.platform.win32：自定义标题栏 WindowChrome DP 属性接口及 Win32 实现**：
  在 `Window` 类新增 5 个 DependencyProperty，实现 WPF WindowChrome 风格的自定义无边框窗口 API，
  全部平台相关代码隔离在 `mine.platform.win32`：

  **`Window` 新增 DP 属性（mine.ui.window）**：
  - `IsCustomChromeProperty`（bool，默认 false）：启用/禁用自定义 Chrome；
  - `CaptionHeightProperty`（float，默认 32.0f）：可拖拽标题栏区域高度（逻辑像素）；
  - `ResizeBorderThicknessProperty`（Thickness，默认 4.0f 各边）：resize 热区厚度；
  - `CornerPreferenceProperty`（WindowCornerPreference，默认 SystemDefault）：窗口圆角策略；
  - `GlassFrameThicknessProperty`（Thickness，默认全零）：DWM 玻璃帧延伸区域。

  **平台抽象层（mine.platform.abi）**：
  - 新增 `WindowCornerPreference.h`：跨平台圆角策略枚举（SystemDefault / DoNotRound / Round / RoundSmall）；
  - 新增 `WindowChromeDesc.h`：Chrome 配置描述符结构体；
  - `IWindow::set_chrome(const WindowChromeDesc&)`：平台可选虚方法接口，默认空实现。

  **Win32 实现（mine.platform.win32）**：
  - `Win32Window::set_chrome()`：存储描述符并调用 `apply_chrome_()`；
  - `Win32Window::apply_chrome_()`：调用 `DwmExtendFrameIntoClientArea`（玻璃帧延伸，含全窗口玻璃模式），
    `DwmSetWindowAttribute(33)` 设置 Windows 11+ 圆角，`SetWindowPos(SWP_FRAMECHANGED)` 触发重计算；
  - `WM_NCCALCSIZE`：启用 Chrome 时将客户区扩展到整个窗口（WPF WindowChrome 同款方案），
    最大化时自动内缩 `SM_CXFRAME + SM_CXPADDEDBORDER` 防止溢出工作区；
  - `WM_NCHITTEST`：按 CaptionHeight / ResizeBorderThickness 实现自定义命中测试，
    支持四角拉伸、四边拉伸、标题栏拖拽和客户区透传。


  通过三层协同，View 层实现零业务路由桩的完整命令绑定：

  **`MINE_COMMAND(Name)` 宏**（新建 `Command.h`，`Mvvm.h` 自动导入）：
  - 一行声明 `RelayCommand Name##_` 公开字段 + 自动注册命令 getter，
    等价于 `MINE_OBSERVABLE` 对普通属性的作用；
  - 构造时自动向 `ObservableObject` 反射表注册 `get_property(#Name)` → `Variant{ICommand*}` 的映射，
    使 `set_binding(Button::CommandProperty, "cmd_name")` 可按名解析命令，无需手写任何注册代码

  **`Button::CommandProperty` / `CommandParameterProperty`**（新增 DependencyProperty）：
  - `CommandProperty`（Variant 存储 `ICommand*`）：写入时 `on_command_changed` 回调自动
    取消旧订阅、订阅新命令 `can_execute_changed`、并立即刷新 `is_enabled_`
  - `CommandParameterProperty`（Variant，默认为空）：传递给 `execute/can_execute`，
    支持进一步绑定
  - `Button::~Button()` 析构时自动取消 `can_execute_changed` 订阅，防止悬空回调
  - `Button::raise_click()` 触发 Click 路由事件后自动调用 `ICommand::execute(param)`，
    无需用户代码介入

  **Command 绑定一行接入（等价 WPF）**：
  ```cpp
  btn_inc_.set_binding(ui::Button::CommandProperty, "increment_cmd");
  ```
  等价于 WPF 的 `btn.SetBinding(Button.CommandProperty, new Binding("IncrementCmd"))`

  **`sample.01-mvvm-binding` 更新**：
  - `CounterViewModel`：三个命令从手动声明 + 手动注册改为 `MINE_COMMAND` 宏一行完成
  - `CounterWindow`：删除所有业务命令路由桩（`s_on_click_inc/dec/reset`），
    改为 `bind_()` 中三行 `set_binding(CommandProperty, "cmd_name")`；
    仅保留 `s_on_click_quit`（跨层信号，不属于 ViewModel 业务）

### Fixed
- **mine.gfx.d3d11 / mine.ui.window：修复窗口 resize 时视口尺寸不更新**：
  - `D3D11Queue::wait_idle()` 不再仅调用 `Flush()`；改为使用可复用的 `D3D11_QUERY_EVENT`
    精确等待 GPU 执行到命令流尾部
  - `D3D11Queue::submit()` 在 `ExecuteCommandList()` 返回后立即释放旧
    `ID3D11CommandList`；避免上一帧命令列表继续持有 swapchain 后缓冲引用，
    导致 resize 发生在两帧之间时 `ResizeBuffers()` 无法成功
  - 修复 `Window::handle_resized()` 在交换链 `ResizeBuffers()` 前 GPU 仍持有旧后缓冲引用的风险，
    避免 resize 失败后 DWM 继续拉伸旧表面，表现为“窗口变大但视口不跟着变化”
  - 样例链路构建通过，`sample.01-mvvm-binding` 可正常启动
- **mine.ui.controls / mine.ui.animation：修复 Button 从 Hovered/Pressed/Disabled 回到 Normal 时外观停留在前一状态**：
  - `Storyboard` 新增显式 `from/to` 注册路径，允许调用方直接提供动画起止值，
    不再需要通过改写目标属性的 `Local` 槽来喂 `capture_from_values()`
  - `Button::on_visual_state_changed()` 改为基于 `old_state` 采样当前可见背景；
    其中 `Disabled` 起始色取渲染期禁用色，而非错误读取当前 `is_enabled_` 后的目标色
  - Button 背景过渡不再污染 `BackgroundProperty` 的语义 Normal 值，
    修复 `Pressed→Hovered→Normal` 回到 Pressed 外观、`Normal→Disabled→Normal` 停留在 Disabled 外观的问题
  - 新增回归测试覆盖 `Normal→Disabled→Normal` 与 `Pressed→Hovered→Normal`


  通过 `struct Binding` 描述符，`set_binding()` 现可承载 converter/conv_param/fallback 完整配置：
  - `BindingConfig.h` 新建：`struct Binding { prop_name, mode, converter, conv_param, fallback }`，
    等价于 WPF 的 `new Binding("PropName") { Converter=..., Mode=... }`；
    支持 C++20 指定初始化语法，未设置字段使用合理默认值
  - `Binding.h` 伞形文件：新增 `#include <mine/ui/binding/BindingConfig.h>`
  - `BindingExpression`：新增 `conv_param`（`core::StringView`）公开字段；
    `Impl` 存储 `conv_param`；`re_evaluate()` 将 `conv_param` 传递给 `IConverter::convert()`；
    move 构造/赋值同步处理 `conv_param`
  - `BindingExpression::bind(out, src, Binding, target, target_prop)`（新增重载）：
    有 src 版——从 `Binding` 描述符读取所有配置并激活绑定
  - `BindingExpression::bind(out, Binding, target, target_prop)`（新增重载）：
    无 src 版——从 `target.DataContext` 自动解析 `INotifyPropertyChanged*`，再应用描述符
  - `FrameworkElement::set_binding(target_prop, Binding)`（新增重载）：
    等价于 WPF 的 `element.SetBinding(prop, new Binding("x") { Converter=&cv, ConvParam="MB" })`；
    绑定生命周期仍由内置 `bindings_` 管理

### Added
- **mine.ui.visual / mine.ui.binding：`element.set_binding()` — 完全等价 WPF SetBinding 的内置绑定 API**：
  FrameworkElement 现在持有内置绑定存储，View 层无需声明任何 BindingExpression 成员变量：
  - `FrameworkElement::set_binding(target_prop, prop_name, mode)`（新增）：
    等价于 WPF 的 `element.SetBinding(prop, new Binding("PropName"))`；
    从控件自身 DataContext 属性自动解析 `INotifyPropertyChanged*`，
    按属性名建立订阅，绑定生命周期由 `FrameworkElement::bindings_`（内置 `Vector<BindingExpression>`）管理
  - `FrameworkElement::clear_all_bindings()`（新增）：批量 detach 并清除所有绑定
  - `FrameworkElement` 显式 move 构造/赋值（替代 `= default`）：
    移动后对 `bindings_` 中每个 `BindingExpression` 调用 `retarget(*this)`，
    确保 `Impl::target_obj` 指向新地址，消除潜在悬空指针
  - `BindingExpression::bind(out, prop_name, target, target_prop)`（新增无 src 重载）：
    从 `target.get_value(DataContextProperty)` 读取 `INotifyPropertyChanged*`，
    再复用带 src 的 `bind()` 完成订阅激活；避免在 `mine.ui.binding` 引入 Window 类型
  - `BindingExpression::register_data_context_property(dp)`（新增）：
    DataContextProperty 描述符的注入接口；由 `mine.ui.window` 静态初始化时调用，
    打破 `mine.ui.binding ← mine.ui.window` 的潜在循环依赖
  - `BindingExpression::retarget(new_target)`（新增）：
    修正 `Impl::target_obj` 指针，供 FrameworkElement 移动时调用
  - `mine.ui.visual/xmake.lua`：新增 `mine.ui.binding` 依赖（无循环：binding 不依赖 visual）
  - `mine.ui.window/xmake.lua`：新增显式 `mine.ui.binding` 依赖声明
  - `Window.cpp`：静态初始化阶段注入 DataContextProperty 描述符（匿名命名空间 bool 常量技巧）
  - `samples/01-mvvm-binding/CounterWindow`：去掉 `count_bind_`/`hint_bind_` 成员，
    `bind_()` 精简为两行 `element.set_binding()` 调用，零显式 ViewModel 引用

### Added
- **mine.ui.binding / mine.mvvm：WPF 风格属性名绑定（消除手写 getter lambda）**：
  通过三层协同实现 `BindingExpression::bind()` 工厂，视图层无需编写任何 getter lambda：
  - `INotifyPropertyChanged::get_property(name)`（新增虚方法）：
    接口层新增按属性名读取值的默认虚方法（返回空 Variant），
    供 `BindingExpression::bind()` 在不依赖 `mine.mvvm` 的前提下按名反射读取属性值，
    从根本上避免 `mine.ui.binding ← mine.mvvm` 循环依赖
  - `ObservableObject::get_property(name)` / `register_property_getter(name, getter)`（新增）：
    重写接口层虚方法；新增 protected `register_property_getter()` 接口供子类注册属性 getter；
    `Impl` 内部维护 `Vector<PropertyEntry>` 线性查找表（属性数量通常 < 20，线性查找优于哈希）
  - `MINE_OBSERVABLE` 宏扩展：新增私有成员 `bool mine_reg_<Name>_`，
    其成员初始化器在对象构造时自动调用 `register_property_getter(#Name, [this]{ return Variant{prop}; })`，
    将每个属性的 getter 注册到查找表，无需手动注册；
    lambda 仅捕获 `this`（8 字节），符合 `core::Function` SBO 上限
  - `BindingExpression::bind(out, src, prop_name, target, target_prop)`（新增工厂）：
    内部自动构造 `getter = [&src]{ return src.get_property(prop_name); }`，
    等价于 WPF 的 `{Binding PropName}` 语法；`bind_inpc()` 旧工厂保持不变
  - `CounterWindow::bind_()` 更新：使用新 `bind()` API，彻底消除手写 getter lambda

### Added
- **mine.ui.window：Window 接入依赖属性（DP）系统**：
  `Window` 现在继承 `DependencyObject`，可持有任意依赖属性值：
  - 新增 `Window::DataContextProperty`（`inherits = true`）：作为窗口级 ViewModel 上下文属性，
    通过 Visual 层的 inherits 传播机制自动向下扩散到整棵视觉子树
  - 新增 `set_data_context(ctx)` / `data_context()` 接口
  - `set_content(element)` 时若已设置 DataContext，自动以 `Inherited` 优先级写入内容根
  - DataContext 变更回调 `s_on_data_context_changed` 将新值推送到内容根，
    视觉树通过 `Visual::on_property_changed` 的 inherits 传播机制继续向下
  - `xmake.lua` 新增对 `mine.ui.property` 的直接依赖声明（原为间接依赖）
- **mine.ui.visual：实现 inherits=true 依赖属性向下传播**：
  `Visual::on_property_changed()` 中，当 `prop.metadata().inherits == true` 时，
  自动以 `ValuePriority::Inherited` 将新值写入所有直接子节点；子节点的
  `on_property_changed` 递归继续传播，整棵子树无需手动遍历

### Added
- **samples/01-mvvm-binding：MVVM + 数据绑定计数器演示（任务 28 阶段一）**：
  新增 `samples/01-mvvm-binding/` 示例，以纯 C++ 演示 MVVM 分层与 BindingExpression 工作原理：
  - `CounterViewModel`（头文件）：继承 `ViewModelBase`，使用 `MINE_OBSERVABLE` 宏声明
    `count(int)`、`count_text(InlineString)`、`hint_text(InlineString)` 三个可观察属性；
    通过 `RelayCommand` 封装加一（上限 10）、减一（下限 0）、重置三个操作，
    并在 `update_display_()` 中同步格式化字符串与命令可执行状态通知
  - `CounterWindow`（View 层）：继承 `mine::ui::Window`，持有 ViewModel 作为值成员；
    通过 `BindingExpression::from_inpc(vm_, "count_text")` / `"hint_text"` 订阅 ViewModel 属性变更，
    自动同步到对应 `TextBlock` 的 `TextProperty`；
    按钮点击通过静态路由桩调用 ViewModel 命令，View 层不含业务逻辑
  - 成员声明顺序：`vm_` 先声明（后析构），`count_bind_`/`hint_bind_` 后声明（先析构），
    保证 `detach()/unsubscribe()` 时 ViewModel 仍存活
  - 构建：`xmake build sample.01-mvvm-binding`，仅有 C4251/C4275 已知警告，无错误
- **mine.logging：日志 sink 模块（任务 23）**：
  实现 `mine.logging` 模块，同时为 `mine.diag` 添加 `Logger` 日志接口：
  - **mine.diag::Logger**（新增）：全局日志调度器（Pimpl + MINE_DIAG_API），
    维护最多 16 个并发 sink（固定数组，无堆分配）；
    `set_min_level()`：全局最低级别快速过滤（低于此级别不分发）；
    `add_sink()`：返回注销 token；`remove_sink()`：调用 `destroy_fn` 释放资源；
    `clear_sinks()`：批量注销所有 sink；`write()`：分发到满足级别条件的 sink；
    `flush()`：通知所有 sink 刷新缓冲区
  - **mine.diag::LogSink**（新增）：函数指针描述符（`write_fn/flush_fn/destroy_fn/ctx/min_level`），
    无虚函数、无 RTTI、无异常
  - **mine.diag 宏**（新增）：`MINE_LOG(level, cat, msg)` / `MINE_LOGF(level, cat, fmt, ...)` +
    各级别快捷宏 `MINE_LOG_TRACE/DEBUG/INFO/WARN/ERROR/FATAL` 和 `MINE_LOGF_*`
  - **mine.logging::ConsoleSink**：彩色控制台 sink；
    ANSI 颜色（Trace=灰/Debug=青/Info=绿/Warn=黄/Error=红/Fatal=洋红）；
    `use_stderr_for_warn`：Warn 及以上输出到 stderr；`use_color`：可禁用颜色
  - **mine.logging::FileSink**：文件 sink（C 标准库 `FILE*`，不依赖 mine.io）；
    `append/overwrite` 两种模式；`auto_flush`：每条日志后可选立即 fflush；
    `max_bytes`：最大写入字节数限制（超出后静默丢弃，0=无限制）
  - 输出格式：`[YYYY-MM-DD HH:MM:SS] [LEVEL] [category] message\n`
  - 依赖：`mine.diag` → `mine.core`；`mine.logging` → `mine.diag` + `mine.core`
  - 单测：44 个测试用例 94 个断言，全部通过
- **mine.localization：本地化模块（任务 22）**：
  实现 `mine.localization` 模块，提供多语言资源字典、运行时语言切换和 `tr()` 翻译查询：
  - `LocalizationManager`：核心管理器（Pimpl + MINE_LOCALIZATION_API），
    维护三层回退策略（当前语言 → 回退语言（默认 `"en"`）→ key 原文）
  - 资源包格式支持：
    - JSON 格式（平面对象）：`{"翻译键":"翻译文本",...}`，支持转义序列和 UTF-8，非字符串值跳过
    - KeyValue 格式：`翻译键=翻译文本`（每行一条，`#`/`;` 开头为注释行，空行跳过）
  - `load_catalog(lang, data, fmt)`：加载资源包，多次加载同语言合并（后者覆盖前者重复键）
  - `add_translation(lang, key, value)`：手动注册单条翻译
  - `tr(key)`：翻译查询（未找到返回 key 原文）
  - `tr_format(key, {0}~{3})`：带位置参数替换的翻译查询
  - `tr_plural(key, n)`：单复数查找（n==1 使用 key，n!=1 优先查 `key_plural` 后缀）
  - `subscribe`/`unsubscribe`：语言切换通知（函数指针 + token，swap-with-back O(1) 删除）
  - `global_manager()` 单例 + `tr()`/`tr_format()` 全局快捷函数
  - 依赖：`mine.core`、`mine.containers`
  - 单测：57 个测试用例 99 个断言，全部通过
- **mine.config：分层配置模块（任务 21）**：
  实现 `mine.config` 模块，提供三层优先级配置系统（Env > File > Default）：
  - `ConfigValue`：四种基础值类型（`Null/Bool/Integer/Float/String`），
    字符串使用 `InlineString` 就地存储（无堆分配 ≤23 字节），
    `as_xxx()` 精确类型访问，`to_xxx(fallback)` 带类型提升的宽松转换
  - `ConfigManager`：三层配置管理器（Pimpl + MINE_CONFIG_API），
    `ConfigLayer`（Default=0/File=1/Env=2）优先级递增，高层覆盖低层；
    接口：`set_default`/`load_file`/`load_string`/`reload`/`load_env`/
    `clear_layer`/`clear_all`/`has`/`get`/`which_layer`/`get_bool`/`get_integer`/`get_float`/`get_string`
  - `JsonParser`（内部）：手写递归下降解析器，嵌套对象展平为点分隔键（`a.b.c`），
    支持 null/bool/number/string，数组跳过，处理转义序列
  - `TomlParser`（内部）：逐行扫描解析器，支持注释/节头 `[section]`/键值对/
    基础字符串（带引号 & 字面字符串）/整数/浮点/布尔
  - 环境变量加载：前缀过滤 + 下划线转点分隔，Windows 用 `GetEnvironmentStringsA`
    前向声明（无需 windows.h），POSIX 用 `environ`
  - 依赖：`mine.core`、`mine.containers`（不依赖 mine.io）
  - 单测：41 个测试用例 158 个断言，全部通过
- **mine.nav：路由导航框架（任务 20）**：
  实现 `mine.nav` 模块，提供路由导航所需的所有核心组件：
  - `INavigationService`：导航服务抽象接口，解耦应用层与具体控件，
    定义路由注册（`add_route`）、导航（`navigate_to`）、回退（`go_back`/`can_go_back`）、
    查询（`current_route`）和事件订阅（`subscribe`/`unsubscribe`）等接口
  - `Frame`：路由导航容器控件（继承 `UserControl`，实现 `INavigationService`），
    通过 `set_content()` 切换显示的 `Page`，历史栈使用 `OwnedPtr<Page>` 管理生命周期，
    页面工厂类型 `PageFactory = Page* (*)(void*)` 支持 C 函数指针 + user_data 模式
  - 导航生命周期：`on_navigate_away()` 拦截 → `on_navigated_from()` 旧页离开 → `on_navigated_to()` 新页到达
  - 导航事件广播：`NavigationEventType`（Navigating / Navigated / NavigationFailed / NavigationCancelled）
  - 订阅/取消订阅：token 模式，`unsubscribe` 使用 swap-with-back 保持 O(1)
  - `mine.ui.controls`：`Page` 的三个导航生命周期方法改为 `public virtual`（框架接口设计）
  - 依赖：`mine.core`、`mine.containers`、`mine.math`、`mine.ui.property`、
    `mine.ui.visual`、`mine.ui.controls`
  - 单测：28 个测试用例 67 个断言，全部通过
- **mine.mvvm：MVVM 模式基础模块（任务 19）**：
  实现 `mine.mvvm` 模块，提供 MVVM 架构所需的核心组件：
  - `ObservableObject`：属性变更通知基类，实现 `INotifyPropertyChanged`，
    提供 `set<T>(field, value, name)` 模板（等值不通知）和 `raise(name)` 手动触发，
    通过 Pimpl 隐藏订阅者列表，不可拷贝/不可移动（避免悬空指针）
  - `MINE_OBSERVABLE(Type, Name, ...)` 宏：在 `ObservableObject` 子类内一行声明可观察属性，
    自动生成私有字段、const-ref getter 和 set_ setter（值不变则不触发通知）
  - `ViewModelBase`：MVVM ViewModel 完整基类（继承 `ObservableObject`），
    新增六个生命周期虚方法钩子（默认空实现）：
    `on_initialize()`、`on_navigated_to(param)`、`on_navigated_from()`、
    `on_appear()`、`on_disappear()`、`on_dispose()`；
    框架层（mine.nav Frame/Page）通过对应公开方法触发
  - `ObservableCollectionBase`：集合变更通知非模板基类，实现 `INotifyCollectionChanged`，
    将订阅者管理移入 .cpp 确保 ABI 稳定，提供 `notify_subscribers()` 供子模板类调用
  - `ObservableCollection<T>`：带变更通知的动态数组容器（继承 `ObservableCollectionBase`），
    `add()`、`insert(index,item)`、`remove_at(index)`、`remove(item)`、
    `replace(index,item)`、`move(from,to)`、`clear()` 均自动触发对应 `CollectionChangedAction` 通知
  - `CollectionChangedArgs`：集合变更参数（action / new_index / old_index / count）
  - `INotifyCollectionChanged`：集合变更通知接口（函数指针 + token 订阅模式）
  - 重导出 `mine.ui.event` 的 `ICommand` / `RelayCommand`，无需额外包含头文件
  - 依赖：`mine.core`、`mine.containers`、`mine.ui.binding`（INotifyPropertyChanged）、`mine.ui.event`（ICommand）
  - 单测：22 个测试用例 76 个断言，全部通过
- **mine.di：DI/IoC 容器模块（任务 18）**：
  实现 `mine.di` 依赖注入容器，支持三种服务生命周期管理：
  - `ServiceCollection`：服务注册器，支持链式调用：
    - `add_singleton<ISvc, Impl, Deps...>()`：进程级单例（MINE_INJECT 自动发现依赖）
    - `add_scoped<ISvc, Impl, Deps...>()`：作用域级单例（同 Scope 内共享）
    - `add_transient<ISvc, Impl, Deps...>()`：每次创建新实例
    - `add_instance<ISvc>(ptr)`：预注册实例（不拥有所有权）
    - `add_factory<ISvc>(fn, lifetime)`：用户自定义工厂函数
    - `add_module<Module>()`：批量注册（`IServiceModule` 接口）
    - `build()`：消耗注册表，生成 `ServiceProvider`
  - `ServiceProvider`：服务解析容器（由 `build()` 创建，仅可移动）：
    - `resolve<T>()`：必须已注册，否则断言失败
    - `try_resolve<T>()`：未注册返回 nullptr
    - `create_scope()`：创建子作用域
  - `Scope`：子作用域（Scoped 服务隔离；Singleton 委托给根 Provider）
  - `MINE_INJECT(Class, Deps...)` 宏：在类内声明可注入构造函数，配合 `add_singleton<ISvc, Impl>()` 使用
  - `IServiceModule` 接口：`configure(ServiceCollection&)` 方法，支持模块化批量注册
  - 无 RTTI，使用 `mine::core::TypeId` 作为类型键
  - 正确处理多重继承指针偏移（`FactoryResult` 同时持有 service_ptr 和 impl_ptr）
  - 全部实例按注册逆序析构（LIFO）
  - 单测：21 个测试用例 44 个断言，全部通过
- **mine.ui.controls：Page 基类（任务 17.3）**：
  新增 `Page : public UserControl`，作为 mine.nav 导航系统的页面单元：
  - 导航生命周期虚方法（接口先行，由 F3.1 的 Frame 控件调用）：
    - `on_navigated_to(const Variant& param)`：导航到达时触发，接收导航参数
    - `on_navigated_from()`：离开页面时触发，用于保存状态/取消异步操作
    - `on_navigate_away() → bool`：询问是否允许离开，返回 false 可拦截导航（默认 true）
  - 完全继承 UserControl 的 DataContext、生命周期（on_initialized/on_loaded/on_unloaded）等所有能力
  - 单测：8 个新 Page 测试用例，全部通过（共 41 测试 88 断言）
- **mine.ui.controls：UserControl 基类（任务 17.2）**：
  新增 `UserControl : public ContentControl`，作为所有自定义 UI 组件的基类：
  - `DataContextProperty`（DependencyProperty）：MVVM 数据绑定上下文，`affects_measure=false/affects_render=false`，changed 回调通知 `on_data_context_changed`
  - `set_data_context()` / `data_context()` —— 数据上下文读写接口
  - 生命周期虚方法：`on_initialized()`（首次测量后触发，仅一次）、`on_loaded()`（加入视觉树时触发）、`on_unloaded()`（离开视觉树时触发）
  - `on_data_context_changed(old, new)` —— 数据上下文变更通知，子类可重写
  - `on_content_changed` override：通过直接管理视觉子树（`remove_child`/`add_child`）自动将 `UIElement*` 内容挂入子树
  - `on_parent_changed` override：在父节点变化时调度 `on_loaded`/`on_unloaded`
  - `measure_override`：委托给内容元素，首次调用后触发 `on_initialized`
  - `arrange_override`：将内容元素排列至全部内容区域
  - **mine.ui.visual：Visual 新增 `on_parent_changed` 虚方法钩子**：`add_child`/`remove_child`/`remove_all_children`/析构时自动调用，子类可覆盖以响应加入/离开视觉树事件
  - 单测：8 个新 UserControl 测试用例，全部通过（共 33 测试 74 断言）
- **mine.ui.controls：ContentControl 基类（任务 17.1）**：
  新增 `ContentControl : public Control`，为所有"内容"类控件提供统一基类：
  - `ContentProperty`（DependencyProperty）：存储 `InlineString`（文字）或 `UIElement*`（元素），默认空值；`affects_measure=true/affects_render=true` 自动触发失效
  - `set_content(UIElement* element)` —— 任务要求的标准接口（nullptr 清空内容）
  - `set_content(core::StringView text)` —— 字符串便捷接口
  - `content()` / `content_element()` / `content_text()` —— 读取接口
  - 虚方法 `on_content_changed(old, new)` —— 子类可重写同步私有缓存
  - `Button` 重构为继承 `ContentControl`：`ContentProperty` 上移至基类，`Button::ContentProperty` 通过 `using` 别名向后兼容；`on_content_changed` 改为 `override` 同步 `text_` 缓存；`set_text()` 委托给 `ContentControl::set_content()`
  - 单测：7 个新测试用例，全部通过（共 24 测试 44 断言）
- **mine.ui.animation（AnimationClock）**：新增全局动画时钟，统一管理所有活跃动画的 tick 驱动：
  - Meyer's Singleton 模式（`AnimationClock::instance()`），无全局变量
  - 注册接口 `register_animation(handle, tick_fn)`：以 `void* handle + TickFn` 函数指针回调为注册单元（与框架路由事件、DP changed 回调风格一致，无 `std::function` 堆分配）
  - `tick_all(dt) -> bool`：推进所有已注册动画，回调返回 `false` 时以尾部覆盖方式 O(1) 删除（无顺序保证），方法返回 `true` 表示仍有活跃动画
  - `unregister_animation(handle)`：线性扫描按 handle 删除，析构/停止时安全调用（handle 不存在时幂等）
  - `has_active() const noexcept`：轮询是否有活跃动画
  - 内部使用 `SmallVector<Entry, 8>` 存储，防重入标志 `ticking_` 保护 `tick_all` 内部状态
  - 新增至 `AnimationAll.h` 伞形头文件

### Changed
- **samples/02-controls-demo（改造为继承模式+code-behind五文件分工）**：
  将示例从组合模式（持有 `win_` 成员的包装类）重构为继承模式，完整体现 `docs/04-precompiler.md §4.4.2` 的设计契约：
  - `mine::ui::Window::~Window()` 改为 `virtual`，支持 `DemoWindowBase` 继承
  - 新增 `DemoWindow.g.h`：`DemoWindowBase : public mine::ui::Window`，`~DemoWindowBase()` 析构体首句 `close()`，`on_count_clicked/on_reset_clicked` 纯虚接口，`status_label_` protected 引用（`#id` 暴露）
  - 新增 `DemoWindow.g.cpp`：视觉树构建全部调用继承自 Window 的方法（无 `win_.` 前缀），静态处理器多态分派到 method
  - `DemoWindow.h/.cpp`（code-behind）：`DemoWindow final : DemoWindowBase`，实现两个 method，直接访问 `status_label_` 和 `render()`
  - 更新 `xmake.lua`：`add_files` 新增 `DemoWindow.g.cpp`
- **设计文档（docs/03-mml-language.md、04-precompiler.md、07-windowing.md）**：Window 组件架构从"组合模式"升级为"继承模式 + code-behind 五文件分工"：
  - `mine::ui::Window` 改为虚析构，支持继承和多态
  - mmlc 生成 `XxxBase : public mine::ui::Window`，析构安全由 `~XxxBase()` 中首句 `close()` 保证
  - 新增 `method` 关键字（BNF + §3.2 关键字表）：声明纯虚 code-behind 接口
  - 新增 `#id` 暴露语义：标记元素以 `protected` 引用成员形式暴露给 code-behind
  - 定义五文件分工契约：.mml / .g.h / .g.cpp / .h / .cpp
  - mmlc 文件发现规则：检测同目录 `.h` 文件判断 code-behind 存在
- **samples/02-controls-demo（重构为 Window 组件样板代码分层结构）**：
  将单文件混合结构拆分为符合 `docs/04-precompiler.md §4.4.2` 规定的代码生成契约：
  - 新增 `DemoWindow.h`：Window 组件包装类头文件（模拟 `.g.h`），聚合 `mine::ui::Window win_` 值成员（最后声明=最先析构），暴露 `show/hide/close/is_closed/window()` 委托接口
  - 新增 `DemoWindow.cpp`：包装类实现（模拟 `.g.cpp`），`_configure()` 设置窗口 pending 属性，`_build(font)` 完整视觉树构建，`_bind()/_states()` 占位实现
  - 精简 `main.cpp` 为薄壳 `DemoApp`（<50行），`on_startup()` 仅含 `setup(font)` + 信号订阅 + `show()`
  - 更新 `xmake.lua`：`add_files("main.cpp", "DemoWindow.cpp")`
- **mine.ui.visual + mine.ui.layout（架构修正：FrameworkElement/Control 归还 visual 层）**：
  将 `FrameworkElement` 和 `Control` 从 `mine.ui.layout` 迁回 `mine.ui.visual`，符合设计文档 §02-modules 规定：
  - 继承关系修正为 `Visual → UIElement → FrameworkElement → Control`（全部在 mine.ui.visual）
  - `HorizontalAlignment`/`VerticalAlignment` 枚举随 FrameworkElement 迁至 visual，layout 保留向后兼容转发头
  - 新增 `src/mine/ui/visual/api/include/mine/ui/visual/HorizontalAlignment.h`
  - 新增 `src/mine/ui/visual/api/include/mine/ui/visual/VerticalAlignment.h`
  - 新增 `src/mine/ui/visual/api/include/mine/ui/visual/FrameworkElement.h`（导出宏 `MINE_UI_VISUAL_API`）
  - 新增 `src/mine/ui/visual/src/FrameworkElement.cpp`（完整布局协议实现）
  - `visual/Control.h` 还原为完整类定义（`MINE_UI_VISUAL_API`，继承 `FrameworkElement`）
  - `visual/Control.cpp` 还原为完整实现（`measure_override`/`arrange_override`/VSM/模板绑定）
  - `layout/FrameworkElement.h` / `layout/Control.h` 改为向后兼容转发头（包含路径变更透明）
  - `layout/FrameworkElement.cpp` / `layout/Control.cpp` 清空（实现已迁至 visual）
  - `mine.ui.visual/xmake.lua` 新增 `mine.ui.style` 依赖（Control 需要 ControlTemplate/VSM）
  - `mine.ui.layout/xmake.lua` 移除 `mine.ui.style` 依赖（layout 不再包含 Control）
  - `VisualAll.h` 新增 HorizontalAlignment/VerticalAlignment/FrameworkElement 头文件
  - `LayoutAll.h` 移除 Control 包含（layout 层不再拥有 Control）
  - 测试全部通过：layout 27/27、visual 53/53、controls 16/16

- **mine.ui.layout（迁移第二步：Control 迁移至 FrameworkElement）**：将 `Control` 基类物理迁移至 `mine.ui.layout` 模块，使所有控件自动参与两遍布局协议：
  - `Control` 继承链由 `UIElement → Control` 升级为 `UIElement → FrameworkElement → Control`
  - 新增 `src/mine/ui/layout/api/include/mine/ui/layout/Control.h`（继承 `FrameworkElement`，导出宏 `MINE_UI_LAYOUT_API`）
  - 新增 `src/mine/ui/layout/src/Control.cpp`（`on_measure` 改为 `measure_override`，`on_arrange` 改为 `arrange_override`）
  - `src/mine/ui/visual/api/include/mine/ui/visual/Control.h` 改为向后兼容转发头（仅保留注释 + `#include <mine/ui/layout/Control.h>`）
  - `src/mine/ui/visual/src/Control.cpp` 清空（实现已迁移至 layout 模块）
  - `mine.ui.layout/xmake.lua` 新增 `mine.ui.style` 依赖；`mine.ui.visual/xmake.lua` 移除 `mine.ui.style` 依赖
  - `mine.ui.visual.test` 目标追加 `mine.ui.layout` 依赖以访问迁移后的 Control 实现
  - 测试全部通过：layout 27/27、visual 53/53、controls 16/16

- **mine.ui.layout（迁移第一步）**：布局容器子元素类型从 `FrameworkElement*` 放宽为 `UIElement*`，打通现有 `Button/TextBlock` 直接接入 `StackPanel/Grid` 的能力：
  - `Panel::add_child/remove_child/child_at` 与 `children_` 改为 `UIElement*`
  - `Grid` 附加属性便捷接口（`get_row/set_row/get_column/...`）参数从 `FrameworkElement` 放宽为 `UIElement`
  - `StackPanel/Grid` 内部布局循环统一按 `UIElement*` 处理子元素

- **mine.ui.controls / Button（F2 技术债清理）**：完成 §21.8 全部技术债清理：
  - **新增 4 个 DependencyProperty**：`ForegroundProperty`（白色默认值）、`BorderColorProperty`（透明默认值）、`HoveredBackgroundProperty`（MD3 Primary+8% state layer）、`PressedBackgroundProperty`（MD3 Primary+12% state layer）
  - **移除 4 个 plain member**：`foreground_`、`background_hover_`、`background_press_`、`border_color_` 全部替换为对应 DP 的 `get_value()`/`set_value()` 读写
  - **新增 `on_foreground_changed` 静态回调**：前景色变更时自动推送到 `ContentPresenter`（模板未构建时静默跳过）
  - **删除公开 API `has_active_animation()` 和 `tick_bg_animation(dt)`**：改由 `AnimationClock` 统一驱动
  - **新增私有 `anim_tick_callback()`**：统一推进 Ripple elapsed_ms 和 bg_storyboard_；返回 `false` 时 `AnimationClock` 自动移除注册
  - **Ripple 去 chrono**：`RippleState::start`（`chrono::time_point`）改为 `elapsed_ms`（`float`），由 `anim_tick_callback` 每帧以 `dt * 1000.0f` 累加；消除 `<chrono>` 依赖
  - **`on_mouse_down`**：启动 Ripple 时调用 `AnimationClock::instance().register_animation(this, &Button::anim_tick_callback)`
  - **`on_visual_state_changed`**：读取 `HoveredBackgroundProperty`/`PressedBackgroundProperty` DP 作为过渡目标色；状态切换后调用 `AnimationClock::instance().register_animation(this, &Button::anim_tick_callback)`
  - **析构函数**：调用 `AnimationClock::instance().unregister_animation(this)` 清理注册

- **samples/02-controls-demo（改为 AnimationClock 驱动）**：
  - 删除对各 `Button::tick_bg_animation()` 和 `Button::has_active_animation()` 的直接调用
  - `ripple_timer_proc` 改为调用 `anim::AnimationClock::instance().tick_all(kDt)` 统一推进所有动画
  - 鼠标/键盘事件后改为检查 `AnimationClock::instance().has_active()` 决定是否启动定时器

- **mine.ui.controls / Button（Material Design 3 Filled Button 外观）**：将 Button 默认模板外观重设计为 Material Design 3 Filled Button 风格：
  - 背景色改为 MD3 Primary（#6750A4，蓝紫色）；Hovered 叠加 OnPrimary 8% state layer；Pressed 叠加 12% state layer
  - 文字颜色改为 MD3 On Primary（白色），Disabled 状态降至 OnSurface 38% 半透明
  - 形状改为完全圆角（`radius = height / 2`，胶囊形），使用 `fill_rounded_rect` 渲染
  - 删除矩形实心边框（Filled Button 无边框）
  - 默认水平内边距从 12px 升至 24px，垂直 8px 升至 10px（符合 MD3 尺寸规范）
  - Disabled 背景改为 OnSurface 12% 半透明；`set_enabled` 同时触发 `invalidate_measure` 以刷新文字颜色

### Fixed
- **samples/02-controls-demo 动画延时与中断卡顿修复**：
  - **根因 1（延时）**：原实现在状态切换 → `register_animation` 后调用 `ui_win_->render()`，再等待 Win32 定时器下一次触发（最多 16ms 后）才第一次 `tick_all`；期间按钮冻结在 `from_brush`，用户感知到明显的响应延迟。
  - **根因 2（卡顿）**：`ripple_timer_proc` 使用硬编码 `kDt = 16ms`，实际定时器精度约 ±4ms，动画速度与期望不符；动画打断时新状态同样须等下一定时器周期，导致视觉冻结后突跳。
  - **根因 3（跳色）**：三个自定义颜色按钮（蓝/蓝灰/红）未调用 `set_background_hovered`，Hover 过渡默认使用 MD3 紫色（`#735BAC`），产生强烈的色调跳变，加剧卡顿感。
  - **修复**：
    - 新增 `DemoApp::last_anim_tick_ms_`（`DWORD`）记录上次 tick 的真实系统时间戳。
    - 新增 `tick_animations_and_render()` 辅助方法：使用 `GetTickCount()` 计算实际 `dt`（首帧默认 16ms，后续取真实帧间隔，最大钳制 100ms 防调试/最小化大跳帧）；每次调用仅推进上次 tick 后经过的真实时间，事件与定时器两条路径共享 `last_anim_tick_ms_` 保证不重复计时。
    - 事件处理器（`on_window_event`）改为调用 `tick_animations_and_render()` 代替直接 `render() + SetTimer`：状态切换后立即 tick，首帧动画在同一消息处理轮次内渲染，消除起始延迟。
    - `ripple_timer_proc` 委托给 `tick_animations_and_render()`：不再使用硬编码 dt，定时器与事件共享时间戳，打断时下一 tick 精确消耗打断至 tick 之间的实际间隔，无突跳。
    - 为 `btn_count/btn_reset/btn_quit` 补充 `set_background_hovered`（分别为 Material Blue 400 `#1E88E5`、Blue Grey 600 `#546E7A`、Red 700 `#D32F2F`），Hover 过渡不再错误跳变为 MD3 紫色。

- **samples/02-controls-demo 崩溃修复（0xC0000005 悬空指针）**：`on_startup` 中 `font_face` 原为局部 `OwnedPtr`，`on_startup` 返回后字体对象被销毁，后续鼠标事件触发第二次渲染时各控件的 `font_face_` 成为悬空指针（use-after-free），导致 STATUS_ACCESS_VIOLATION 崩溃。修复方法：将字体资源提升为 `DemoApp::font_face_` 成员变量（`OwnedPtr<text::FontFace>`），生命周期与 `DemoApp` 一致，覆盖全部渲染周期，彻底消除悬空指针。

### Added
- **mine.ui.controls / Button（MD3 Ripple 涟漪动画）**：为 Button 控件新增 Material Design Ripple 点击波纹效果：
  - `Button` 新增私有 `RippleState` 成员（圆心局部坐标、`steady_clock::time_point` 动画起始时刻、活跃标志）
  - 鼠标按下（`on_mouse_down`）时记录鼠标位置为涟漪圆心，触发 ripple 动画
  - `on_render` 中在背景圆角矩形之上、文字之下绘制涟漪圆：通过 `canvas.save()`/`clip_rounded_rect`/`fill_circle`/`restore` 裁剪到胶囊边界；半径从 0 扩展至按钮对角线的 60%，alpha 从 0.24 线性淡出到 0，总时长 400ms
  - 新增公开只读接口 `has_active_ripple() const noexcept` 供外部驱动器查询动画是否仍在播放
  - `samples/02-controls-demo`：在 MouseDown 事件处理中使用 Win32 `SetTimer(nullptr, 0, 16, ripple_timer_proc)` 启动约 60fps 的渲染驱动定时器；`ripple_timer_proc`（TimerProc 形式，不依赖窗口过程）每帧检查三个按钮的 `has_active_ripple()`，有活跃 ripple 则调用 `ui_win_->render()` 驱动下一帧，所有 ripple 结束后自动 `KillTimer` 停止轮询；应用退出时在 `on_exit` 中清理定时器

- **samples/02-controls-demo（控件交互演示示例）**：新增窗口+控件交互演示程序（F3.3 任务 #28.1）：
  - 采用 `DemoRoot : FrameworkElement` 方案，在 `measure_override` / `arrange_override` 中对 6 个控件（2 个 TextBlock + 3 个 Button + 1 个状态 TextBlock）实施手动绝对坐标布局
  - 通过 `Visual::add_child()` 将 Button / TextBlock 实例纳入视觉树（规避 `Panel::add_child` 只接受 `FrameworkElement*` 的限制）
  - 演示三个按钮交互：计数+1（更新状态文字）、重置（归零）、退出（调用 `app_host->quit()`）
  - 演示 `InputRouter::set_root()` + `IWindowEventSink` 鼠标/键盘转发完整链路
  - 字体回退：优先 Arial；回退微软雅黑（支持中文字符渲染）

- **mine.ui.animation（Storyboard 属性动画）**：实现 F2.2 任务 #17 的属性动画与 VSM 过渡集成：
  - 新增 `PropertyAnimation` 结构体：描述单个依赖属性动画（target/prop/from/to/duration/easing/elapsed/complete）；`to_is_resolved` 标志区分显式 to（`animate_dp_to`）与状态采样（`animate_dp`）两条路径
  - 新增 `Storyboard` 类：属性动画时间线容器，支持多属性并行动画：
    - `animate_dp(target, prop, duration, easing)` —— 自动 to（`resolve_and_begin` 时从对象采样）
    - `animate_dp_to(target, prop, to, duration, easing)` —— 显式 to
    - `capture_from_values()` —— 在 StyleTrigger 写入前采样所有动画的起始值
    - `resolve_and_begin()` —— 解析 to 值（对 `to_is_resolved=false` 的动画从对象当前值读取），然后以 `ValuePriority::Animation(60)` 写入 from 值，启动动画
    - `tick(dt)` —— 推进所有动画，返回 `true` 表示全部完成；内部调用 `lerp_variant`（支持 `float`/`math::Color`/`math::Thickness`/任意离散类型）
    - `stop()` —— 清除所有属性的 `Animation` 优先级槽
  - 内部 `lerp_variant`：float 线性插值 / Color 四分量各自插值 / Thickness 四边各自插值 / 其他类型 `t>=1.0f` 时 snap 到 to
  - 更新 `AnimationAll.h` 伞形头文件，新增 `PropertyAnimation.h` / `Storyboard.h`
  - `xmake.lua` 追加 `mine.ui.property` 依赖（DependencyObject / DependencyProperty / ValuePriority）
  - 新增 11 个 Storyboard 单元测试（51/355 总计），覆盖：动画注册计数、空 Storyboard 立即完成、Animation 层写入 from、Linear 中间进度插值、完成时精确等于 to、stop 清除 Animation 层、Color / Thickness 各分量插值、自动 to 从对象读取、超时 tick 夹紧到 to、多属性并行动画均完成才返回 true；51/51 全部通过

- **mine.ui.style（VisualStateManager 动画集成）**：Task #17 验收：VSM 与 Storyboard 过渡集成：
  - 新增 `add_transition(from, to, configure_fn)` 重载：注册带 `core::Function<void(Storyboard&)>` 配置函数的动画过渡
  - 新增 `tick_animations(dt) -> bool`：推进所有活跃 Storyboard，返回 `true` 表示仍有活跃动画；完成的 Storyboard 自动调用 `stop()`（清除 Animation 层）并从 `active_storyboards_` 移除（SmallVector 双指针压缩策略，无 `erase`）
  - `go_to_state` 扩展流程：有 configure_fn 过渡时：创建 Storyboard → configure → `capture_from_values` → `apply_state`（写 StyleTrigger） → `resolve_and_begin`（读 to，写 Animation=from） → 压入 `active_storyboards_`；无 configure_fn 过渡时：直接 `apply_state`（立即跳变）；状态切换时自动停止旧动画
  - `VisualStateTransition` 扩展为 move-only：新增 `configure` 字段（`core::Function<void(Storyboard&)>`，SBO 32 字节）
  - 新增 `active_storyboards_`（`SmallVector<OwnedPtr<Storyboard>, 4>`）私有成员
  - `xmake.lua` 追加 `mine.ui.animation` 依赖
  - 新增 7 个 VSM 动画集成测试，覆盖：带/不带 configure_fn 的 has_transition、动画启动后 Animation 层写入 from、tick 推进中间值、动画完成后 tick 返回 false、无 configure_fn 立即跳变、中断活跃动画并替换、无活跃动画返回 false；56/56 全部通过

- **mine.ui.animation（缓动函数库）**：实现 F2.2 任务 #16 的缓动函数库：
  - 新增 `Duration` 类：时间段描述，内部以秒存储；`milliseconds(ms)` / `seconds(s)` 工厂函数，支持加减乘算术、比较运算、`is_zero()` 查询
  - 新增 10 个缓动函数系列（In / Out / InOut 各变体，共 22 个全局函数指针常量）：
    `Linear`（1）、`Quad`（3）、`Cubic`（3）、`Quart`（3）、`Quint`（3）、`Expo`（3）、
    `Sine`（3）、`Back`（3）、`Elastic`（3）、`Bounce`（3）
  - 新增 `CubicBezier` 类：参数化三次贝塞尔缓动曲线；牛顿迭代逆映射（最多 8 次，精度 1e-6）；
    静态预置常量 `Ease` / `EaseIn` / `EaseOut` / `EaseInOut`（与 CSS 规范一致）
  - 新增 `SpringEasing` 类：有状态弹簧物理缓动模拟器；半隐式欧拉积分（先速度后位置，能量守恒好）；
    支持 `retarget(new_target)` 中途改变目标而保持当前速度；`is_settled()` 检测稳定；
    `damping_ratio()` / `natural_frequency()` / `estimated_settle_time()` 参数查询
  - 新增 `AnimationAll.h` 伞形头文件，一键引入所有公共类型
  - 新增 40 个单元测试（316 个断言）覆盖：端点与帧序列值精确性、Back/Elastic/Bounce 超调特征、
    CubicBezier 线性退化与 CSS 预置方向性、SpringEasing 欠阻尼振荡稳定 / 临界阻尼无振荡 /
    过阻尼单调性 / retarget 保持速度 / is_settled 零偏移零速度判定；全部通过

- **mine.ui.controls（ContentPresenter）**：实现 F2.1 任务 #15 的模板内容占位控件：
  - 新增 `ContentPresenter` 类（继承 `Control`），作为 `ControlTemplate` 视觉树中的内容占位元素
  - `ContentProperty`：`DependencyProperty`，接受 `InlineString`（文字模式）或 `UIElement*`（未来元素模式）；`on_measure` 按内容类型动态计算期望尺寸
  - `PaddingProperty`：`DependencyProperty`，类型 `math::Thickness`，计入内容区域 measure 结果
  - M1.1 阶段：文字内容以水平占位线渲染（`fill_rect`），元素模式渲染委托给视觉子树
  - 新增至 `ControlsAll.h` 伞形头文件

- **mine.ui.controls（Button → 模板路径）**：`Button` 迁移至样式模板渲染路径：
  - 注册 `ContentProperty`（默认空 Variant）和 `PaddingProperty`（默认 `Thickness::symmetric(12, 8)`）为 `DependencyProperty`
  - 注册 C++ 默认 `ControlTemplate`（`TemplateRegistry`，键 `"DefaultButtonTemplate"`），内部创建 `ContentPresenter` 并绑定 `Content`/`Padding` 属性
  - 修改 `on_measure`：有模板根时委托给 `Control::on_measure`（模板路径），无模板根时回退旧路径
  - 修改 `on_render`：背景/边框始终绘制，有模板根时不再手动绘制文字占位线

- **mine.ui.controls（TextBlock → DependencyProperty）**：`TextBlock` 关键属性迁移至 DP 系统：
  - 新增 `TextProperty`/`FontSizeProperty`/`ForegroundProperty`/`BackgroundProperty`/`PaddingProperty` 为 `DependencyProperty`
  - 各 setter 同步更新 DP 值；`changed` 回调同步成员缓存；`on_measure`/`on_render` 仍使用成员缓存（TextBlock 不使用模板路径）

- **mine.ui.visual（Control 模板所有权）**：`Control` 基类强化模板根管理能力：
  - 新增 `set_template_root(OwnedPtr<UIElement>)` 重载：将模板根元素的生命周期纳入控件管理，模板重建时自动释放旧根
  - 新增模板辅助 inline 重载 `set_template_root<TElement>(OwnedPtr<TElement>)`：通过 `get_deleter()` + `release()` 类型擦除后委托基类重载，支持 `OwnedPtr<ContentPresenter>` 等子类型直接传入
  - `on_measure` 首次测量时自动从 `TemplateRegistry` 按 `template_slot_` 查找并构建模板，之后委托给模板根的 `measure()`

- **测试**：新增 8 个 doctest 测试用例（controls 模块），16/16 全部通过，覆盖 ContentPresenter 空/文字内容尺寸与渲染、Padding 影响、Button 模板路径 measure 委托、TextBlock DP 同步


  - `register_theme(name, ResourceDictionary&&)`：注册命名主题资源字典（堆分配，地址稳定，支持 global_resources_ 弱引用合并）；同名重复注册时覆盖旧字典，若为当前激活主题则重新广播 `resource_changed("*")`
  - `set_theme(name) -> bool`：切换当前激活主题；未注册名称静默返回 false，不改变任何状态；成功则依次：`clear_merged()` → `merge(*theme.dict)` → 更新 `current_theme_name_` → `notify_resource_changed("*")`
  - `current_theme() -> StringView`：返回当前主题名（未设置时为空串）
  - `global_resources()` / `global_resources() const`：获取应用级根资源字典（供窗口/控件字典通过 `set_parent()` 接入）
  - `Application::Impl` 新增：`ThemeEntry`（`InlineString name + OwnedPtr<ResourceDictionary> dict`）、`themes_`（`SmallVector<ThemeEntry, 4>`）、`current_theme_name_`（`InlineString`）、`global_resources_`（`ResourceDictionary`）
  - 主题字典使用 `OwnedPtr` 堆分配：保证 `SmallVector` 扩容后地址稳定，`global_resources_.merge()` 弱引用不悬空
  - `xmake.lua` 追加 `mine.ui.style` 依赖（ResourceDictionary）
  - `Application.h` 追加 `#include <mine/core/StringView.h>`，前向声明 `mine::ui::style::ResourceDictionary`（参数类型为 `&&` 避免不完整类型按值传递限制）
  - 新增 19 个单元测试（doctest），26/26 全部通过，覆盖：初始状态、set_theme 未注册返回 false、current_theme 跟踪、global_resources 资源查找、**核心验收：Light→Dark→Light 多次切换无崩溃**、resource_changed 回调触发、同名主题覆盖、未注册切换不影响已激活主题

- **mine.ui.style（VisualStateManager）**：实现 F2.1 任务 #13 的视觉状态机核心 API：
  - 新增 `VisualStateTransition` 结构体：记录 `from`/`to` 状态名（`from` 为 `"*"` 或空串时通配任意来源状态）
  - 新增 `VisualStateManager` 类：状态定义、过渡配置、运行时切换的状态机管理器
    - `define_state(StringView name)`：注册合法状态名，重复注册安全幂等
    - `add_transition(from, to)`：注册过渡规则（from 可为 `"*"` 表示任意来源）
    - `go_to_state(state_name, use_transitions=true)`：切换当前状态；状态未注册 / 已是当前状态返回 false；切换成功后自动调用 `style->apply_state(owner, state_name)`（StyleTrigger 优先级 30）
    - `current_state()`：返回当前状态名（`StringView`），初始为空串
    - `has_state(name)` / `has_transition(from, to)`：查询接口
    - `set_style(style*)` / `attached_style()`：关联 Style 对象，供 go_to_state 回调
    - 所有者存为指针（`DependencyObject*`）以支持移动语义
    - `use_transitions` 参数保留但 M2 阶段暂忽略（Task #17 补充 Storyboard 动画）
  - 扩展 `mine.ui.visual::Control`：
    - `set_visual_state_manager(VisualStateManager vsm)`：安装状态机实例（`OwnedPtr<VisualStateManager>`）
    - `vsm()` / `vsm() const`：获取已安装的 VSM 指针（未安装返回 nullptr）
    - `compute_state_name() const noexcept`：枚举 `ControlVisualState` → 字符串名（Normal / Hovered / Pressed / Focused / Disabled）
    - 更新 `update_visual_state()`：若 VSM 已安装则先调用 `vsm->go_to_state(compute_state_name())`，再执行原有枚举状态切换逻辑
  - 更新 `StyleAll.h`：新增 `VisualStateManager.h` 伞形引入
  - 新增 15 个单元测试（doctest），49/49 全部通过，覆盖：初始化与状态查询、**核心验收 Normal→Hovered 正确触发**、状态定义约束（重复/未注册）、过渡注册与查询（通配 *）、样式 setter 集成（apply_state 实际写属性值）、移动语义、多状态切换

- **mine.ui.style（ControlTemplate + TemplateRegistry）**：实现 F2.1 任务 #12 的控件模板系统：
  - 新增 `ControlTemplate` 类：封装控件视觉树构建函数（`BuildFn = void(*)(DependencyObject&)`）和元数据（名称、目标类型）
    - `build(DependencyObject& target)`：调用 `build_fn_`，若为 nullptr 则安全跳过
    - `set_name()` / `set_target_type()`：构建器接口，返回 `*this` 支持链式调用
    - `build_fn_` 公开成员：在 build_fn_ 内部通过 `static_cast<Control&>(target)` 还原控件类型（避免 mine.ui.style 对 mine.ui.visual 循环依赖）
  - 新增 `TemplateRegistry` 类（全局单例 Pimpl 模式）：模板注册与查找中心
    - `instance()`：函数内静态单例，线程安全（C++11 保证）
    - `register_template(name, target_type, build_fn)`：注册模板，返回堆上对象的常量引用（引用永久稳定，`Vector<OwnedPtr<ControlTemplate>>` 扩容不影响地址）
    - `find(name)`：按名称查找（从后往前，返回最后注册的同名模板）
    - `find_default(type_id)`：按目标 TypeId 查找
  - 扩展 `UIElement`：新增公开成员 `set_template_name(StringView)` / `template_name()`，用于在模板子树中标识特定元素（供 `Control::find_template_child` 搜索）
  - 扩展 `Control`：新增控件模板运行时 API：
    - `set_template_root(UIElement*)`：将模板根元素加入控件的视觉子树（先移除旧根），模板根生命周期由调用方管理
    - `find_template_child(StringView)`：从模板根开始深度优先搜索，返回第一个匹配 `template_name` 的 UIElement；未找到返回 nullptr；支持任意深度嵌套
    - `bind_template(child, child_prop, host_prop)`：建立单向 TemplateBinding：立即将宿主属性值以 `ValuePriority::TemplateBind(40)` 同步到子元素，并通过 `subscribe_property_changed` 订阅后续变更（仅订阅一次，所有绑定共享同一订阅 token）
    - `Control::Impl`（私有 Pimpl）：存储模板根指针、绑定列表（`SmallVector<TemplateBinding, 8>`）、订阅 token
    - `Control::~Control()`：自动取消属性变更订阅（在 `Impl` 析构前执行，防止悬空回调）
  - 优先级保证：`TemplateBind(40)` 低于 `Local(50)`，模板绑定值不覆盖控件本地设置值
  - 更新 `mine.ui.visual/xmake.lua`：追加 `mine.ui.style` 依赖
  - 更新 `StyleAll.h`：新增 `ControlTemplate.h` / `TemplateRegistry.h` 伞形引入
  - 修复 `InlineString.h`：新增 `#include <new>`，解决 MSVC C++17 对 dllexport 类的 `operator=(InlineString&&)` 触发 3 参数对齐 placement new 的编译错误
  - 新增 13 个单元测试（doctest），53/53 全部通过，覆盖：ControlTemplate 构建器接口、build() 触发回调、nullptr build_fn_ 安全、TemplateRegistry 注册+find、find_default、未注册返回 nullptr、UIElement template_name 存取、set_template_root 加入子树、remove_child 清除根、find_template_child 匹配/不存在/嵌套深度、bind_template 立即同步、属性变更传播、**核心验收：build→bind→find 全流程集成测试**

- **mine.ui.style（Style + StyleSetter）**：实现 F2.1 任务 #11 的 `Style` 与 `StyleSetter`：
  - 新增 `StyleSetter` 结构体：描述"将某依赖属性设置为指定值"的操作，支持静态值（`value` 字段）与 DynamicResource 键（`res_key` 字段）两种模式
  - 新增 `VisualStateSetters` 结构体：单个视觉状态下的 setter 集合（`state_name` + `SmallVector<StyleSetter, 8>`）
  - 新增 `Style` 类：实现依赖属性值优先级链的样式层
    - `apply(DependencyObject&)`：以 `ValuePriority::StyleSetter(20)` 写入属性值；先应用父样式（BasedOn 继承），再应用本样式（子样式覆盖同属性父样式值）；`apply_fn_` 非空时使用 mmlc 生成路径
    - `apply_state(DependencyObject&, StringView)`：以 `ValuePriority::StyleTrigger(30)` 写入指定视觉状态的 setter；状态不存在时为空操作
    - 构建器接口：`set_name()` / `set_target_type()` / `set_base()` / `add_setter()` / `add_state_setters()`
    - `apply_fn_` 公开成员（`ApplyFn = void (*)(DependencyObject&)`）：为 mmlc 生成代码预留入口
  - 优先级链保证：`StyleSetter(20)` 低于 `Local(50)`，样式值不覆盖本地设置值；`StyleTrigger(30)` 可覆盖 `StyleSetter(20)`，仍低于 `Local(50)`
  - 更新 `xmake.lua`：追加 `mine.ui.property` 依赖（`Style::apply()` 需要 `DependencyObject` + `ValuePriority`）
  - 更新 `StyleAll.h`：新增 `StyleSetter.h` / `Style.h` 的伞形引入
  - 新增 17 个单元测试（doctest），全部通过，覆盖：结构体默认构造、静态值/DynamicResource 键构造、VisualStateSetters 构造、构建器接口、apply() 写入优先级、**核心验收（Local 值不被样式覆盖）**、逆向验收（apply 后设本地值有效）、apply_state() 写入 StyleTrigger、StyleTrigger 不覆盖 Local、BasedOn 继承、apply_fn_ 路径、状态不存在空操作、Style 可拷贝、set_base(nullptr) 清除继承、多属性并行 setter、查询接口

- **mine.ui.style（树形资源字典）**：实现 F2.1 任务 #10 的 `ResourceDictionary`：
  - 新增 `ResourceDictionary`（Pimpl 模式）：多层键值资源存储，支持树形查找链
    - `set(key, value)`：写入静态值（不触发通知，适用于批量初始化 / 主题加载）
    - `set_dynamic(key, value)`：写入动态值，触发特定 key 的 `subscribe()` 回调并广播 `resource_changed`
    - `merge(other)` / `clear_merged()`：合并外部字典（弱引用）；查找时按本层 → 合并层 → 父层的顺序
    - `find(key)`：树形查找，未命中返回空 `Variant`（调用方用 `has_value()` 判断）
    - `find_local(key)`：仅查本层
    - `subscribe(key, cb)` / `unsubscribe(id)`：订阅特定键的值变更（DynamicResource 场景）
    - `set_parent(parent)`：连接父字典，自动订阅父层的 `resource_changed` 以传播变更
    - `on_resource_changed(cb)` / `off_resource_changed(id)`：订阅任意键的广播通知
    - `notify_resource_changed(key)`：手动广播（主题切换后调用，`key="*"` 表示全量刷新）
  - 内部存储：`Vector<Entry>`（本层）、`SmallVector<const ResourceDictionary*, 4>`（合并层）、`Vector<KeySub>`（move-only Function 订阅）、`SmallVector<ChangedSub, 4>`（广播订阅），均避免使用未特化的 `Hash<InlineString>`
  - 新增 `HandlerId`（`uint32_t`）、`kInvalidHandlerId = 0`、伞形头 `StyleAll.h`
  - 新增 17 个单元测试（doctest），全部通过，覆盖：本层写入/查找、未命中返回空 Variant、三层树形查找、合并层查找、优先级顺序（本层 > 合并层 > 父层）、clear_merged、set 不触发通知、set_dynamic 触发订阅、unsubscribe、resource_changed 广播/取消、父层变更传播到子层、子层本层覆盖时父层不触发、析构自动断开父层订阅、notify_resource_changed 手动广播

- **mine.ui.controls（基础控件）**：实现 M1.1 任务 #23 的最小可用基础控件模块：
  - 新增 `TextBlock`：支持文本内容、字体大小、前景/背景色、内边距和基础测量；支持可选字体对象绘制（未设置字体时使用占位线保证可见输出）
  - 新增 `Button`：支持文本、启用状态、按下态、基础外观绘制（背景/边框）与 `Click` 路由事件；接入 `MouseDown/MouseUp` 输入事件触发点击
  - 新增 `Border`：支持单子元素容器、边框厚度/颜色、背景色，完成子元素测量与排列传递
  - 新增 `mine.ui.controls` 模块伞形头 `ControlsAll.h`，并复用 `mine.ui.layout` 中的 `StackPanel/Grid`
  - 在 `mine.ui.visual::Control` 预留样式/模板/视觉状态挂点：`style_slot`、`template_slot`、`update_visual_state()`，为后续 `mine.ui.style`（任务 #38）接入做接口准备
  - 新增 8 个单元测试（doctest）并全部通过，覆盖 TextBlock 测量与渲染、Button 点击行为、Border 布局传递、伞形头可用性及样式模板槽位

- **mine.ui.app（应用程序宿主与主循环管理）**：实现 Application 基类（M1.1 任务 #22）：
  - `Application`（Pimpl 模式）：整合平台宿主、图形基础设施和窗口生命周期的完整应用层容器
    - `run(argc, argv)`：按顺序初始化 `IApplicationHost` → `IDevice` → `IQueue(Graphics)` → `IRenderer`，调用 `on_startup`，进入 `IApplicationHost::run()` 主循环，返回后调用 `on_exit`，完成清理（取消主窗口订阅 → 销毁所有窗口）
    - `quit(int)`：委托到 `IApplicationHost::quit()`
    - `create_window(WindowDesc)`：通过宿主创建原生 IWindow，包装为 `ui::Window`（创建 ISwapchain、订阅事件），存入内部 `SmallVector<WindowEntry, 4>`
    - `set_main_window(Window*)`：取消旧主窗口订阅 → 订阅新主窗口原生 Closed 事件，Closed 时自动调用 `IApplicationHost::quit(0)`
    - `host() / device() / renderer()`：在 `on_startup` 中访问基础设施引用
    - 受保护生命周期扩展点（可在子类覆盖）：`on_startup / on_exit / on_create_host / on_create_device / on_create_renderer`
  - `MINE_APPLICATION_MAIN(AppClass)` 宏：展开为标准 `int main(int argc, char** argv)`，一行声明应用入口
  - 便捷总头文件 `AppAll.h`：一次 include 覆盖全部 API
  - 14 个单元测试（doctest），覆盖：run() 前未调用 on_startup、on_startup 在主循环前被调用、on_exit 在 run() 后被调用、on_exit 接收正确退出码、run() 返回值等于宿主退出码、on_startup 收到正确 argc、quit() 委托到宿主、create_window() 返回非空指针、create_window() 创建的窗口初始 is_closed=false、set_main_window() 订阅后 sink 非空、主窗口 Closed 不触发（run() 已结束）、消息循环中窗口关闭触发 quit(0)、on_startup 中可访问 host/device/renderer
  - 同步修复 `SmallVector<T>::push_back(const T&)` 和 `Vector<T>::push_back(const T&)` 添加 `requires std::is_copy_constructible_v<T>` 约束，避免移动专属类型触发 MSVC 即时模板实例化错误

- **mine.ui.window（平台窗口与视觉树桥接）**：实现 Window 类，将 `platform::IWindow`、`gfx::IDevice/IQueue`、`paint::IRenderer` 与 UIElement 视觉树整合（M1.1 任务 #21）：
  - `Window`（Pimpl 模式）：封装平台窗口与渲染管线的完整生命周期
    - 构造时自动创建 `ISwapchain`（按逻辑尺寸×DPI 缩放因子得到物理像素分辨率）并订阅平台窗口事件（`IWindowEventSink`）
    - 析构时取消事件订阅，确保 Impl 不被悬空调用
    - `set_content(UIElement*)` / `content()`：设置视觉树根元素，设置后立即执行布局（Measure + Arrange）并渲染
    - `show() / hide() / close()`：委托到 `platform::IWindow`
    - `set_title(StringView) / set_size(Size)`：委托到 `platform::IWindow`
    - `size() / dpi() / native_window()`：查询委托到 `platform::IWindow`
    - `is_closed()`：返回 Closed 事件后设置的 `is_closed_` 标志
    - `render()`：`is_closed` 时为空操作，否则执行完整渲染流程
    - DPI 感知：`dpi_scale_ = dpi() / 96.0f`，构造和 `DpiChanged` 事件时更新渲染器缩放因子并重调交换链尺寸
    - 事件响应：`Resized` → wait_idle + 交换链 resize + 布局 + 渲染；`DpiChanged` → 更新 dpi_scale + wait_idle + 交换链 resize + 布局 + 渲染；`Closed` → `is_closed_ = true`
    - 渲染流程（`do_render`）：`Canvas::fill_rect`（背景）→ `UIElement::render_to_canvas`（视觉树） → `canvas.end()` 得到 DisplayList → `renderer.begin_frame/render/end_frame` → `swapchain.present()`
    - 布局流程（`run_layout`）：`content_->measure(available_size)` → `content_->arrange(Rect{0,0,w,h})`
  - 共 20 个单元测试（doctest），覆盖：构造交换链创建/事件订阅、析构取消订阅、初始状态、DPI 初始化、set_content/content 读写/清除、set_content 触发布局与渲染、show/hide/close 委托、size/dpi 委托、Resized 事件（wait_idle + resize + 布局 + 渲染）、DpiChanged 事件（dpi_scale 更新 + 交换链 resize）、Closed 事件（is_closed 置位）、close() 调用后 is_closed、render() 调用流程与 present、is_closed 时 render 空操作、native_window 访问

- **mine.ui.layout（两遍布局协议）**：实现 WPF 风格 Measure/Arrange 两遍布局系统，新增 FrameworkElement、Panel、StackPanel、Grid（M1.1 任务 #20）：
  - `UIElement`（扩展）：新增公共 `measure(Size)` 入口方法和受保护 `set_desired_size(Size)` 辅助方法，为 FrameworkElement 提供协议扩展钩子
  - `FrameworkElement`：在 UIElement 基础上实现完整两遍布局协议：
    - 依赖属性：`Width / Height`（默认 NaN=Auto）、`MinWidth / MaxWidth / MinHeight / MaxHeight`（默认 0 / +∞）、`Margin`（`math::Thickness`，默认 0）、`HorizontalAlignment / VerticalAlignment`（默认 Stretch）
    - `on_measure`：减 Margin → 应用 Width/Height 明确值 → clamp Min/Max → 调用 `measure_override()` → 再次应用 Width/Height → 再次 clamp → 加 Margin → `set_desired_size()`
    - `arrange(Rect slot)`：减 Margin → 依据 HA/VA 计算内容矩形（非 Stretch 使用 desiredSize 尺寸并对齐）→ `set_bounds_rect(content_rect)`
    - `on_arrange(Rect)`：调用 `arrange_override(final_rect.size())`
    - 子类覆盖点：`measure_override(Size)`（默认 {0,0}）/ `arrange_override(Size)`（默认接受 final_size）
  - `Panel`：FrameworkElement 的多子元素容器基类：
    - `add_child(FrameworkElement*)`：防重复添加，同步加入视觉树（`Visual::add_child()`），失效布局
    - `remove_child(FrameworkElement*)`：手动移位保序删除（SmallVector 无 erase），同步从视觉树移除，失效布局
    - `child_at(uint32_t)` / `children_count()`：随机访问
  - `StackPanel`：沿单一方向线性堆叠子元素：
    - `Orientation` 依赖属性（默认 Vertical）
    - Vertical Measure：向各子元素传 `{panelWidth, INFINITY}`，总高度累加，总宽度取最大
    - Horizontal Measure：向各子元素传 `{INFINITY, panelHeight}`，总宽度累加，总高度取最大
    - Arrange：沿主轴累计偏移量逐一调用 `child->arrange(slot)`，交叉轴尺寸 = 面板分配尺寸（允许子元素内部 Stretch）
  - `Grid`：行列网格布局，三步尺寸计算算法：
    - `GridLength`：`Pixel / Auto / Star` 三种模式，含静态工厂 `pixel() / auto_() / star(weight)`
    - `RowDefinition / ColumnDefinition`：含 GridLength 尺寸和 Min/Max 约束
    - 附加属性：`RowProperty / ColumnProperty`（0-based 索引）、`RowSpanProperty / ColumnSpanProperty`（默认 1）；静态便捷方法 `get/set_row/column/row_span/column_span(FrameworkElement&)`
    - 尺寸计算：Pixel → 固定像素（clamp Min/Max）；Auto → 收集该行/列内单跨子元素的 desiredSize 取最大；Star → 按权重比例分配剩余空间
    - 无行/列定义时视为单行单列（1*）
    - RowSpan/ColumnSpan：slot 宽/高为跨越行/列的实际尺寸之和
  - 共 27 个单元测试（doctest），覆盖：默认属性值、Measure（叶子零尺寸/明确Width-Height/Margin增量/Min/Max约束）、Arrange（Stretch充满/Left-Center-Right-Bottom-Center对齐/Margin偏移）、Panel 子节点增删/防重复、StackPanel 垂直水平 Measure desiredSize 累加/Arrange 顺序偏移、Grid 像素行列单/多子元素位置、Auto 行列尺寸、Star 等权重/不等权重比例、ColumnSpan slot 合并宽度、无定义默认单行单列

- **mine.ui.input（键盘/鼠标输入路由）**：实现 Win32 输入接收与视觉树事件派发（M1.1 任务 #18）：
  - `mine.platform.abi / WindowEvent`：扩展 `WindowEventKind` 枚举新增 `KeyDown / KeyUp / Char / MouseMove / MouseDown / MouseUp / MouseWheel`；`WindowEvent` 结构体新增键盘字段（`key_vk_code / key_scan_code / char_utf32 / key_is_repeat`）、鼠标字段（`mouse_x / mouse_y / mouse_button / mouse_wheel_delta`）和修饰键字段（`mod_ctrl / mod_shift / mod_alt`）
  - `mine.platform.win32 / Win32Window`：新增对 `WM_KEYDOWN / WM_SYSKEYDOWN / WM_KEYUP / WM_SYSKEYUP / WM_CHAR / WM_MOUSEMOVE / WM_LBUTTONDOWN / WM_LBUTTONUP / WM_RBUTTONDOWN / WM_RBUTTONUP / WM_MBUTTONDOWN / WM_MBUTTONUP / WM_XBUTTONDOWN / WM_XBUTTONUP / WM_MOUSEWHEEL` 共 15 类消息的处理，填充平台 `WindowEvent` 并通过 `event_source_.fire(ev)` 派发；鼠标坐标使用 `phys_to_logical(DPI)` 转换为逻辑像素；`WM_MOUSEWHEEL` 坐标用 `ScreenToClient` 由屏幕坐标转为客户区坐标；左键按下/释放时调用 `SetCapture / ReleaseCapture` 确保按住拖拽消息连续接收
  - `Key`：平台无关虚拟键枚举（值与 Win32 VK_ 常量对齐），包含字母 A-Z、数字 0-9、功能键 F1-F12、导航键（方向/Home/End/PageUp/PageDown）、数字键盘、锁定键和修饰键；`key_from_win32_vk(uint32_t) → Key` 直接转换（大多数键无需查表）
  - `ModifierKeys`：Ctrl / Shift / Alt 位域枚举，重载 `| / & / ~` 和 `|= / &=` 运算符，`has_flag()` 检测辅助函数
  - `MouseButton`：左 / 右 / 中 / X1 / X2 枚举（None=255 表示无特定按键，用于 MouseMove/MouseWheel）
  - `KeyEventArgs`：键盘路由事件参数，继承 `RoutedEventArgs`，携带 `Key / scan_code / ModifierKeys / is_repeat` 及 `ctrl() / shift() / alt()` 快捷访问器
  - `MouseEventArgs`：鼠标路由事件参数，继承 `RoutedEventArgs`，携带 `MouseButton / math::Point position / ModifierKeys / wheel_delta` 及 `ctrl() / shift() / alt()` 快捷访问器
  - `InputEvents`：10 个标准路由事件声明与注册（Meyer's 单例）：`PreviewKeyDown / KeyDown / PreviewKeyUp / KeyUp`（键盘）+ `PreviewMouseDown / MouseDown / PreviewMouseUp / MouseUp / MouseMove / MouseWheel`（鼠标）；Preview 系列注册为 Tunnel 策略，其余为 Bubble 策略；所有事件以 `UIElement` 为所有者类型
  - `InputRouter`：实现 `IWindowEventSink`，订阅平台窗口事件并路由到视觉树：
    - `set_root(UIElement*)` / `set_keyboard_focus(UIElement*)` 配置路由目标
    - 鼠标事件：通过 `UIElement::hit_test(Point)` 命中测试确定目标，更新 `mouse_over_element()`，按 Preview-正式事件对派发（Preview 被处理则跳过正式事件）
    - 键盘事件：派发到 `keyboard_focus_` 元素，无焦点时退化到根节点
    - 修饰键提取：`make_modifiers(WindowEvent&)` 内部辅助从 `mod_ctrl/shift/alt` 字段组装 `ModifierKeys`
  - 共 26 个单元测试（doctest），覆盖：ModifierKeys 位运算与 `has_flag`、Key 枚举转换、KeyEventArgs / MouseEventArgs 构造与访问器、InputEvents 单例引用同一性与路由策略验证、InputRouter 无根节点不崩溃、命中测试命中/不命中派发、鼠标移动/滚轮事件、键盘焦点派发、无焦点退化路由、Preview 被处理后正式事件跳过、mouse_over_element 状态更新

- **mine.ui.visual（视觉树与渲染管线接入）**：实现视觉基类 Visual、UIElement、Control，完成视觉树管理、局部变换、矩形裁剪、依赖属性接入和 paint 渲染管线（M1.1 任务 #19）：
  - `Visibility`：枚举 Visible / Hidden / Collapsed，Hidden 保留布局空间但跳过渲染，Collapsed 同时跳过渲染与布局
  - `Visual`：视觉树根基类，继承 `DependencyObject` + `IRoutedEventTarget`：
    - 视觉树管理：`add_child` / `remove_child` / `remove_all_children`，析构时自动从父节点移除防止悬空
    - 局部变换：`render_transform()` / `set_render_transform(Transform2D)`，直接字段存储（Transform2D 36字节超出 Variant SBO）
    - 矩形裁剪：`set_clip_rect(Rect)` / `has_clip_rect()` / `clip_rect()` / `clear_clip_rect()`
    - 依赖属性：`OpacityProperty`（float [0,1]，affects_render）、`VisibilityProperty`（affects_measure/arrange/render）
    - 脏区传播：`invalidate_render()` 沿父链向上传播，直到已脏祖先节点或根节点
    - 渲染管线：`render_to_canvas(Canvas&)` → save → transform → clip → `on_render()` → 递归子树 → restore → 清除脏标志
    - 路由事件：实现 `add_handler / remove_handler / invoke_handlers / parent_target`，handled=true 中止本层后续处理器
    - 无 RTTI 类型探查：`virtual UIElement* as_element()` 虚方法，替代 `dynamic_cast`（项目禁用 `/GR-`）
  - `UIElement`：布局接口与命中测试基类：
    - 布局边界：`bounds_rect()` / `set_bounds_rect(Rect)` / `desired_size()`
    - 命中测试：`hit_test(Point)` — Collapsed 跳过 → 逆变换局部坐标 → 裁剪检查 → 子节点逆序递归 → `hit_test_local()` 自身判断
    - 布局虚方法桩：`on_measure(Size)` / `on_arrange(Rect)` / `hit_test_local(Point)`（默认检查 bounds_rect）
    - 布局失效：`invalidate_measure()` / `invalidate_arrange()` 维护内部脏标志
  - `Control`：控件基类（M1.1 仅作为继承链节点，后续 mine.ui.style 扩展）
  - 共 35 个单元测试（doctest），覆盖：视觉树增删/父子关系/析构清理、变换存取/渲染失效触发、裁剪区域管理、Opacity/Visibility 依赖属性含边界钳制、脏区传播链、渲染管线（Hidden/Collapsed/子树递归）、路由事件处理器顺序/handled 中止/remove_handler、命中测试（点内/点外/Collapsed/子节点优先）

- **mine.ui.event（路由事件系统与命令接口）**：实现 WPF 风格路由事件系统和 ICommand/RelayCommand（M1.1 任务 #17）：
  - `RoutingStrategy`：枚举 Bubble（冒泡）/ Tunnel（隧道/Preview）/ Direct（直接）
  - `RoutedEvent`：全局唯一路由事件描述符，地址稳定，由 `RoutedEventRegistry`（Meyer's 单例）统一管理；`register_event<TOwner>(name, strategy)` 类型安全注册 API；内部分配名称字符串副本保证生命周期
  - `RoutedEventArgs`：路由事件参数基类，携带事件描述符引用、source/original_source（void*）和 Handled 标志；栈上分配，禁止拷贝（防止多态切片）
  - `IRoutedEventTarget`：路由目标纯虚接口，提供 `parent_target()` 和 `invoke_handlers()`，将路由算法与 UIElement 实现完全解耦
  - `EventManager`：纯静态派发器，`raise(source, args)` 根据事件策略执行三种传播算法：Bubble（source→root，handled 停止）、Tunnel（收集完整路径后 root→source 反向派发，handled 停止）、Direct（仅 source）
  - `ICommand`：命令抽象接口，`can_execute() / execute()` + `subscribe_can_execute_changed() / unsubscribe_can_execute_changed(token)` 订阅模式
  - `RelayCommand`：ICommand 的 Pimpl 实现，接受 `Function<void(const Variant&)>` execute 和可选 `Function<bool(const Variant&)>` can_execute；`notify_can_execute_changed()` 手动触发订阅者通知；默认 can_execute 空时始终返回 true
  - 共 24 个单元测试（doctest），覆盖：RoutedEvent 注册/属性/多所有者、RoutedEventArgs 初始状态/source/handled、Bubble 三层树传播/handled 中止/无父节点、Tunnel 反向顺序/handled 中止/无父节点、Direct 不传播、RelayCommand execute/can_execute/subscribe/unsubscribe/notify/多订阅/重复取消/move/ICommand 接口

- **mine.ui.binding（单向数据绑定系统）**：实现 OneWay/OneTime 单向绑定（M1.1），并为 TwoWay 预留完整基础设施：
  - `BindingMode`：枚举 OneWay / TwoWay / OneWayToSource / OneTime
  - `IConverter`：绑定值转换器接口，`convert()`（正向，M1.1 使用）+ `convert_back()`（反向，M2 预留）
  - `INotifyPropertyChanged`：ViewModel 属性变更通知接口，基于原始函数指针 + void* user_data，无 RTTI/异常依赖
  - `PropertyDependency`：依赖源描述符，支持两种来源：`DependencyObject + DependencyProperty`（按属性描述符指针过滤）或 `INotifyPropertyChanged + StringView 属性名`（按名字符串过滤）
  - `BindingExpression`：运行时绑定核心，Pimpl 实现，`attach()/detach()/evaluate()` 生命周期管理；attach 时预 reserve 订阅记录确保地址稳定；OneTime 模式 attach 后立即取消订阅；`fallback` 值在 getter 返空时自动使用；TwoWay 基础：`setter` 字段已定义、`is_updating` 防循环标志已预留
  - `mine.core::Function<R(Args...)>`：新增 32 字节 SBO move-only 函数包装器，作为 getter/setter 的闭包容器，static const Ops 虚表（无多态开销），noexcept 移动/调用接口
  - 共 22 个单元测试（doctest），覆盖：Function<> 基础语义、DependencyObject 来源绑定（初始求值/变更触发/属性过滤/detach）、INotifyPropertyChanged 来源绑定、converter 正向转换、fallback 回退、OneTime 模式、TwoWay 预留字段验证、手动 evaluate、move 语义、析构自动取消订阅

- **mine.ui.property（依赖属性系统）**：完整实现 WPF 风格依赖属性模块（M1.1）：
  - `DependencyProperty`：全局唯一静态描述符，支持 `register_property` / `register_attached_property` 注册，包含名称、属主类型、值类型、默认值、属性元数据（影响布局/渲染失效标志、变更回调、继承标志）
  - `DependencyObject`：基于 Pimpl 的属性值存储基类，支持多优先级槽（Default / Inherited / StyleSetter / StyleTrigger / TemplateBind / Local / Animation），`set_value` / `get_value` / `clear_value` / `has_value` 按优先级合并生效
  - 属性变更通知：`on_property_changed` 虚方法 + `subscribe_property_changed` 订阅机制，内置防递归保护
  - 虚方法钩子 `invalidate_measure` / `invalidate_arrange` / `invalidate_render`，与属性元数据 `affects_*` 标志联动
  - 共 19 个单元测试（doctest），覆盖注册、get/set/clear、优先级覆盖、通知、防递归、附加属性等场景

### Fixed
- **paint（路径裁剪形状错误）**：修复 `ClipPushPath` 渲染路径中多边形顶点数 `nv` 在 HLSL 着色器内被错误限制为最多 16 个的 Bug：
  - `evaluate_clip_coverage` 中 `clamp(..., 3, 16)` 同步更新为 `clamp(..., 3, 64)`，与 C++ 侧 `k_max_clip_poly_verts=64` 保持一致
  - 修复前：椭圆裁剪仅使用前 16 个折线顶点，呈现为楔形；心形裁剪区域完全为空白
  - 修复后：任意复杂曲线路径裁剪均可正确显示

- **paint（心形路径裁剪空白）**：修复 `Canvas::save()/restore()` 不保存/恢复裁剪层深度导致相邻 `clip_path` 块裁剪状态叠加的 Bug：
  - `canvas.restore()` 现在自动弹出自上次 `save()` 以来推入的所有裁剪层（发出对应数量的 `ClipPop` 命令），无需手动调用 `clip_pop()`
  - 所有 `clip_*()` 方法（`clip_rect / clip_rounded_rect / clip_complex_rounded_rect / clip_polygon / clip_path`）现在维护 `clip_depth_` 计数器，`clip_pop()` 也对应递减
  - 修复前：先 `clip_path(椭圆)` 再 `clip_path(心形)` 时两个裁剪层同时生效，心形内的内容完全位于椭圆裁剪区外，导致空白
  - 修复后：每个 `save/clip_path.../restore` 块独立管理裁剪层，互不干扰

- **paint（路径描边曲线锯齿）**：`StrokePath` 渲染方案从折线扁平化逐段 kind=6 改为遍历原始路径命令并为每种曲线段发射对应 SDF 图元，真正实现曲线 SDF 抗锯齿描边：
  - `LineTo` → SDF kind=6（线段 SDF）
  - `QuadTo` → SDF kind=8（二次贝塞尔 SDF，IQ 解析解，无折线化误差）
  - `CubicTo` → SDF kind=9（三次贝塞尔 SDF，数值迭代）
  - 内部接缝使用 Round cap，路径首尾端点沿用 `Pen` 设定的 cap 样式（kind=8/9 不支持 Square，回退为 Flat）
  - 修复前：波浪线等 QuadBezier 路径在折线化后存在折线感和接缝瑕疵
  - 修复后：每段曲线均以原生 SDF 渲染，边缘天然抗锯齿，无折线化误差

- **paint（路径描边抗锯齿）**：`StrokePath` 渲染方案从 CPU 三角带展开（`solid_pipeline_`，无抗锯齿）改为逐线段 SDF 渲染（kind=6，天然抗锯齿）：
  - 每条扁平化折线段发射独立的 SDF kind=6 quad，使用 `sdf_pipeline_` 渲染
  - 内部接缝处使用 Round cap 填充，路径首尾端点沿用 `Pen` 设定的 cap 样式
  - 同步移除不再使用的 `push_polyline_stroke_vertices` 辅助函数

### Added
- **paint（路径裁剪）**：实现 `clip_path(const Path&)` 路径形状裁剪：
  - 新增 `DrawCmdKind::ClipPushPath`，携带原始 Path 索引，曲线段扁平化在渲染器内部完成
  - `Canvas::clip_path()` API：直接存储路径引用，保持 Canvas 层轻量
  - `RhiRenderer` 新增 `ClipPushPath` 执行分支：调用 `flatten_path_to_subpaths`，优先取第一个闭合子路径，降采样至 ≤64 顶点后填写 `ClipSdfLayer`（kind=10 多边形 SDF）
  - 同步将 `k_max_clip_poly_verts` 从 16 扩展至 64，更新 HLSL `ClipSdfLayer.poly_verts` 数组大小及 C++ `static_assert`（`ClipSdfLayer` 1072 字节，`ClipSdfCB` 4304 字节）

- **paint（路径渲染）**：实现 `FillPath / StrokePath` 执行链路：
  - `RhiRenderer` 新增 Path 扁平化（`LineTo/QuadTo/CubicTo/Close`）
  - `FillPath`：闭合子路径转换为多边形 SDF 填充（支持抗锯齿边缘）
  - `StrokePath`：子路径展开为三角带并走 `solid_pipeline_` 描边
  - 路径绘制纳入现有变换系统（`save/restore/transform`）

- **platform（IME 输入法服务）**：实现 Win32 平台层完整 IME 支持（Windows IMM32 API）：
  - `WindowEventKind` 新增四种 IME 事件类型：`ImeCompositionStarted`、`ImeCompositionChanged`、`ImeCompositionCommitted`、`ImeCompositionEnded`
  - `WindowEvent` 新增 `ime_text_utf8[256]` 和 `ime_text_length` 字段，承载 UTF-8 编码的组合文字或提交文字
  - `Win32IMEService` 从空存根升级为完整实现：通过 `ImmGetContext` / `ImmSetCandidateWindow` / `ImmSetCompositionWindow` 控制候选框位置，通过 `ImmAssociateContext` / `ImmAssociateContextEx` 启用/禁用 IME
  - `Win32IMEService` 新增内部接口 `on_focus(HWND)` / `on_blur()`，在窗口焦点变化时维护活跃 HWND
  - `Win32Window` 构造函数新增 `Win32IMEService*` 参数，在 `WM_SETFOCUS` / `WM_KILLFOCUS` 时同步通知 IME 服务
  - `Win32Window::handle_message` 新增 `WM_IME_SETCONTEXT`、`WM_IME_STARTCOMPOSITION`、`WM_IME_COMPOSITION`（含 `GCS_COMPSTR` + `GCS_RESULTSTR` 分支）、`WM_IME_ENDCOMPOSITION` 消息处理
  - `Win32ApplicationHostImpl::create_window` 向 `Win32Window` 传入 `&ime_service_`，完成焦点→IME 的通知链路

- **paint（变换系统）**：实现 Canvas 2D 变换系统（平移、缩放、旋转）：
  - `DrawCmdKind::TransformSet`：新增非压栈变换命令（`transform()`/`translate()`/`rotate()`/`scale()` 使用）
  - `DrawCmdKind::TransformPush`：语义修正为仅保存当前变换快照（`save()` 使用，不级联变换）
  - `Canvas::save()`/`restore()`：正确实现 HTML Canvas 风格的状态保存/恢复语义
  - `Canvas::translate(Vec2)`/`scale(float)`/`scale(Vec2)`/`rotate(float)` 现在实际生效
  - `RhiRenderer::render()` 新增 CPU 端变换状态追踪（`current_transform` + `transform_save_stack`）
  - 方案：CPU 端后置变换 — 对 SDF 顶点屏幕坐标应用变换，`local` 坐标保持画布空间不变
  - GPU 属性插值机制确保旋转/缩放后 SDF 仍在画布本地坐标系中正确计算，**无需修改任何 HLSL**
  - 支持无限嵌套变换栈（`save()`/`restore()` 正确恢复整组变换）
  - 所有 SDF 形状（FillRect/FillRoundedRect/FillEllipse/StrokeRect/StrokeLine/StrokeArc/StrokeQuadBezier/StrokeCubicBezier/FillPolygon 等）、文字（DrawText）均支持当前累积变换
  - 当变换为单位矩阵时零开销（`current_transform_is_identity` 快速路径跳过顶点遍历）
  - 支持 `Transform2D::rotation_about(angle, pivot)` 绕任意点旋转

- **samples（样例重构）**：`00-hello-rect` 网格布局从 2列×18行（竖长格）改为 6列×6行（接近正方形），36 个演示格子内容完全保留，按功能分组排布：行0 基本形状、行1 椭圆+线段、行2 圆弧+贝塞尔、行3 多边形+渐变、行4 亚克力+裁剪、行5 变换演示

- **samples（样例更新）**：
  - `00-hello-rect`：增加 2 行新演示（行16-18）：
    - 行16左：`save/translate/rotate/restore` — 旋转 30° 的圆角矩形（含未旋转轮廓参考）
    - 行16右：`save/translate/scale/restore` — 放大 1.5 倍的椭圆（含正常大小轮廓参考）
    - 行17左：嵌套变换演示 — 外层旋转15° 绿色矩形 + 内层叠加旋转30°+缩放0.6的橙色矩形
    - 行17右：`rotation_about` 演示 — 五个彩色圆角矩形绕格子中心以72°间隔均匀分布
    - 行18左：`translate` 纯平移演示 — 原始位置半透明轮廓 + 平移后填充矩形对比
    - 行18右：`translate` 步进演示 — 五个矩形以固定步进递进平移，颜色蓝渐变橙

- **paint（裁剪系统升级）**：将 Stencil 二值裁剪替换为 ClipSdfCB SDF 软裁剪，消除裁剪边缘锯齿：
  - 移除 stencil 渲染目标绑定及 `clear_depth_stencil` 调用，改为直接绑定颜色渲染目标（无深度/模板附件）
  - 新增 `ClipSdfLayer`（304 字节）/ `ClipSdfCB`（1232 字节）C++ 结构体，最多 4 层嵌套裁剪、每层最多 16 多边形顶点
  - 主 SDF 像素着色器（b3 槽）内新增 `evaluate_clip_coverage()` + `sdClipPolygon()`，所有 `return` 分支均乘以覆盖率，实现 `smoothstep` 亚像素级抗锯齿裁剪
  - `ClipPush*/ClipPop` 命令处理器改为直接填写 `ClipSdfCB.layers[]` 并重建 GPU 端 b3 常量缓冲，无需再绘制裁剪几何体到模板缓冲
  - 移除 `make_clip_vb` / `make_clip_polygon_cb` 辅助 lambda（stencil 路径专用，已废弃）
  - 管线选择简化：始终使用 `sdf_pipeline_` / `text_pipeline_` 主管线，不再按裁剪深度切换到 `*_clip_test_pipeline_`

- **paint（裁剪系统）**：新增基于 D3D11 模板缓冲（Stencil Buffer）的 SDF 嵌套裁剪系统：
  - `Canvas::clip_rect(Rect)`：压入矩形裁剪区域
  - `Canvas::clip_rounded_rect(RoundedRect)`：压入均匀圆角矩形裁剪区域
  - `Canvas::clip_complex_rounded_rect(ComplexRoundedRect)`：压入四角独立圆角矩形裁剪区域
  - `Canvas::clip_polygon(Span<const Vec2>)`：压入任意多边形裁剪区域（凸/凹均支持，最多 64 顶点）
  - `Canvas::clip_pop()`：弹出最近一次压入的裁剪区域
  - 支持嵌套裁剪（基于 IncrSat/DecrSat 模板操作，最大嵌套深度 255）
  - 新增 `k_sdf_clip_pixel_shader_hlsl`（SDF clip(-d) 裁剪像素着色器）
  - 新增 5 个裁剪管线变体（`sdf_clip_write`、`sdf_clip_erase`、`sdf_clip_test`、`text_clip_test`、`solid_clip_test`）
  - `render()` 绘制循环自动按 `clip_depth` 切换到对应 ClipTest 管线变体，对所有形状/文字命令生效

- **gfx.rhi / gfx.d3d11（接口扩展）**：
  - `StencilMode` 枚举（None / ClipWrite / ClipTest / ClipErase）及 `PipelineDesc.stencil_mode` 字段
  - `ICommandList::set_stencil_ref(uint8_t ref)` 接口（D3D11 后端已实现）
  - `D3D11Pipeline` 支持四种深度/模板状态的完整创建路径

- **samples（样例更新）**：
  - `00-hello-rect`：增加 2 行新演示（行14-15）：矩形裁剪/圆角矩形裁剪/四角独立圆角裁剪/三角形多边形裁剪

- **paint（渐变与亚克力画刷）**：新增多种画刷类型（M0 阶段）：
  - `BrushKind::LinearGradient`：线性渐变画刷，起点/终点使用归一化坐标 [0,1]，最多 4 个色标
  - `BrushKind::RadialGradient`：径向渐变画刷，支持 `inner_radius` 实现环形渐变效果
  - `BrushKind::AcrylicBrush`：亚克力画刷（捕获上一帧内容 + 高斯模糊 + 饱和度调整 + 色调叠加）
  - `GradientStop { float offset; math::Color color; }`：色标结构体
  - `LinearGradientData` / `RadialGradientData` / `AcrylicData`：各类型画刷数据结构（POD，无动态分配）
  - `Brush::linear_gradient(start, end, Span<GradientStop>)` 工厂方法
  - `Brush::radial_gradient(center, outer_radius, Span<GradientStop>)` 工厂方法
  - `Brush::radial_gradient_ring(center, inner_radius, outer_radius, Span<GradientStop>)` 工厂方法
  - `Brush::acrylic(tint_color, tint_opacity, blur_amount, saturation)` 工厂方法
  - `Brush` 类改为 union 内联存储（不超过 4 个色标，无堆分配），总大小约 104 字节

- **gfx.rhi（接口扩展）**：
  - `IDevice::copy_texture(dst, src)`：GPU-to-GPU 全量纹理拷贝接口（亚克力背景捕获用）

- **gfx.d3d11（后端实现）**：
  - `D3D11Device::copy_texture()`：使用 `ID3D11DeviceContext::CopyResource` 实现

- **paint（SDF 渲染管线扩展）**：
  - `BrushDataCB`（HLSL/C++ 双端，128 字节，b2 槽）：每 DrawCall 传递画刷类型及渐变参数
  - SDF 像素着色器新增 `sample_gradient()` 辅助函数（支持 2-4 个色标的分段线性插值）
  - SDF 像素着色器支持 `brush_kind` 分支：纯色/线性渐变/径向渐变/亚克力，适用于所有 fill 形状
  - `ViewportCB` 增加 `phys_width/phys_height` 字段（取代原 padding），供亚克力着色器将 SV_Position 转为 UV
  - 新增高斯模糊管线 `blur_pipeline_`（BlurVertex 16 字节，9-tap 分离式卷积，H/V 两趟复用同一管线）
  - 新增 `ensure_scratch_textures()` 亚克力 scratch 纹理懒创建（尺寸变化时自动重建）
  - 新增 `make_brush_cb()` 静态方法（归一化坐标 → local 像素坐标转换，生成 GPU 画刷参数缓冲）
  - `render()` 新增亚克力前处理步骤（copy_texture + H 模糊 + V 模糊，在主绘制循环前执行）
  - 所有 fill 形状命令（FillRect/FillRoundedRect/FillComplexRoundedRect/FillEllipse）现支持渐变和亚克力画刷

- **samples（样例更新）**：
  - `00-hello-rect`：增加 3 行新演示（行11-13）：线性渐变矩形/径向渐变椭圆/渐变圆角矩形/多色标径向渐变/亚克力圆角矩形/亚克力椭圆

- **text（mine.text 模块）**：新增基于 FreeType 的字体光栅化模块（M0 阶段）：
  - `FontFace`：字体面封装类（FreeType `FT_Face`），支持 `load_from_file` / `load_from_memory` 工厂方法，`set_pixel_size` / `rasterize` / `ascender` / `descender` / `line_height` API
  - `GlyphMetrics` / `GlyphBitmap`：字形度量和位图结构体（bearing / advance / pitch）
  - `FT_RENDER_MODE_NORMAL`：8-bit 灰度 Alpha 光栅化（非 SDF），与 GPU 字形图集匹配
  - FreeType 库通过 xmake 包管理器自动获取（`add_requires("freetype", {system=false})`）
  - `mine_module.lua` 扩展支持 `packages` 参数，传给 `add_packages()`

- **gfx.rhi（接口扩展）**：
  - `PixelFormat::R8_UNorm`：新增 8-bit 单通道线性格式，D3D11 映射为 `DXGI_FORMAT_R8_UNORM`
  - `ICommandList::set_shader_resource(slot, texture)`：像素着色器 SRV 绑定（同时绑定线性采样器）
  - `IDevice::update_texture_region(texture, x, y, w, h, data, row_pitch)`：部分纹理区域 CPU→GPU 上传

- **gfx.d3d11（后端实现）**：
  - `D3D11CommandList`：实现 `set_shader_resource()`，构造函数中预创建双线性线性采样器（CLAMP 模式）
  - `D3D11Device`：实现 `update_texture_region()`，使用 `D3D11_BOX` + `UpdateSubresource`

- **paint（文字渲染管线）**：
  - `DrawCmdKind::DrawText`：新绘制命令，`path_index` 复用为 `text_run` 数组下标
  - `TextRun`：内联字符串（512 字节 UTF-8）+ `font_face`（`void*`）+ `size_px` + 基线原点
  - `DisplayList`：新增 `text_runs_` 成员，三参数构造，`text_runs()` / `text_run_count()` 访问器
  - `Canvas::draw_text(text, origin, font_face, size_px, brush)`：录制文字绘制命令
  - `RhiRenderer` 新增文字渲染管线 `text_pipeline_`（`TextVertex`，32 字节：pos + uv + color）和 `GlyphAtlas`（字形 GPU 图集管理器）：
    - `GlyphAtlas`：1024×1024 R8_UNorm CPU 位图 + Shelf Packer 分配策略 + 线性缓存（最多 2048 条）+ dirty flag 全量上传
    - `k_text_vertex_shader_hlsl` / `k_text_pixel_shader_hlsl`：文字专用 HLSL（R8 图集采样，预乘 Alpha 输出）
    - `DrawText` 命令处理：UTF-8 解码 → 字形光栅化写入图集 → GPU 上传 → 生成 TextVertex 四边形 → DrawCall

- **samples/00-hello-rect**：在 10 行演示网格顶部边距绘制 `draw_text()` 标题文字，展示 FreeType M0 文字渲染效果（加载 Segoe UI / Arial 系统字体）


  - `k_sdf_vertex_shader_hlsl`：SDF 顶点着色器，将屏幕坐标变换到 NDC 并透传形状参数
  - `k_sdf_pixel_shader_hlsl`：SDF 像素着色器，包含 `box_sdf`（矩形）/ `rounded_box_sdf`（均匀圆角矩形）/ `complex_rounded_box_sdf`（四角独立圆角矩形，基于 IQ sdRoundedBox）/ `ellipse_sdf`（IQ 近似椭圆 SDF）四种 SDF 函数，使用 `fwidth()` 实现亚像素级抗锯齿；填充和描边（中心对齐）统一在一个着色器中处理
  - `SdfVertex`（64 字节）：新顶点格式，包含屏幕坐标、颜色、本地坐标、形状参数（kind / half_w / half_h / p0-p3 圆角半径 / stroke_w）
  - `create_sdf_pipeline()`：新 SDF 管线（5 个顶点元素），与保留的 `solid_pipeline_`（折线描边）并行存在
  - `push_sdf_quad_vertices()`：为形状包围盒（含 padding）生成 6 个 SdfVertex（两个三角形），本地坐标以形状中心为原点
  - `render()` 完全重写：改为逐命令 DrawCall 架构，每个 `DrawCmd` 独立创建顶点缓冲并提交；视口常量缓冲全帧共享
  - 支持命令：`FillRect` / `StrokeRect` / `FillRoundedRect` / `StrokeRoundedRect` / `FillComplexRoundedRect` / `StrokeComplexRoundedRect` / `FillEllipse` / `StrokeEllipse`（均 SDF）；`StrokeLine` 保留折线展开方案（`solid_pipeline_`）
  - 移除：旧 PathBuilder 依赖（`flatten_cubic_bezier` / `flatten_path_to_polygon` / `push_convex_polygon_vertices` / `push_rect_vertices`）
  - 支持命令：`FillRect` / `StrokeRect` / `FillRoundedRect` / `StrokeRoundedRect` / `FillComplexRoundedRect` / `StrokeComplexRoundedRect` / `FillEllipse` / `StrokeEllipse`（均 SDF）
- **paint（StrokeBorderedRect）**：新增四边各自独立宽度的矩形内侧描边命令（CSS border 语义，SDF kind=4）：
  - `BorderWidths` 结构体：top/right/bottom/left 四个 float（内侧描边宽度，逻辑像素），含 `all(w)` 和 `axes(v, h)` 工厂方法
  - `DrawCmdKind::StrokeBorderedRect`：新绘制命令，使用 `DrawCmd::border_widths` 字段
  - `Canvas::stroke_bordered_rect(rect, brush, widths)`：录制对应命令
  - SDF 着色器 kind=4：四边各自独立计算到边缘的距离遮罩（smoothstep AA），取联集（max），乘以外矩形轮廓 alpha，零宽边遮罩恒为 0 避免伪线
- **paint（StrokeBorderedRoundedRect）**：新增四边独立宽度 + 四角独立圆角的矩形内侧描边命令（SDF kind=5）：
  - `DrawCmdKind::StrokeBorderedRoundedRect`：新绘制命令
  - `DrawCmd::border_radii`（`math::CornerRadii`）：四角独立圆角半径字段
  - `Canvas::stroke_bordered_rounded_rect(rect, brush, widths, radii)`：录制对应命令
  - `SdfVertex` 扩展至 80 字节：新增 TEXCOORD3（e0-e3，存放四角圆角半径）
  - SDF 管线 `create_sdf_pipeline()` 扩展至 6 个顶点元素（element_count=6，stride=80）
  - SDF 着色器 kind=5：外轮廓用 `complex_rounded_box_sdf`，各边独立遮罩（同 kind=4），联集后与外轮廓 alpha 相乘实现圆角自然裁剪
  - `push_sdf_quad_vertices` 添加 e0-e3 参数（默认 0），已有调用无需修改
- **samples/00-hello-rect（2×5 网格演示）**：第5行更新为 StrokeBorderedRoundedRect 效果演示：
  - 行5 左（红色）：四边不等宽（上4/右2/下16/左8）+ 四角独立圆角（左上30/右上10/右下20/左下直角）
  - 行5 右（青色）：均匀圆角（16px）+ 仅上下各12px 边框（左右宽度为 0）


### Fixed
- **gfx.d3d11**：修复 D3D11 交换链格式错误导致程序启动即崩溃（退出码 1）的问题：
  - `swapchain_typeless_format`：DXGI FLIP_DISCARD 交换链不接受 TYPELESS 格式，改为返回 BGRA8_UNORM/RGBA8_UNORM
  - `swapchain_rtv_format`：为保证 D3D11 标准兼容性（完全类型资源不可使用不同格式视图），RTV 格式与交换链格式统一为 UNORM，不再强制 sRGB
- **samples/00-blank-window**：错误报告改用 `MessageBoxA`（GUI 子系统无控制台，`fprintf(stderr,...)` 无效）；补充命令队列/命令列表空指针检查；修复 WIN32_LEAN_AND_MEAN/NOMINMAX 宏重定义警告

### Added
- 初始化顶层 xmake 构建入口、构建规则与基础工程配置
- 初始化 M0 基础模块、样例、工具、测试与脚本目录骨架
- **core**：实现 `mine.core` 基础模块，包含以下全套基础类型（无 RTTI、无 STL 容器头、无 C++ 异常）：
  - `Assert.h`：`MINE_ASSERT`/`MINE_CHECK`/`MINE_UNREACHABLE`/`MINE_ASSUME` 断言宏
  - `Status.h`：轻量操作状态 `Status` 与 `StatusCode` 枚举，含 `MINE_TRY` 宏
  - `Result.h`：无异常 `Result<T,E>` 判别联合，支持 `OkTag`/`ErrTag` 工厂
  - `StringView.h`：非拥有 UTF-8 字符串视图，含 `substr`/`find`/`starts_with`/`ends_with`
  - `Span.h`：非拥有连续内存视图 `Span<T>`，支持 C 数组/指针/容器隐式推导
  - `TypeId.h`：无 RTTI 编译期类型标识符，O(1) 比较，cv/引用透明
  - `Allocator.h`：`IAllocator` 接口与 `default_allocator()`/`set_default_allocator()` 全局入口
  - `Memory.h`：`OwnedPtr<T>`（跨 DLL 安全）、`make_owned`、`MINE_NEW`/`MINE_DELETE` 宏
  - `Pimpl.h`：PIMPL 惯用法辅助 `Pimpl<Impl>`/`make_pimpl`
  - `Variant.h`：无 RTTI 类型擦除值容器，16 字节 SBO，支持 `has<T>`/`get<T>`/`emplace<T>`/`any_cast`
  - `Core.h`：伞形头文件，一次 include 引入全部 core 子头
  - `DefaultAllocator.cpp`：`MallocAllocator`（Windows/Linux 对齐分配）、`assertion_failed` 实现
  - `CoreTest.cpp`：61 个测试用例，142 个断言，全部通过
- **containers**：实现 `mine.containers` 高性能容器模块（无 RTTI、无 STL 容器头、使用 IAllocator）：
  - `Hash.h`：`Hash<T>`/`Equal<T>` 模板，含整数/指针/float/double/StringView 特化，FNV-1a 哈希
  - `Vector.h`：`Vector<T>` 动态数组，2 倍增长策略，平凡类型 memcpy/memmove 优化
  - `SmallVector.h`：`SmallVector<T,N>` 内联缓冲区优化数组，超出 N 自动切换堆分配
  - `HashMap.h`：`HashMap<K,V>` Robin Hood 开放寻址哈希表，负载因子 0.75
  - `InlineString.h`：`InlineString` 带 SSO（23 字节内联）的拥有式 UTF-8 字符串
  - `IntrusiveList.h`：`IntrusiveList<T>`/`IntrusiveListNode<T>` 侵入式双向链表
  - `Containers.h`：伞形头文件，含 `Hash<InlineString>` 特化
  - `ContainersTest.cpp`：86 个测试用例，418 断言，全部通过（doctest）
- **math**：实现 `mine.math` 数学基础模块，覆盖二维/三维向量、矩阵、几何、颜色与二维变换：
  - `Common.h`：浮点容差、`clamp`/`clamp01`/`lerp`、角度弧度转换
  - `Vec2.h` / `Vec3.h` / `Vec4.h`：浮点向量，支持算术、点积、叉积、归一化
  - `Mat3.h` / `Mat4.h`：行主序矩阵，支持平移/缩放/旋转、矩阵乘法与点/向量变换
  - `Point.h` / `Size.h` / `Rect.h` / `RoundedRect.h`：二维几何基础类型与包含/相交/并集等操作
  - `Color.h`：线性空间 RGBA 颜色，支持 8 位打包转换、预乘与 Alpha 混合
  - `Transform2D.h`：二维仿射变换封装，支持组合、逆变换与矩形包围盒映射
  - `Math.h`：伞形头文件
  - `MathTest.cpp`：25 个测试用例，68 个断言，全部通过（doctest）
- **math（扩展）**：新增 `CornerRadii`、`ComplexRoundedRect`、`Thickness` 三个补充几何类型：
  - `CornerRadii.h`：四角独立圆角半径，支持统一构造、加减法与均匀缩放
  - `ComplexRoundedRect.h`：带 `CornerRadii` 的圆角矩形，继承 `RoundedRect` 语义
  - `Thickness.h`：四边内边距/外边距，`deflate`/`inflate` 操作对应 CSS box-model
  - `MathTest.cpp` 扩展：新增 33 个测试用例（114 断言），全部通过
- **platform.abi**：新增 `mine.platform.abi` 平台抽象接口层（纯头文件，无实现，依赖 mine.core + mine.math）：
  - `Api.h`：`MINE_PLATFORM_ABI_API` 宏（接口层无 DLL 导出，留作占位）
  - `ModuleTag.h`：模块名常量 `kModuleName = "mine.platform.abi"`
  - `NativeHandle.h`：平台原生句柄联合体，含 `ptr`/`value` 两种访问方式
  - `WindowKind.h`：`WindowKind` 枚举（Normal/Tool/Dialog/Splash/Popup）
  - `WindowEvent.h`：扁平 `WindowEvent` 结构体 + `WindowEventKind` 枚举（9 种事件）
  - `IWindowEventSink.h`：`on_window_event(WindowEvent&)` 纯虚接口
  - `IWindowEventSource.h`：`subscribe` / `unsubscribe` 观察者注册接口
  - `ScreenInfo.h`：`ScreenInfo` 显示器信息结构体（bounds、work_area、dpi、scale、is_primary）
  - `IScreenManager.h`：多显示器查询接口（screen_count / screen_at / primary_screen / screen_for_point）
  - `IClipboard.h`：系统剪贴板接口（has_text / get_text / set_text / clear）
  - `IMEService.h`：输入法服务接口（M0 阶段占位）
  - `WindowDesc.h`：窗口创建参数结构体（title、size、position、resizable、frameless 等）
  - `IWindow.h`：窗口操作接口（show / hide / close / set_title / set_size / set_position / native_handle / events）
  - `IApplicationHost.h`：应用宿主接口（run / quit / create_window / clipboard / screens / ime）
  - `PlatformAbi.h`：伞形头文件，一次 include 引入全部平台接口
- **platform.win32**：新增 `mine.platform.win32` Win32 窗口后端实现（仅 Windows，依赖 mine.platform.abi）：
  - 开启 per-monitor DPI v2 感知（`DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2`）
  - `Win32WindowClass`：WNDCLASSEX 单例注册/注销管理
  - `Win32WindowEventSource`：IWindowEventSource 的 vector 实现，支持快照迭代防重入
  - `Win32Window`：IWindow 实现，HWND 管理、GWLP_USERDATA 关联 this，处理 WM_SIZE/WM_MOVE/WM_CLOSE/WM_DESTROY/WM_SETFOCUS/WM_KILLFOCUS/WM_ACTIVATE/WM_DPICHANGED 等核心消息
  - `Win32ScreenManager`：IScreenManager 实现，基于 `EnumDisplayMonitors` + `GetDpiForMonitor`
  - `Win32Clipboard`：IClipboard 实现，基于 `OpenClipboard`/`GetClipboardData`/`SetClipboardData`，UTF-16 与 UTF-8 自动互转
  - `Win32IMEService`：IMEService M0 存根实现（所有方法为空操作）
  - `Win32ApplicationHostImpl`：IApplicationHost 实现，GetMessage 消息循环，最后一个窗口关闭时自动 PostQuitMessage(0)
  - 工厂函数：`mine::platform::win32::create_application_host()` 返回 `OwnedPtr<IApplicationHost>`
- **samples/00-blank-window**：新增最小化 Win32 GUI 示例，演示创建窗口 → 显示 → 消息循环 → 退出全流程，编译并运行通过
- **gfx.rhi**：新增 `mine.gfx.rhi` 图形渲染接口抽象层（纯头文件，无实现，依赖 mine.core + mine.math）：
  - `GfxTypes.h`：核心枚举与值类型（Backend / QueueType / PixelFormat / Vsync / TextureBindFlags / BufferBindFlags / Color4f / Viewport / ScissorRect / TextureDesc / BufferDesc / SwapchainDesc）
  - `ITexture.h` / `IBuffer.h`：GPU 资源接口（desc 查询 + 宽/高/格式/大小内联访问器）
  - `ICommandList.h`：命令录制接口（begin/end / set_render_target / clear / set_viewport / set_scissor / draw / draw_indexed 等）
  - `IQueue.h`：命令提交接口（submit / wait_idle / type）
  - `ISwapchain.h`：交换链接口（resize / present / current_render_target / width/height/format/image_count/vsync）
  - `IPipeline.h`：管线状态对象接口（M0 占位，type 查询）
  - `IFence.h`：栅栏/同步接口（signal / wait / completed_value）
  - `IDevice.h`：设备接口（create_queue / create_swapchain / create_buffer / create_texture / create_command_list / create_fence / backend / adapter_name）
  - `GfxHost.h`：工厂函数声明 `mine::gfx::create_device(Backend)`（实现由链接的后端库提供）
  - `Gfx.h`：伞形头文件，一次 include 引入全部 RHI 接口
- **gfx.d3d11**：新增 `mine.gfx.d3d11` Direct3D 11 图形后端实现（仅 Windows，依赖 mine.gfx.rhi）：
  - `D3D11Helpers.h`：内部辅助头（HRESULT 检查宏、RHI↔DXGI 格式转换、交换链 TYPELESS/RTV 格式工具）
  - `D3D11Texture`：ITexture 实现，支持交换链后缓冲专用构造（传入已有 ID3D11Texture2D+RTV）与通用构造（按 bind_flags 自动创建 SRV/RTV/DSV）
  - `D3D11Buffer`：IBuffer 实现，按 bind_flags 创建 D3D11 缓冲（VB/IB/CB/SRV/UAV），常量缓冲自动 16 字节对齐
  - `D3D11CommandList`：ICommandList 实现，基于 D3D11 延迟上下文（每实例独占），begin→ClearState，end→FinishCommandList，M0 绘制方法已接线 Draw*/DrawIndexed*
  - `D3D11Queue`：IQueue 实现，封装立即上下文，submit→ExecuteCommandList，wait_idle→Flush
  - `D3D11Swapchain`：ISwapchain 实现，DXGI FLIP_DISCARD 模型，TYPELESS 基础格式+sRGB RTV，支持 DXGI_FEATURE_PRESENT_ALLOW_TEARING，resize 正确释放后缓冲再重建
  - `D3D11Fence`：IFence 实现，基于 D3D11_QUERY_EVENT 轮询等待，支持超时
  - `D3D11DeviceImpl`：IDevice 实现，D3D11CreateDevice（Feature Level 11.0/11.1，硬件不可用时回退 WARP），查询 IDXGIFactory2 用于交换链创建，适配器名称 UTF-8 输出
  - `D3D11Backend.cpp`：实现 `mine::gfx::create_device()` 工厂函数（链接时替换模式）
- **samples/00-blank-window（更新）**：集成 D3D11 RHI，演示"蓝色清屏"验收场景：
  - 创建 D3D11 设备 + 交换链 + Graphics 队列 + 命令列表
  - 首帧清屏为纯蓝色（R=0, G=0.4, B=1, A=1）并呈现
  - 订阅 Resized 事件自动 resize 交换链并重新渲染，窗口大小变化后蓝色持续保持
- **paint**：新增 `mine.paint` 2D 绘制模块骨架（M0.3 任务 #13）：
  - `PaintFwd.h`：前向声明伞形头（Brush / Pen / Path / PathBuilder / DrawCmd / DisplayList / Canvas / IRenderer）
  - `Brush.h`：画刷类型，支持 `SolidColor`（M0 实现）、`LinearGradient`/`RadialGradient`/`ImageBrush`（占位），静态工厂 `solid()` / `solid_rgb()` / `solid_rgba()`
  - `Pen.h`：描边样式结构体（width / LineJoin / LineCap / miter_limit）
  - `Path.h`：不可变路径，持有 `PathCmd`（MoveTo / LineTo / CubicTo / QuadTo / Close）序列
  - `PathBuilder.h` + `PathBuilder.cpp`：流式路径构造器，支持 `add_rect()` / `add_rounded_rect()` / `add_ellipse()`（贝塞尔圆角近似，κ ≈ 0.5523）
  - `DrawCmd.h`：扁平绘制命令结构体（13 种命令：FillRect~ClipPop~TransformPush 等）
  - `DisplayList.h`：不可变绘制命令序列，持有 DrawCmd + Path 数组，可移动
  - `Canvas.h` + `Canvas.cpp`：录制模式绘制上下文，fill_* / stroke_* / clip_rect / translate / scale / rotate / save / restore / end()
  - `IRenderer.h`：2D 渲染后端抽象接口（begin_frame / end_frame / render），附工厂函数 `create_renderer()`
  - `Paint.h`：模块伞形头文件，一次 include 引入全部子头
- **core（修复）**：`Result.h` 补充 `#include <memory>`，修复 MSVC 2026 中 `std::construct_at`/`std::destroy_at` 不再通过 `<new>` 间接引入的编译错误
- **gfx.rhi（扩展）**：扩展 RHI 抽象层以支持着色器图形管线（M0.3 任务 #14）：
  - `GfxTypes.h`：新增 `ShaderLanguage`/`ShaderDesc`/`VertexSemantic`/`VertexElementFormat`/`VertexElement`/`PipelineDesc` 等着色器与管线描述类型
  - `IDevice.h`：新增 `create_pipeline(const PipelineDesc&)` 纯虚接口；`create_buffer()` 添加 `initial_data` 参数支持含初始数据的不可变缓冲
  - `ICommandList.h`：新增 `set_constant_buffer(uint32_t slot, IBuffer*)` 接口，同时绑定 VS/PS 的相同槽位
- **gfx.d3d11（扩展）**：实现 RHI 着色器管线扩展（M0.3 任务 #14）：
  - `D3D11Pipeline`：新建类，使用 D3DCompile 运行时编译 HLSL（Debug 含调试信息，Release 启用 O2），创建 InputLayout / BlendState（预乘 Alpha）/ RasterizerState（CULL_NONE, FILL_SOLID）/ DepthStencilState（2D 关闭深度测试）
  - `D3D11Device`：实现 `create_pipeline()` 接口，并更新 `create_buffer()` 以透传初始数据
  - `D3D11CommandList`：完整实现 `set_pipeline()`（绑定 VS/PS/InputLayout/BlendState 等），新增 `set_constant_buffer()` 实现
  - `xmake.lua`：添加 `d3dcompiler` 系统链接库
- **paint（RHI 渲染器）**：实现 `mine.paint.IRenderer` 基于 RHI 的真实渲染后端（M0.3 任务 #14）：
  - `RhiRenderer.cpp`：内嵌 HLSL 着色器源码（VS 做像素→NDC 变换，Y 轴翻转；PS 直接输出插值颜色）
  - 顶点格式：`PaintVertex{x, y, r, g, b, a}` = 24 字节；常量缓冲：`ViewportCB{width, height, pad, pad}` = 16 字节（D3D11 对齐要求）
  - `render(dl, target)`：将 FillRect 命令转换为三角形顶点（每矩形 6 顶点），通过 `create_buffer(desc, initial_data)` 创建不可变顶点缓冲，录制绘制命令后提交
  - 工厂函数 `mine::paint::create_renderer(IDevice*)` 在 `namespace mine::paint` 中实现
  - 架构决策：paint 模块直接依赖 `mine.gfx.rhi`，不依赖任何具体图形 API，无 mine.paint.d3d11 独立后端
- **samples/00-hello-rect（完成）**：实现 M0.3 端到端样例（M0.3 任务 #14），演示 Canvas → Paint 渲染器 → 交换链完整链路：
  - 创建 D3D11 设备 + `mine::paint::create_renderer()` 2D 渲染器 + 交换链
  - 每帧：Canvas 录制深灰色背景矩形 + 居中红色矩形 → `begin_frame` / `render` / `end_frame` → `present`
  - 订阅 Resized 事件自动更新交换链尺寸并重新渲染