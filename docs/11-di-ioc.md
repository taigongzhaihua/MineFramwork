# 11 — DI / IoC 容器

## 11.1 设计目标

* 编译期可静态校验注册关系（缺失依赖 → 编译错误，不到运行期才报错）。
* 不依赖 RTTI（`mine::TypeId<T>` 自生成稳定 token）。
* 支持构造函数注入、属性注入、工厂注入。
* 支持作用域（Singleton / Scoped / Transient）。
* 支持 Lazy、Optional、命名服务、多实现集合。

## 11.2 API

```cpp
namespace mine::di {

class ServiceCollection {
public:
    template<class TService, class TImpl, class... Deps>
    ServiceCollection& add_singleton();

    template<class TService, class TImpl, class... Deps>
    ServiceCollection& add_scoped();

    template<class TService, class TImpl, class... Deps>
    ServiceCollection& add_transient();

    template<class TService>
    ServiceCollection& add_factory(Function<TService*(Resolver&)>);

    template<class TService>
    ServiceCollection& add_instance(TService* instance);

    ServiceProvider build() &&;
};

class ServiceProvider {
public:
    template<class T> T*  resolve();                    // 必需
    template<class T> T*  try_resolve();                // 可空
    template<class T> auto resolve_all() -> Span<T*>;   // 多实现
    Scope create_scope();
};
}
```

## 11.3 注册与解析

```cpp
ServiceCollection sc;
sc.add_singleton<ILogger, ConsoleLogger>()
  .add_singleton<IConfig,  JsonConfig>()
  .add_scoped   <IUnitOfWork, SqliteUnitOfWork, IConfig>()
  .add_transient<LoginVM, LoginVM, IUnitOfWork, ILogger>();

auto sp = std::move(sc).build();

auto* vm = sp.resolve<LoginVM>();
```

* 构造参数列表通过模板包 `Deps...` 描述，编译期生成调用绑定。
* 缺失依赖在 `build()` 时一次性诊断并报告。

## 11.4 与 MVVM/MML 集成

* `Application::ConfigureServices(ServiceCollection&)` 是用户主入口。
* MML 生成的视图自动从 `Application::services()` 解析其 `@viewmodel`。
* 控件作者可在控件构造里 `services().resolve<T>()` 拿服务（建议仅在 root 控件）。

## 11.5 作用域

* 进程：`Singleton` 全局唯一。
* 窗口：`window scope`，每窗口一份，关闭窗口释放。
* 页面/导航：`page scope`，每次导航产生新作用域。
* 自定义：用户可手动 `create_scope()`。

## 11.6 弱引用与生命周期

* `OwnedPtr<T>` 表示拥有；
* `BorrowedPtr<T>` 表示非拥有引用；
* 容器统一管理 `OwnedPtr`，调用者拿 `BorrowedPtr`。
* 释放顺序：scope 倒序销毁；scoped 先于 singleton 释放。

## 11.7 编译期反射方案

* 使用 `MINE_INJECT(Class, Deps...)` 宏在类中声明可注入构造：
  ```cpp
  class LoginVM : public ViewModelBase {
      MINE_INJECT(LoginVM, IUnitOfWork*, ILogger*);
  public:
      LoginVM(IUnitOfWork* uow, ILogger* log);
  };
  ```
* 容器通过 `MINE_INJECT` 暴露的元信息生成构造闭包。

## 11.8 模块装配

* `IServiceModule { void configure(ServiceCollection&); }` 作为模块化注册单元。
* 框架自带模块：`UiModule`、`DataModule`、`NetModule`，应用按需 `add_module<UiModule>()`。
