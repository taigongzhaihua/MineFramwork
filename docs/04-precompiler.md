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

对 `LoginView` 组件（见 §3）：

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

## 4.5 增量与缓存

* 每个 `.mml` 产生 `.d` 文件供 xmake 做增量。
* `mmlc` 使用**内容散列 + 依赖闭包散列**作为缓存键，缓存到 `build/.mmlcache/`。
* 工具链共享缓存（LSP 与 mmlc 共用）。

## 4.6 错误诊断

* 每个诊断条目含：等级、码（`MML0001` …）、SourceRange、关联源、修复建议（`fix-it`）。
* 输出格式：人类可读 + JSON（供 IDE）。
* 错误码表集中在 `docs/diag/` 下，文档自动生成。
