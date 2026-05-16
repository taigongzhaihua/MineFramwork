# 10 — MVVM、命令、导航、验证

## 10.1 ViewModel 基类

```cpp
namespace mine::mvvm {
class ViewModelBase : public mine::core::Object {
public:
    Signal<StringView /*propName*/> property_changed;
protected:
    template<class T> bool set(T& field, T value, StringView name);
    void raise(StringView name);
};
}
```

* `set(field, value, "Prop")` 在变化时自动 `raise`。
* `MINE_OBSERVABLE(Type, Name, default)` 宏一行定义 observable 属性。

## 10.2 命令

| 类型 | 用途 |
|------|------|
| `RelayCommand`     | 同步命令 |
| `RelayCommand<T>`  | 带参数 |
| `AsyncCommand`     | 协程 / Task<void>，自带 `IsExecuting` |
| `CompositeCommand` | 多个命令组合 |

## 10.3 集合

* `ObservableCollection<T>`：`Add/Remove/Clear/Move/Replace`，触发结构与项变更事件。
* `ReadOnlyView<T>`：过滤/排序/分组的视图（懒计算）。
* `GroupedCollection<T,K>`：分组绑定。

## 10.4 导航（mine.nav）

```cpp
class INavigationService {
    virtual Status navigate_to(StringView route, Variant param) = 0;
    virtual bool   go_back() = 0;
    virtual bool   can_go_back() const = 0;
    virtual Signal<NavigationEvent>& events() = 0;
};
```

* 路由表在 `Application::OnStartup` 注册：
  ```cpp
  nav.add_route("/login",  []{ return new app::views::LoginView(); });
  nav.add_route("/main",   []{ return new app::views::MainView(); });
  ```
* `Frame` 控件作为路由出口，支持过渡动画。
* 参数通过 `Variant` 传递；强类型可通过 `INavigationService::navigate_to<T>(args)`。

## 10.5 验证

```cpp
struct IValidator { Status validate(Variant const&) = 0; };

class ValidatableProperty<T> {
    void set(T value);
    Vector<ValidationError> errors() const;
    Signal<> errors_changed;
};
```

MML 中：

```mml
TextBox {
    text <=> vm.Email;
    validation: { rules: [ NotEmpty, EmailFormat ] }
}
```

`MINE_VALIDATE(prop, rule, ...)` 宏在 ViewModel 中绑定规则。

## 10.6 ViewModel 生命周期

| 钩子 | 何时触发 |
|------|----------|
| `on_initialize(args)` | 创建后，首次绑定前 |
| `on_navigated_to(args)` | 导航到 |
| `on_navigated_from()` | 离开 |
| `on_appear` / `on_disappear` | 视图可见性变化 |
| `on_dispose` | 销毁 |

## 10.7 Messenger（弱引用消息总线）

```cpp
mvvm::Messenger::default().subscribe<MyMsg>(this, [this](MyMsg const& m){ ... });
mvvm::Messenger::default().send(MyMsg{...});
```

* 默认弱订阅（避免内存泄漏）。
* 支持 token 作用域；ViewModel 销毁时自动取消订阅。

## 10.8 与 DI 集成

* ViewModel 默认通过 DI 容器构造，依赖在构造函数注入。
* 视图 `data_context` 通过 `services.resolve<TVM>()` 获取，MML 的 `@viewmodel` 指令产生该调用。
