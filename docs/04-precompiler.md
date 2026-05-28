# 04 — 预编译器 mmlc

`tool.mmlc` 把 `.mml` 编译为 C++ 源码 + 头文件，参与 xmake 构建图。

## 4.1 编译流水线

```
.mml ─► [Lexer] ─► Tokens
                   │
                   ▼
                [Parser] ─► AST
                                │
                                ▼
                          [Sema/Resolve] ─► Typed-AST
                                              │
                                              ▼
                                        [Lowering] ─► IR (MIR)
                                                          │
                                                          ▼
                                                    [CodeGen] ─► .h/.cpp
                                                          │
                                                          ▼
                                                    [Diagnostics]
```

各阶段职责（每个阶段均为**独立 C++ 库**，便于工具链复用）：

| 阶段 | 模块 | 输入 | 输出 |
|------|------|------|------|
| Lex | `mine.mml.lex` | 源串 | Token 流 + SourceMap |
| Parse | `mine.mml.parse` | Token | AST（含 trivia 用于格式化） |
| Sema | `mine.mml.sema` | AST + 符号表 | Typed-AST |
| Lower | `mine.mml.lower` | Typed-AST | MIR |
| CodeGen | `mine.mml.codegen.cpp` | MIR | C++ 源 |
| Diag | `mine.mml.diagnostics` | 任意阶段 | 诊断条目 |

LSP / 格式化器 / 设计器都基于同一组库，**严禁工具与编译器各自实现一份**。

## 4.2 CLI

```
mmlc [options] <inputs...>

  -I <dir>                添加 import 搜索路径
  -o <dir>                生成根目录
  -p, --project <file>    项目清单（YAML），描述 inputs/outputs/depends
  --emit cpp|json|ir      输出形式（默认 cpp）
  --vm-include <header>   注册 viewmodel 头
  --reflect <on|off>      是否生成运行期反射桥
  --hot-reload            生成热重载入口
  --strict                警告视为错误
  --jobs N                并行
  --depfile <path>        生成 ninja/xmake 用的 .d 依赖文件
  --sourcemap             生成 SourceMap，供调试器/LSP
```

每个 `.mml` 默认产出：

```
<out>/<module-path>/<Name>.g.h
<out>/<module-path>/<Name>.g.cpp
<out>/<module-path>/<Name>.g.d   # 依赖文件
```

## 4.3 与 xmake 集成

`xmake.lua` 提供 rule：

```lua
rule("mine.mml")
    set_extensions(".mml")
    on_load(function(target) target:add("deps", "tool.mmlc") end)
    before_build_file(function (target, sourcefile, opt)
        import("core.project.depend")
        local out = path.join(target:autogendir(), "mml")
        os.runv("mmlc", {"-o", out, "--depfile", out..".d", sourcefile})
        target:add("includedirs", out)
        target:add("files", out.."/**.cpp")
    end)
```

## 4.4 生成代码契约

### 4.4.1 UserControl 组件

对 `LoginView`（`component LoginView : UserControl`，见 §3）：

```cpp
// LoginView.g.h（节选）
namespace app::views {
class LoginView : public ::mine::ui::UserControl {
    MINE_REFLECT_DECL();
public:
    LoginView();
    // 强类型属性
    MINE_PROP(::mine::String, Title);
    MINE_PROP(bool,           Busy);
    // 信号
    MINE_SIGNAL(loginRequested, ::mine::String);
private:
    void _build();             // 构造视觉树
    void _bind();              // 安装绑定
    void _states();            // 状态机
};
}
```

```cpp
// LoginView.g.cpp（节选）
LoginView::LoginView() {
    _build(); _bind(); _states();
}
void LoginView::_build() {
    using namespace ::mine::ui;
    auto* sp = MINE_NEW(StackPanel);
    sp->set_spacing(8);
    sp->set_padding(16);
    {
        auto* tb = MINE_NEW(TextBlock);
        tb->bind_text([this]{ return Title(); }, /*deps*/ {prop_Title()});
        sp->add_child(tb);
    }
    /* ... */
    set_content(sp);
}
```

要点：

* 所有动态分配走 `MINE_NEW`（用统一 Allocator）。
* 绑定生成为 lambda + 依赖列表，由属性系统订阅。
* 不使用 RTTI / dynamic_cast；类型识别经 `mine.reflect` 静态注册。

---

### 4.4.2 Window 组件

对 `DemoWindow`（`component DemoWindow : Window`，见 §3.1 Window 示例）：

`mine::ui::Window` 是 Pimpl 值类型基础设施，**无虚析构，不可派生**。
因此 mmlc 生成**包装类（组合）**而非继承——与 Slint / WinUI3 的生成策略相同。

```cpp
// DemoWindow.g.h（完整接口）
namespace app {

class DemoWindow {
    MINE_REFLECT_DECL();
public:
    DemoWindow();
    ~DemoWindow() = default;

    // ── Window 生命周期委托 ──────────────────────────────────────────────────
    void show()    { win_.show(); }
    void hide()    { win_.hide(); }
    void close()   { win_.close(); }
    [[nodiscard]] bool is_closed()           const noexcept { return win_.is_closed(); }
    [[nodiscard]] ::mine::ui::Window& window()     noexcept { return win_; }

    // ── MML 声明的属性 ──────────────────────────────────────────────────────
    MINE_PROP(::mine::String, StatusText);

    // ── MML 声明的信号 ──────────────────────────────────────────────────────
    MINE_SIGNAL(closeRequested);

private:
    // ── 生成方法 ─────────────────────────────────────────────────────────────
    void _configure();  // 配置 win_ 属性：title/size/resizable 等（在构造函数中调用）
    void _build();      // 构建视觉树，最终调用 win_.set_content(...)
    void _bind();       // 安装属性绑定
    void _states();     // 状态机

    // ── 数据成员：UI 元素声明在 win_ 之前，保证析构顺序正确 ──────────────────
    ::mine::ui::Grid       root_grid_;
    ::mine::ui::TextBlock  title_bar_;
    ::mine::ui::StackPanel content_panel_;
    ::mine::ui::TextBlock  status_label_;
    ::mine::ui::Button     close_btn_;

    // win_ 最后声明 = 最先析构（GPU 资源与 swapchain 在 UI 元素之前释放）
    ::mine::ui::Window win_;
};

} // namespace app
```

```cpp
// DemoWindow.g.cpp（节选）
DemoWindow::DemoWindow() {
    _configure();   // 设置 pending 属性，首次 show() 前生效
    _build();       // 构建视觉树并 set_content
    _bind();        // 安装绑定
    _states();      // 状态机
}

void DemoWindow::_configure() {
    win_.set_title("MineFramework - 演示");
    win_.set_size({ .width = 800, .height = 700 });
    win_.set_resizable(true);
}

void DemoWindow::_build() {
    using namespace ::mine::ui;
    root_grid_.set_row_definitions({ RowDef::pixel(60), RowDef::star() });

    title_bar_.set_text("MineFramework 演示");
    title_bar_.set_font_size(22);
    Grid::set_row(title_bar_, 0);

    status_label_.bind_text([this]{ return StatusText(); }, {prop_StatusText()});
    close_btn_.set_content("关闭");
    close_btn_.click().connect([this]{ closeRequested().emit(); });

    content_panel_.add_child(status_label_);
    content_panel_.add_child(close_btn_);
    Grid::set_row(content_panel_, 1);

    root_grid_.add_child(title_bar_);
    root_grid_.add_child(content_panel_);

    win_.set_content(root_grid_);
}
```

**调用方（手写 Application）**：

```cpp
// main.cpp（手写薄壳）
struct MyApp : mine::ui::app::Application {
    app::DemoWindow main_win_;  // Window 组件包装类

    void on_startup(int, char**) override {
        main_win_.closeRequested().connect([this]{ quit(); });
        main_win_.show();  // 触发 IWindowContext 懒初始化，自动登记为主窗口
    }
};
```

**析构顺序**（最重要的正确性保证）：

```
MyApp 析构
  → main_win_ 析构（DemoWindow）
    → win_ 最先析构（声明最后 = 析构最先）
      → 原生窗口关闭、swapchain 释放、IWindowContext 解注册
    → close_btn_ 析构
    → status_label_ 析构
    → content_panel_ 析构
    → title_bar_ 析构
    → root_grid_ 析构（内容已被 win_ 析构时清空）
```

## 4.5 增量与缓存

* 每个 `.mml` 产生 `.d` 文件供 xmake 做增量。
* `mmlc` 使用**内容散列 + 依赖闭包散列**作为缓存键，缓存到 `build/.mmlcache/`。
* 工具链共享缓存（LSP 与 mmlc 共用）。

## 4.6 错误诊断

* 每个诊断条目含：等级、码（`MML0001` …）、SourceRange、关联源、修复建议（`fix-it`）。
* 输出格式：人类可读 + JSON（供 IDE）。
* 错误码表集中在 `docs/diag/` 下，文档自动生成。
