# Changelog

本项目遵循 Keep a Changelog，版本号遵循语义化版本。

## [Unreleased]

### Added
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