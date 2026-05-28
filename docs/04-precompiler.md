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

`mine::ui::Window` 提供虚析构函数，支持继承和多态。mmlc 生成一个**继承自 `Window` 的 Base 类**（`DemoWindowBase : public ::mine::ui::Window`），用户的 code-behind 类再继承 Base——这与 WPF 的 `.xaml` / `.xaml.cpp` 分层完全对应。

**析构顺序保证**：mmlc 在生成的 `~DemoWindowBase()` 析构体中**第一句调用 `close()`**，确保渲染循环停止在任何数据成员析构之前；之后 C++ 析构数据成员（UI 元素），最后析构基类 `Window`（此时已是 no-op）。

---

#### 生成文件：DemoWindow.g.h / DemoWindow.g.cpp（不可手改）

```cpp
// DemoWindow.g.h
namespace app {

// !! 由 mmlc 自动生成，请勿手动修改 !!
// 用户在 DemoWindow.h / DemoWindow.cpp 中继承本类完成 code-behind

class DemoWindowBase : public ::mine::ui::Window {
    MINE_REFLECT_DECL();
public:
    DemoWindowBase();
    ~DemoWindowBase() override;   // 生成体第一句调用 close()，保证析构安全

    // ── MML property / signal ────────────────────────────────────────────
    MINE_PROP(::mine::String, StatusText);
    MINE_SIGNAL(closeRequested);

    // ── MML method → 纯虚，code-behind 必须 override ────────────────────
    virtual void on_count_clicked() = 0;
    virtual void on_reset_clicked() = 0;

protected:
    // ── #id 元素 → protected 成员（通过引用暴露），code-behind 可直接访问 ──
    //    对应 WPF 中的 x:Name，等效于在 code-behind 中访问命名控件
    ::mine::ui::Button&    btn_count_    { btn_count_s_    };
    ::mine::ui::TextBlock& status_label_ { status_label_s_ };

private:
    void _configure();  // 配置窗口属性（title/size/resizable），构造函数中调用
    void _build();      // 构建视觉树
    void _bind();       // 安装属性绑定
    void _states();     // 状态机

    // 无 #id 的元素（私有，code-behind 不可直接访问）
    ::mine::ui::Grid       root_grid_;
    ::mine::ui::StackPanel content_panel_;

    // 有 #id 的元素存储体（通过 protected 引用暴露给 code-behind）
    ::mine::ui::Button     btn_count_s_;
    ::mine::ui::TextBlock  status_label_s_;

    // 注意：无 win_ 成员——Window 本身就是 this（继承而非组合）
};

} // namespace app
```

```cpp
// DemoWindow.g.cpp（节选）
DemoWindowBase::DemoWindowBase() {
    _configure();   // 设置 pending 属性，首次 show() 前生效
    _build();       // 构建视觉树并 set_content
    _bind();        // 安装绑定
    _states();      // 状态机
}

DemoWindowBase::~DemoWindowBase() {
    // 第一句调用 close()：停止渲染循环，释放 swapchain，解注册 IWindowContext
    // 之后 C++ 析构数据成员（btn_count_s_, root_grid_, ...）→ 基类 ~Window()（no-op）
    if (!is_closed()) close();
}

void DemoWindowBase::_configure() {
    set_title("MineFramework - 演示");     // 直接调用继承自 Window 的方法（无 win_. 前缀）
    set_size({ 800.0f, 700.0f });
    set_resizable(true);
}

void DemoWindowBase::_build() {
    using namespace ::mine::ui;
    root_grid_.set_row_definitions({ RowDef::pixel(60), RowDef::star() });

    status_label_s_.bind_text([this]{ return StatusText(); }, {prop_StatusText()});
    btn_count_s_.set_content("计数 +1");
    btn_count_s_.click().connect([this]{ on_count_clicked(); });   // 调用 method（多态分派）

    content_panel_.add_child(status_label_s_);
    content_panel_.add_child(btn_count_s_);
    Grid::set_row(content_panel_, 1);
    root_grid_.add_child(content_panel_);

    set_content(root_grid_);   // 继承自 Window，直接调用
}
```

---

#### Code-Behind 文件：DemoWindow.h / DemoWindow.cpp（用户手写）

mmlc 检测到同目录下存在 `DemoWindow.h` 时，识别为 code-behind 存在，不再自动生成 `DemoWindow` 最终类。

```cpp
// DemoWindow.h（用户手写，code-behind 头文件）
#pragma once
#include "DemoWindow.g.h"

namespace app {

class DemoWindow final : public DemoWindowBase {
public:
    DemoWindow() = default;

    // 实现 MML 声明的 method
    void on_count_clicked() override;
    void on_reset_clicked() override;

private:
    int click_count_ = 0;   // 视图局部状态（不属于业务逻辑，不放 ViewModel）
};

} // namespace app
```

```cpp
// DemoWindow.cpp（用户手写，code-behind 实现）
#include "DemoWindow.h"
#include <cstdio>

namespace app {

void DemoWindow::on_count_clicked() {
    ++click_count_;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "当前计数：%d 次", click_count_);
    // 直接访问 protected 的 #id 元素（与 WPF code-behind 中访问命名控件一致）
    status_label_.set_text(buf);
    render();   // 继承自 Window，直接调用
}

void DemoWindow::on_reset_clicked() {
    click_count_ = 0;
    status_label_.set_text("当前计数：0 次");
    render();
}

} // namespace app
```

---

#### 调用方：Application 薄壳（手写）

```cpp
// main.cpp（手写薄壳）
struct MyApp : mine::ui::app::Application {
    app::DemoWindow main_win_;  // DemoWindow IS-A Window，多态链完整
                                // 可直接传给任何 Window& / Window* 参数

    void on_startup(int, char**) override {
        main_win_.closeRequested().connect([this]{ quit(); });
        main_win_.show();   // 继承自 Window，直接调用
    }
};
MINE_APPLICATION_MAIN(MyApp)
```

---

#### 析构顺序说明

```
MyApp 析构
  → main_win_ 析构（DemoWindow）
    → ~DemoWindowBase() 体：if (!is_closed()) close()
      → 渲染循环停止、swapchain 释放、IWindowContext 解注册
    → 析构数据成员（btn_count_s_, status_label_s_, content_panel_, root_grid_）
    → ~Window()（基类析构，此时已是 no-op）
```

---

#### 五文件分工

| 文件 | 来源 | 是否可手改 | 说明 |
|------|------|-----------|------|
| `DemoWindow.mml` | 用户写 | ✅ | MML 声明：属性/信号/method/#id/视觉树 |
| `DemoWindow.g.h` | mmlc 生成 | ❌ | `DemoWindowBase : Window`，Base 类接口 |
| `DemoWindow.g.cpp` | mmlc 生成 | ❌ | `_configure/_build/_bind/_states` + 析构保证 |
| `DemoWindow.h` | 用户写 | ✅ | code-behind 头：`DemoWindow : DemoWindowBase` |
| `DemoWindow.cpp` | 用户写 | ✅ | code-behind 实现，可操作 `protected` UI 元素 |

若无需 code-behind（纯 MML 声明即可），则 `DemoWindow.h/.cpp` 可省略；mmlc 自动生成空的 `DemoWindow : DemoWindowBase` 最终类（此时 `method` 声明会导致编译错误，不应使用）。


## 4.5 增量与缓存

* 每个 `.mml` 产生 `.d` 文件供 xmake 做增量。
* `mmlc` 使用**内容散列 + 依赖闭包散列**作为缓存键，缓存到 `build/.mmlcache/`。
* 工具链共享缓存（LSP 与 mmlc 共用）。

## 4.6 错误诊断

* 每个诊断条目含：等级、码（`MML0001` …）、SourceRange、关联源、修复建议（`fix-it`）。
* 输出格式：人类可读 + JSON（供 IDE）。
* 错误码表集中在 `docs/diag/` 下，文档自动生成。
