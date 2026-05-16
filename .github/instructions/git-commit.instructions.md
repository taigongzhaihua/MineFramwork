---
applyTo: "**"
description: >
  MineUI 项目 Git 提交消息规范。
  使用场景：撰写 git commit、生成 commit message、检查提交格式、
  暂存变更后提交、code review 评论中引用提交、生成 changelog。
---

# MineUI Git 提交规范

## 格式

```
<type>(<scope>): <subject>

[body]

[footer]
```

**每行最长 100 字符。**

---

## type（必填）

| type | 含义 |
|------|------|
| `feat` | 新功能 |
| `fix` | Bug 修复 |
| `docs` | 仅文档变更 |
| `style` | 代码格式化（不影响逻辑） |
| `refactor` | 重构（不新增功能也不修复 Bug） |
| `perf` | 性能优化 |
| `test` | 测试相关 |
| `build` | 构建系统变更（xmake.lua、选项、第三方依赖） |
| `ci` | CI/CD 流水线变更 |
| `chore` | 杂项（工具、脚本、配置，不影响源码） |
| `revert` | 回滚提交 |

---

## scope（推荐填写，对应模块名）

使用**模块短名**（去掉 `mine.` 前缀），多个模块以 `/` 分隔：

| scope | 含义 |
|-------|------|
| `core` | mine.core / mine.containers / mine.math |
| `async` | mine.async |
| `io` | mine.io |
| `reflect` | mine.reflect / mine.meta |
| `platform` | mine.platform.* |
| `gfx` | mine.gfx.* |
| `paint` | mine.paint |
| `compose` | mine.compose |
| `text` | mine.text / mine.text.shape / mine.text.layout |
| `image` | mine.image |
| `ui.property` | mine.ui.property |
| `ui.binding` | mine.ui.binding |
| `ui.event` | mine.ui.event |
| `ui.input` | mine.ui.input |
| `ui.layout` | mine.ui.layout |
| `ui.visual` | mine.ui.visual |
| `ui.style` | mine.ui.style |
| `ui.anim` | mine.ui.animation |
| `ui.controls` | mine.ui.controls.* |
| `ui.window` | mine.ui.window |
| `ui.app` | mine.ui.app |
| `di` | mine.di |
| `mvvm` | mine.mvvm |
| `nav` | mine.nav |
| `i18n` | mine.localization |
| `config` | mine.config |
| `data` | mine.data.* |
| `net` | mine.net.* |
| `mml` | mine.mml.* |
| `mmlc` | tool.mmlc |
| `mmlls` | tool.mmlls |
| `mmlfmt` | tool.mmlfmt |
| `pack` | tool.minepack |
| `inst` | tool.mineinst |
| `vscode` | ext.vscode |
| `build` | xmake.lua / xmake/**（构建系统本身） |
| `docs` | docs/** |
| `samples` | samples/** |
| `ci` | .github/workflows/** |

---

## subject（必填）

- 用**中文**（符合项目全中文规范）
- **动词开头**，简明扼要
- 不以句号结尾
- 50 字以内

示例：

```
feat(ui.binding): 新增双向绑定 <=> 语法支持
fix(data): 修复 ORM 查询参数绑定越界问题
perf(compose): 优化脏区合并算法降低重绘次数
build: 升级 harfbuzz 至 8.5.0
```

---

## body（可选）

- 说明**做了什么、为什么做**，不重复 subject
- 每行 ≤ 100 字符
- 与 subject 空一行

---

## footer（可选）

| 格式 | 含义 |
|------|------|
| `BREAKING CHANGE: <描述>` | 不兼容变更，触发 MAJOR 版本号递增 |
| `Closes #<issue>` | 关闭 issue |
| `Refs #<issue>` | 引用 issue |
| `Co-authored-by: Name <email>` | 共同作者 |

---

## 完整示例

```
feat(ui.binding): 新增绑定回退值（fallback）支持

当绑定路径中间节点为 null 时，自动使用 fallback 值渲染，
而非留空。同时更新 MML 语法：

  text: bind(vm.Name, fallback: "未知用户");

对现有单向绑定 API 保持兼容，无破坏性变更。

Closes #42
```

---

## 规则校验清单

提交前逐项确认：

- [ ] `type` 在上表中
- [ ] `scope` 对应实际变更模块（多模块用 `/` 隔开）
- [ ] `subject` 中文、动词开头、≤ 50 字、无句号
- [ ] 破坏性变更已写 `BREAKING CHANGE:`
- [ ] 关联 issue 已写 `Closes #` 或 `Refs #`
- [ ] 代码已通过 `mmlfmt` + `clang-format` 格式化
- [ ] 无 `.g.h`、`.g.cpp`、`build/`、`.xmake/` 等生成文件被错误暂存

---

## 不允许的格式

```
# ❌ 无 type
更新 Visual 基类

# ❌ 英文 subject（项目规范全中文）
feat(core): add new allocator

# ❌ subject 末尾有句号
fix(net): 修复 HTTP 超时未触发重连。

# ❌ scope 使用全名而非短名
feat(mine.ui.visual): ...

# ❌ 多个无关变更混入一次提交
feat(ui.style) + fix(data): 样式更新+数据修复
```
