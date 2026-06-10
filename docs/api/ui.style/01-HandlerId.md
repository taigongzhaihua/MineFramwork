# HandlerId 详细接口文档

## 概述

`HandlerId` 是 `mine.ui.style` 模块的**资源字典订阅取消令牌**。

**核心职责：**
- **订阅标识**：标识 ResourceDictionary 的资源变更订阅
- **取消订阅**：传入对应的 unsubscribe() 方法取消订阅
- **轻量类型**：uint32_t 别名，零开销

**核心特性：**
- **类型别名**：`using HandlerId = uint32_t;`
- **无效值**：kInvalidHandlerId = 0（订阅失败或未订阅）
- **自增分配**：每次订阅返回递增的唯一 ID

**使用场景：**
- ResourceDictionary::subscribe() 订阅资源变更
- ResourceDictionary::unsubscribe() 取消订阅
- 控件订阅样式资源变更通知

---

## 文件位置

```
src/mine/ui/style/api/include/mine/ui/style/HandlerId.h
```

---

## 类型定义

```cpp
namespace mine::ui::style {

/// 资源字典订阅取消令牌。
/// 由 subscribe() / on_resource_changed() 返回,传入对应的 unsubscribe() /
/// off_resource_changed() 即可取消订阅。
/// 0 为无效值（kInvalidHandlerId）。
using HandlerId = uint32_t;

/// 无效订阅令牌常量。
inline constexpr HandlerId kInvalidHandlerId = 0;

}  // namespace mine::ui::style
```

---

## 常量说明

### kInvalidHandlerId

```cpp
inline constexpr HandlerId kInvalidHandlerId = 0;
```

**描述**：无效订阅令牌常量，值为 0。

**用途**：
- 订阅失败时返回（如 ResourceDictionary 不存在）
- 初始化未订阅状态
- 检查 HandlerId 是否有效

**示例**：
```cpp
HandlerId handler_id = dictionary->subscribe(callback);
if (handler_id == kInvalidHandlerId) {
    // 订阅失败
}
```

---

## 使用场景

### 1. 订阅资源变更

```cpp
ResourceDictionary* dict = new ResourceDictionary();

HandlerId handler_id = dict->subscribe([](const std::string& key) {
    std::cout << "资源 " << key << " 已变更" << std::endl;
});

// handler_id 用于后续取消订阅
```

---

### 2. 取消订阅

```cpp
// 订阅
HandlerId handler_id = dict->subscribe(callback);

// 稍后取消订阅
dict->unsubscribe(handler_id);

// handler_id 失效，不应再使用
```

---

### 3. 检查订阅是否有效

```cpp
HandlerId handler_id = dict->subscribe(callback);

if (handler_id != kInvalidHandlerId) {
    std::cout << "订阅成功，ID: " << handler_id << std::endl;
} else {
    std::cerr << "订阅失败" << std::endl;
}
```

---

### 4. 存储多个订阅

```cpp
class MyControl {
public:
    void subscribe_resources(ResourceDictionary* dict) {
        handler_ids_.push_back(dict->subscribe([this](auto& key) {
            on_resource_changed(key);
        }));
        
        handler_ids_.push_back(dict->subscribe([this](auto& key) {
            on_theme_changed(key);
        }));
    }
    
    void unsubscribe_all(ResourceDictionary* dict) {
        for (HandlerId id : handler_ids_) {
            dict->unsubscribe(id);
        }
        handler_ids_.clear();
    }

private:
    std::vector<HandlerId> handler_ids_;
};
```

---

### 5. RAII 订阅管理

```cpp
class ResourceSubscription {
public:
    ResourceSubscription(ResourceDictionary* dict, std::function<void(const std::string&)> callback)
        : dict_(dict), handler_id_(dict->subscribe(std::move(callback))) {}
    
    ~ResourceSubscription() {
        if (handler_id_ != kInvalidHandlerId) {
            dict_->unsubscribe(handler_id_);
        }
    }
    
    ResourceSubscription(const ResourceSubscription&) = delete;
    ResourceSubscription& operator=(const ResourceSubscription&) = delete;
    
    ResourceSubscription(ResourceSubscription&& other) noexcept
        : dict_(other.dict_), handler_id_(other.handler_id_) {
        other.handler_id_ = kInvalidHandlerId;
    }
    
    [[nodiscard]] bool is_valid() const noexcept {
        return handler_id_ != kInvalidHandlerId;
    }

private:
    ResourceDictionary* dict_;
    HandlerId handler_id_;
};

// 使用
{
    ResourceSubscription sub(dict, [](auto& key) { /* ... */ });
    // 自动取消订阅（析构时）
}
```

---

### 6. 条件订阅

```cpp
HandlerId subscribe_if_needed(ResourceDictionary* dict, bool enable) {
    if (enable) {
        return dict->subscribe([](auto& key) { /* ... */ });
    }
    return kInvalidHandlerId;  // 未订阅
}

// 使用
HandlerId id = subscribe_if_needed(dict, should_subscribe);
if (id != kInvalidHandlerId) {
    // 已订阅，稍后取消
    dict->unsubscribe(id);
}
```

---

### 7. 订阅生命周期管理

```cpp
class ThemedControl {
public:
    void attach_theme(ResourceDictionary* theme) {
        detach_theme();  // 先取消旧订阅
        
        theme_ = theme;
        theme_handler_ = theme->subscribe([this](auto& key) {
            refresh_theme();
        });
    }
    
    void detach_theme() {
        if (theme_ && theme_handler_ != kInvalidHandlerId) {
            theme_->unsubscribe(theme_handler_);
            theme_handler_ = kInvalidHandlerId;
        }
        theme_ = nullptr;
    }
    
    ~ThemedControl() {
        detach_theme();
    }

private:
    ResourceDictionary* theme_ = nullptr;
    HandlerId theme_handler_ = kInvalidHandlerId;
};
```

---

## 最佳实践

### 1. 初始化为 kInvalidHandlerId

```cpp
// ✅ 推荐：初始化为 kInvalidHandlerId
class MyControl {
    HandlerId handler_id_ = kInvalidHandlerId;
};

// ❌ 不推荐：未初始化（未定义值）
class MyControl {
    HandlerId handler_id_;  // ❌ 可能包含随机值
};
```

---

### 2. 取消订阅后置为 kInvalidHandlerId

```cpp
// ✅ 推荐：取消订阅后置为无效值
void unsubscribe() {
    if (handler_id_ != kInvalidHandlerId) {
        dict_->unsubscribe(handler_id_);
        handler_id_ = kInvalidHandlerId;  // ✅ 防止重复取消
    }
}

// ❌ 不推荐：取消订阅后不置为无效值
void unsubscribe() {
    dict_->unsubscribe(handler_id_);  // ❌ 可能重复取消
}
```

---

### 3. 检查有效性后再使用

```cpp
// ✅ 推荐：检查有效性
void unsubscribe() {
    if (handler_id_ != kInvalidHandlerId) {
        dict_->unsubscribe(handler_id_);
    }
}

// ❌ 不推荐：不检查直接取消（可能取消无效 ID）
void unsubscribe() {
    dict_->unsubscribe(handler_id_);  // ❌ 可能是 kInvalidHandlerId
}
```

---

### 4. 使用 RAII 管理订阅

```cpp
// ✅ 推荐：RAII 自动管理
class MyControl {
    ResourceSubscription subscription_;  // ✅ 析构时自动取消
};

// ❌ 不推荐：手动管理（容易忘记取消）
class MyControl {
    HandlerId handler_id_;
    
    ~MyControl() {
        // ❌ 容易忘记取消订阅
    }
};
```

---

## 常见陷阱

### 1. 重复取消订阅

```cpp
// ❌ 问题：重复取消同一订阅
dict->unsubscribe(handler_id);
dict->unsubscribe(handler_id);  // ❌ 重复取消（可能导致错误）

// ✅ 解决：取消后置为 kInvalidHandlerId
dict->unsubscribe(handler_id);
handler_id = kInvalidHandlerId;

if (handler_id != kInvalidHandlerId) {
    dict->unsubscribe(handler_id);  // ✅ 不会重复执行
}
```

---

### 2. 未初始化 HandlerId

```cpp
// ❌ 问题：未初始化（包含随机值）
class MyControl {
    HandlerId handler_id_;  // ❌ 未定义值
    
    void unsubscribe() {
        if (handler_id_ != kInvalidHandlerId) {  // ❌ 可能误判
            dict_->unsubscribe(handler_id_);
        }
    }
};

// ✅ 解决：初始化为 kInvalidHandlerId
class MyControl {
    HandlerId handler_id_ = kInvalidHandlerId;  // ✅ 明确初始化
};
```

---

### 3. 订阅后未检查有效性

```cpp
// ❌ 问题：订阅后未检查有效性（订阅可能失败）
HandlerId handler_id = dict->subscribe(callback);
// 假定订阅成功，直接使用 handler_id

// ✅ 解决：检查有效性
HandlerId handler_id = dict->subscribe(callback);
if (handler_id == kInvalidHandlerId) {
    std::cerr << "订阅失败" << std::endl;
    return;
}
```

---

### 4. 取消订阅后仍使用 HandlerId

```cpp
// ❌ 问题：取消订阅后仍使用 HandlerId
HandlerId handler_id = dict->subscribe(callback);
dict->unsubscribe(handler_id);
dict->unsubscribe(handler_id);  // ❌ handler_id 已失效

// ✅ 解决：取消后置为 kInvalidHandlerId
HandlerId handler_id = dict->subscribe(callback);
dict->unsubscribe(handler_id);
handler_id = kInvalidHandlerId;  // ✅ 明确标记失效
```

---

## 完整示例

```cpp
#include <mine/ui/style/HandlerId.h>
#include <mine/ui/style/ResourceDictionary.h>
#include <iostream>
#include <vector>

using namespace mine::ui::style;

// ────────────────────────────────────────────────────────────────────────────
// RAII 订阅管理器
// ────────────────────────────────────────────────────────────────────────────

class ResourceSubscription {
public:
    ResourceSubscription(ResourceDictionary* dict, std::function<void(const std::string&)> callback)
        : dict_(dict), handler_id_(dict->subscribe(std::move(callback))) {
        if (handler_id_ == kInvalidHandlerId) {
            std::cerr << "订阅失败" << std::endl;
        }
    }
    
    ~ResourceSubscription() {
        if (handler_id_ != kInvalidHandlerId && dict_) {
            dict_->unsubscribe(handler_id_);
        }
    }
    
    // 禁止复制
    ResourceSubscription(const ResourceSubscription&) = delete;
    ResourceSubscription& operator=(const ResourceSubscription&) = delete;
    
    // 允许移动
    ResourceSubscription(ResourceSubscription&& other) noexcept
        : dict_(other.dict_), handler_id_(other.handler_id_) {
        other.dict_ = nullptr;
        other.handler_id_ = kInvalidHandlerId;
    }
    
    [[nodiscard]] bool is_valid() const noexcept {
        return handler_id_ != kInvalidHandlerId;
    }
    
    [[nodiscard]] HandlerId id() const noexcept {
        return handler_id_;
    }

private:
    ResourceDictionary* dict_;
    HandlerId handler_id_;
};

// ────────────────────────────────────────────────────────────────────────────
// 主题化控件（管理多个订阅）
// ────────────────────────────────────────────────────────────────────────────

class ThemedButton {
public:
    ThemedButton(const std::string& text) : text_(text) {}
    
    void attach_theme(ResourceDictionary* theme) {
        detach_theme();  // 先取消旧订阅
        
        theme_ = theme;
        
        // 订阅背景色变更
        bg_handler_ = theme->subscribe([this](const std::string& key) {
            if (key == "ButtonBackground") {
                refresh_background();
            }
        });
        
        // 订阅前景色变更
        fg_handler_ = theme->subscribe([this](const std::string& key) {
            if (key == "ButtonForeground") {
                refresh_foreground();
            }
        });
        
        // 检查订阅是否成功
        if (bg_handler_ == kInvalidHandlerId || fg_handler_ == kInvalidHandlerId) {
            std::cerr << "主题订阅失败" << std::endl;
        } else {
            std::cout << "主题订阅成功（背景: " << bg_handler_
                      << ", 前景: " << fg_handler_ << "）" << std::endl;
        }
    }
    
    void detach_theme() {
        if (!theme_) return;
        
        // 取消背景色订阅
        if (bg_handler_ != kInvalidHandlerId) {
            theme_->unsubscribe(bg_handler_);
            bg_handler_ = kInvalidHandlerId;
        }
        
        // 取消前景色订阅
        if (fg_handler_ != kInvalidHandlerId) {
            theme_->unsubscribe(fg_handler_);
            fg_handler_ = kInvalidHandlerId;
        }
        
        theme_ = nullptr;
    }
    
    ~ThemedButton() {
        detach_theme();
    }

private:
    void refresh_background() {
        std::cout << "[" << text_ << "] 背景色已更新" << std::endl;
    }
    
    void refresh_foreground() {
        std::cout << "[" << text_ << "] 前景色已更新" << std::endl;
    }
    
    std::string text_;
    ResourceDictionary* theme_ = nullptr;
    HandlerId bg_handler_ = kInvalidHandlerId;
    HandlerId fg_handler_ = kInvalidHandlerId;
};

// ────────────────────────────────────────────────────────────────────────────
// 订阅管理器（批量管理）
// ────────────────────────────────────────────────────────────────────────────

class SubscriptionManager {
public:
    HandlerId subscribe(ResourceDictionary* dict, std::function<void(const std::string&)> callback) {
        HandlerId id = dict->subscribe(std::move(callback));
        if (id != kInvalidHandlerId) {
            subscriptions_.push_back({dict, id});
        }
        return id;
    }
    
    void unsubscribe_all() {
        for (auto& [dict, id] : subscriptions_) {
            dict->unsubscribe(id);
        }
        subscriptions_.clear();
    }
    
    size_t count() const {
        return subscriptions_.size();
    }
    
    ~SubscriptionManager() {
        unsubscribe_all();
    }

private:
    std::vector<std::pair<ResourceDictionary*, HandlerId>> subscriptions_;
};

// ────────────────────────────────────────────────────────────────────────────
// 主函数
// ────────────────────────────────────────────────────────────────────────────

int main() {
    ResourceDictionary* theme = new ResourceDictionary();
    
    // ════════════════════════════════════════════════════════════════════════
    // 1. 基础订阅与取消
    // ════════════════════════════════════════════════════════════════════════
    
    std::cout << "=== 1. 基础订阅与取消 ===" << std::endl;
    
    HandlerId handler1 = theme->subscribe([](const std::string& key) {
        std::cout << "订阅1: 资源 " << key << " 已变更" << std::endl;
    });
    
    std::cout << "订阅1 ID: " << handler1 << std::endl;
    
    // 取消订阅
    theme->unsubscribe(handler1);
    handler1 = kInvalidHandlerId;
    std::cout << "订阅1 已取消" << std::endl;
    
    // ════════════════════════════════════════════════════════════════════════
    // 2. RAII 订阅管理
    // ════════════════════════════════════════════════════════════════════════
    
    std::cout << "\n=== 2. RAII 订阅管理 ===" << std::endl;
    
    {
        ResourceSubscription sub(theme, [](const std::string& key) {
            std::cout << "RAII订阅: 资源 " << key << " 已变更" << std::endl;
        });
        
        std::cout << "RAII订阅 ID: " << sub.id() << std::endl;
        std::cout << "RAII订阅有效: " << (sub.is_valid() ? "是" : "否") << std::endl;
        
        // 离开作用域自动取消订阅
    }
    std::cout << "RAII订阅已自动取消" << std::endl;
    
    // ════════════════════════════════════════════════════════════════════════
    // 3. 主题化控件
    // ════════════════════════════════════════════════════════════════════════
    
    std::cout << "\n=== 3. 主题化控件 ===" << std::endl;
    
    ThemedButton* button = new ThemedButton("确定");
    button->attach_theme(theme);
    
    // 触发资源变更
    theme->set_resource("ButtonBackground", Color{255, 0, 0, 255});
    theme->set_resource("ButtonForeground", Color{255, 255, 255, 255});
    
    button->detach_theme();
    delete button;
    
    // ════════════════════════════════════════════════════════════════════════
    // 4. 批量订阅管理
    // ════════════════════════════════════════════════════════════════════════
    
    std::cout << "\n=== 4. 批量订阅管理 ===" << std::endl;
    
    SubscriptionManager manager;
    
    manager.subscribe(theme, [](auto& key) { std::cout << "订阅A: " << key << std::endl; });
    manager.subscribe(theme, [](auto& key) { std::cout << "订阅B: " << key << std::endl; });
    manager.subscribe(theme, [](auto& key) { std::cout << "订阅C: " << key << std::endl; });
    
    std::cout << "订阅数量: " << manager.count() << std::endl;
    
    manager.unsubscribe_all();
    std::cout << "所有订阅已取消，剩余: " << manager.count() << std::endl;
    
    // 清理
    delete theme;
    
    return 0;
}
```

**预期输出：**
```
=== 1. 基础订阅与取消 ===
订阅1 ID: 1
订阅1 已取消

=== 2. RAII 订阅管理 ===
RAII订阅 ID: 2
RAII订阅有效: 是
RAII订阅已自动取消

=== 3. 主题化控件 ===
主题订阅成功（背景: 3, 前景: 4）
[确定] 背景色已更新
[确定] 前景色已更新

=== 4. 批量订阅管理 ===
订阅数量: 3
所有订阅已取消，剩余: 0
```

---

## 总结

`HandlerId` 是 `mine.ui.style` 模块的资源字典订阅取消令牌，具备：

1. **类型别名**：uint32_t 别名，零开销
2. **无效值**：kInvalidHandlerId = 0
3. **订阅标识**：标识 ResourceDictionary 订阅
4. **取消订阅**：传入 unsubscribe() 取消订阅

**使用建议**：
- **初始化为 kInvalidHandlerId**（避免未定义值）
- **取消订阅后置为 kInvalidHandlerId**（防止重复取消）
- **检查有效性后再使用**（避免取消无效 ID）
- **使用 RAII 管理订阅**（自动取消，防止泄漏）
- 避免重复取消同一订阅
- 避免未初始化 HandlerId
- 避免订阅后未检查有效性
- 避免取消订阅后仍使用 HandlerId
