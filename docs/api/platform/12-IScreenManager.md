# IScreenManager 详细接口文档

## 概述

`IScreenManager` 是 `mine.platform.abi` 模块的**多显示器管理接口**，提供显示器枚举与查询能力。

**核心特性：**
- **4 个方法**：screen_count、screen_at、primary_screen、screen_for_point
- **自动刷新**：DPI 改变或显示器热插拔时自动刷新信息
- **主线程调用**：所有方法均应在主线程调用
- **逻辑像素坐标**：所有坐标和尺寸使用逻辑像素

---

## 文件位置

```
src/mine/platform/abi/api/include/mine/platform/IScreenManager.h
```

---

## 类型定义

```cpp
namespace mine::platform {

class IScreenManager {
public:
    virtual ~IScreenManager() = default;
    
    /// 当前已连接的显示器数量
    [[nodiscard]] virtual int screen_count() const = 0;
    
    /**
     * @brief 获取指定索引的显示器信息。
     * @param index 从 0 开始，须 < screen_count()
     */
    [[nodiscard]] virtual ScreenInfo screen_at(int index) const = 0;
    
    /// 获取主显示器信息
    [[nodiscard]] virtual ScreenInfo primary_screen() const = 0;
    
    /**
     * @brief 获取包含指定逻辑坐标点的显示器信息。
     * @param p 虚拟桌面逻辑坐标
     * @return 最近的显示器信息（点不在任何显示器内时返回最近的一个）
     */
    [[nodiscard]] virtual ScreenInfo screen_for_point(math::Point p) const = 0;
};

}
```

---

## 成员方法

### screen_count()

```cpp
[[nodiscard]] virtual int screen_count() const = 0;
```

**描述**：当前已连接的显示器数量。

**返回**：显示器数量（≥ 1）

**特征**：
- 显示器热插拔后自动更新
- 至少返回 1（主显示器）

**示例**：
```cpp
IScreenManager* screen_manager = host->screen_manager();
int count = screen_manager->screen_count();
log("检测到 {} 个显示器", count);
```

---

### screen_at()

```cpp
[[nodiscard]] virtual ScreenInfo screen_at(int index) const = 0;
```

**描述**：获取指定索引的显示器信息。

**参数**：
- `index`：显示器索引（从 0 开始，须 < `screen_count()`）

**返回**：显示器信息（`ScreenInfo`）

**注意**：
- `index` 须在 `[0, screen_count())` 范围内
- 越界访问未定义行为

**示例**：
```cpp
int count = screen_manager->screen_count();
for (int i = 0; i < count; ++i) {
    ScreenInfo screen = screen_manager->screen_at(i);
    log("显示器 {}: {}x{}, DPI={}", 
        i, screen.bounds.width, screen.bounds.height, screen.dpi);
}
```

---

### primary_screen()

```cpp
[[nodiscard]] virtual ScreenInfo primary_screen() const = 0;
```

**描述**：获取主显示器信息。

**返回**：主显示器信息（`ScreenInfo`）

**特征**：
- 主显示器：系统启动时的默认显示器，通常任务栏在此显示器上
- 虚拟桌面原点：主显示器的左上角通常为 (0, 0)

**示例**：
```cpp
ScreenInfo primary = screen_manager->primary_screen();
log("主显示器: {}x{}, DPI={}", 
    primary.bounds.width, primary.bounds.height, primary.dpi);
```

---

### screen_for_point()

```cpp
[[nodiscard]] virtual ScreenInfo screen_for_point(math::Point p) const = 0;
```

**描述**：获取包含指定逻辑坐标点的显示器信息。

**参数**：
- `p`：虚拟桌面逻辑坐标

**返回**：包含该点的显示器信息（`ScreenInfo`）

**特征**：
- 点在任何显示器内：返回包含该点的显示器
- 点不在任何显示器内：返回最近的显示器

**示例**：
```cpp
math::Point window_position = window->position();
ScreenInfo screen = screen_manager->screen_for_point(window_position);
log("窗口位于显示器: DPI={}, 缩放={}x", screen.dpi, screen.scale);
```

---

## 使用场景

### 1. 枚举所有显示器

```cpp
void list_all_screens(IScreenManager* screen_manager) {
    int count = screen_manager->screen_count();
    log("检测到 {} 个显示器:", count);
    
    for (int i = 0; i < count; ++i) {
        ScreenInfo screen = screen_manager->screen_at(i);
        log("显示器 {}:", i);
        log("  位置: ({}, {})", screen.bounds.x, screen.bounds.y);
        log("  尺寸: {}x{}", screen.bounds.width, screen.bounds.height);
        log("  工作区: {}x{}", screen.work_area.width, screen.work_area.height);
        log("  DPI: {}", screen.dpi);
        log("  缩放: {}x", screen.scale);
        log("  主显示器: {}", screen.is_primary ? "是" : "否");
    }
}
```

---

### 2. 获取主显示器信息

```cpp
void get_primary_screen_info(IScreenManager* screen_manager) {
    ScreenInfo primary = screen_manager->primary_screen();
    
    log("主显示器信息:");
    log("  尺寸: {}x{}", primary.bounds.width, primary.bounds.height);
    log("  工作区: {}x{}", primary.work_area.width, primary.work_area.height);
    log("  DPI: {}", primary.dpi);
    log("  缩放: {}x", primary.scale);
}
```

---

### 3. 窗口居中显示在主显示器

```cpp
void center_window_on_primary_screen(IWindow* window, IScreenManager* screen_manager) {
    ScreenInfo primary = screen_manager->primary_screen();
    math::Rect work_area = primary.work_area;
    math::Size window_size = window->size();
    
    math::Point center = {
        work_area.x + (work_area.width - window_size.width) / 2,
        work_area.y + (work_area.height - window_size.height) / 2
    };
    
    window->set_position(center);
}
```

---

### 4. 查找窗口所在显示器

```cpp
void find_window_screen(IWindow* window, IScreenManager* screen_manager) {
    math::Point position = window->position();
    ScreenInfo screen = screen_manager->screen_for_point(position);
    
    log("窗口位于显示器: DPI={}, 缩放={}x", screen.dpi, screen.scale);
    
    // 根据显示器 DPI 调整窗口内容
    adjust_for_dpi(screen.dpi);
}
```

---

### 5. 多显示器布局

```cpp
void show_multi_screen_layout(IScreenManager* screen_manager) {
    int count = screen_manager->screen_count();
    
    for (int i = 0; i < count; ++i) {
        ScreenInfo screen = screen_manager->screen_at(i);
        log("显示器 {}: ({}, {}) {}x{}", 
            i, 
            screen.bounds.x, screen.bounds.y,
            screen.bounds.width, screen.bounds.height);
    }
}
```

---

### 6. 全屏窗口

```cpp
void enter_fullscreen(IWindow* window, IScreenManager* screen_manager) {
    ScreenInfo primary = screen_manager->primary_screen();
    
    // 设置窗口尺寸为显示器完整尺寸
    window->set_position({primary.bounds.x, primary.bounds.y});
    window->set_size({primary.bounds.width, primary.bounds.height});
    
    // 无边框全屏
    window->set_state(WindowState::Maximized);
}
```

---

### 7. DPI 感知资源加载

```cpp
void load_high_dpi_assets(IWindow* window, IScreenManager* screen_manager) {
    math::Point position = window->position();
    ScreenInfo screen = screen_manager->screen_for_point(position);
    float scale = screen.scale;
    
    if (scale >= 2.0f) {
        resource_manager->load("assets@2x");
    } else if (scale >= 1.5f) {
        resource_manager->load("assets@1.5x");
    } else {
        resource_manager->load("assets");
    }
}
```

---

## 显示器热插拔

### 自动刷新

**特征**：
- 显示器热插拔后，`screen_count()` 自动更新
- 应用通过监听 `DpiChanged` 事件感知显示器变化

**示例**：
```cpp
class Application : public IWindowEventSink {
public:
    void on_window_event(WindowEvent& event) override {
        if (event.kind == WindowEventKind::DpiChanged) {
            // 显示器可能变化，重新查询
            refresh_screen_info();
        }
    }
    
private:
    void refresh_screen_info() {
        int count = screen_manager_->screen_count();
        log("当前显示器数量: {}", count);
        
        // 重新查询窗口所在显示器
        math::Point position = window_->position();
        ScreenInfo screen = screen_manager_->screen_for_point(position);
        adjust_for_dpi(screen.dpi);
    }
};
```

---

## 虚拟桌面坐标系

### 多显示器坐标系统

**特征**：
- **虚拟桌面**：所有显示器组成的逻辑桌面
- **原点**：主显示器的左上角通常为 (0, 0)
- **负坐标**：副显示器可能有负坐标（位于主显示器左侧或上方）

**示例**：
```cpp
// 主显示器: (0, 0) - (1920, 1080)
// 副显示器: (-1920, 0) - (0, 1080) （位于主显示器左侧）

IScreenManager* screen_manager = host->screen_manager();
int count = screen_manager->screen_count();

for (int i = 0; i < count; ++i) {
    ScreenInfo screen = screen_manager->screen_at(i);
    log("显示器 {}: ({}, {}) {}x{}", 
        i, 
        screen.bounds.x, screen.bounds.y,  // 可能为负
        screen.bounds.width, screen.bounds.height);
}
```

---

## 最佳实践

### 1. 使用 primary_screen() 而非 screen_at(0)

```cpp
// ✅ 推荐：使用 primary_screen()
ScreenInfo primary = screen_manager->primary_screen();

// ❌ 不推荐：假设主显示器索引为 0
ScreenInfo primary = screen_manager->screen_at(0);  // 可能不是主显示器
```

---

### 2. 使用 screen_for_point() 查找窗口所在显示器

```cpp
// ✅ 推荐：使用 screen_for_point()
math::Point position = window->position();
ScreenInfo screen = screen_manager->screen_for_point(position);

// ❌ 不推荐：遍历所有显示器
for (int i = 0; i < screen_manager->screen_count(); ++i) {
    ScreenInfo screen = screen_manager->screen_at(i);
    if (screen.bounds.contains(position)) {
        // ...
    }
}
```

---

### 3. 检查索引范围

```cpp
// ✅ 推荐：检查索引范围
int index = 2;
if (index < screen_manager->screen_count()) {
    ScreenInfo screen = screen_manager->screen_at(index);
    // ...
}

// ❌ 不推荐：不检查索引范围
ScreenInfo screen = screen_manager->screen_at(2);  // 可能越界
```

---

### 4. 使用 work_area 而非 bounds 居中窗口

```cpp
// ✅ 推荐：使用 work_area
ScreenInfo screen = screen_manager->primary_screen();
math::Point center = {
    screen.work_area.x + (screen.work_area.width - window_size.width) / 2,
    screen.work_area.y + (screen.work_area.height - window_size.height) / 2
};

// ❌ 不推荐：使用 bounds（可能被任务栏遮挡）
math::Point center = {
    screen.bounds.x + (screen.bounds.width - window_size.width) / 2,
    screen.bounds.y + (screen.bounds.height - window_size.height) / 2
};
```

---

### 5. DPI 变化时重新查询显示器信息

```cpp
// ✅ 推荐：DPI 变化时重新查询
void on_window_event(WindowEvent& event) override {
    if (event.kind == WindowEventKind::DpiChanged) {
        math::Point position = window->position();
        ScreenInfo screen = screen_manager->screen_for_point(position);
        reload_resources(screen.dpi);
    }
}

// ❌ 不推荐：缓存显示器信息
ScreenInfo cached_screen = screen_manager->primary_screen();
// DPI 变化后 cached_screen 过时
```

---

## 常见陷阱

### 1. 假设主显示器索引为 0

```cpp
// ❌ 问题：假设主显示器索引为 0
ScreenInfo primary = screen_manager->screen_at(0);  // 可能不是主显示器

// ✅ 解决：使用 primary_screen()
ScreenInfo primary = screen_manager->primary_screen();
```

---

### 2. 索引越界

```cpp
// ❌ 问题：索引越界
int count = screen_manager->screen_count();  // 返回 2
ScreenInfo screen = screen_manager->screen_at(2);  // 越界

// ✅ 解决：检查索引范围
if (index < count) {
    ScreenInfo screen = screen_manager->screen_at(index);
}
```

---

### 3. 缓存显示器信息不更新

```cpp
// ❌ 问题：缓存显示器信息，显示器热插拔后过时
class Application {
    ScreenInfo cached_screen_ = screen_manager->primary_screen();
    
    void render() {
        use_dpi(cached_screen_.dpi);  // 可能过时
    }
};

// ✅ 解决：每次使用时查询
void render() {
    ScreenInfo screen = screen_manager->primary_screen();
    use_dpi(screen.dpi);
}
```

---

### 4. 假设所有显示器 DPI 相同

```cpp
// ❌ 问题：假设所有显示器 DPI 相同
ScreenInfo primary = screen_manager->primary_screen();
float dpi = primary.dpi;  // 假设所有显示器 DPI 相同
load_resources(dpi);

// ✅ 解决：根据窗口所在显示器查询 DPI
math::Point position = window->position();
ScreenInfo screen = screen_manager->screen_for_point(position);
load_resources(screen.dpi);
```

---

### 5. 使用 bounds 而非 work_area 居中窗口

```cpp
// ❌ 问题：使用 bounds，窗口被任务栏遮挡
ScreenInfo screen = screen_manager->primary_screen();
math::Point center = {
    screen.bounds.x + (screen.bounds.width - window_size.width) / 2,
    screen.bounds.y + (screen.bounds.height - window_size.height) / 2
};

// ✅ 解决：使用 work_area
math::Point center = {
    screen.work_area.x + (screen.work_area.width - window_size.width) / 2,
    screen.work_area.y + (screen.work_area.height - window_size.height) / 2
};
```

---

## 完整示例

```cpp
#include <mine/platform/IApplicationHost.h>
#include <mine/platform/IScreenManager.h>
#include <mine/platform/IWindow.h>

class Application : public IWindowEventSink {
    IApplicationHost* host_;
    IScreenManager* screen_manager_;
    IWindow* window_;
    
public:
    Application(IApplicationHost* host) : host_(host) {
        screen_manager_ = host_->screen_manager();
        
        // 列出所有显示器
        list_all_screens();
        
        // 创建窗口
        WindowDesc desc;
        desc.title = "My Application";
        desc.size = {800, 600};
        auto window_ptr = host_->create_window(desc);
        window_ = window_ptr.release();
        
        // 订阅事件
        window_->events().subscribe(this);
        
        // 居中显示在主显示器
        center_on_primary_screen();
        
        // 显示窗口
        window_->show();
    }
    
    ~Application() {
        if (window_) {
            window_->events().unsubscribe(this);
        }
    }
    
    void on_window_event(WindowEvent& event) override {
        switch (event.kind) {
        case WindowEventKind::Moved:
            handle_window_moved();
            break;
            
        case WindowEventKind::DpiChanged:
            handle_dpi_changed(event);
            break;
            
        case WindowEventKind::Closed:
            cleanup();
            break;
            
        default:
            break;
        }
    }
    
private:
    void list_all_screens() {
        int count = screen_manager_->screen_count();
        log("检测到 {} 个显示器:", count);
        
        for (int i = 0; i < count; ++i) {
            ScreenInfo screen = screen_manager_->screen_at(i);
            log("显示器 {}:", i);
            log("  位置: ({}, {})", screen.bounds.x, screen.bounds.y);
            log("  尺寸: {}x{}", screen.bounds.width, screen.bounds.height);
            log("  工作区: {}x{}", screen.work_area.width, screen.work_area.height);
            log("  DPI: {}", screen.dpi);
            log("  缩放: {}x", screen.scale);
            log("  主显示器: {}", screen.is_primary ? "是" : "否");
        }
    }
    
    void center_on_primary_screen() {
        ScreenInfo primary = screen_manager_->primary_screen();
        math::Rect work_area = primary.work_area;
        math::Size window_size = window_->size();
        
        math::Point center = {
            work_area.x + (work_area.width - window_size.width) / 2,
            work_area.y + (work_area.height - window_size.height) / 2
        };
        
        window_->set_position(center);
    }
    
    void handle_window_moved() {
        math::Point position = window_->position();
        ScreenInfo screen = screen_manager_->screen_for_point(position);
        
        log("窗口位于显示器: DPI={}, 缩放={}x", screen.dpi, screen.scale);
        adjust_for_dpi(screen.dpi);
    }
    
    void handle_dpi_changed(const WindowEvent& event) {
        log("DPI 变化: {}", event.new_dpi);
        window_->set_position(event.suggested_rect.position());
        window_->set_size(event.suggested_rect.size());
        reload_resources(event.new_dpi);
    }
    
    void cleanup() {
        log("清理资源");
        window_->events().unsubscribe(this);
        window_ = nullptr;
    }
    
    void adjust_for_dpi(float dpi) {
        log("根据 DPI 调整窗口内容: {}", dpi);
    }
    
    void reload_resources(float dpi) {
        log("重新加载资源: DPI={}", dpi);
    }
};

int main() {
    auto host = platform::initialize();
    Application app(host.get());
    host->run();
    return 0;
}
```

---

## 总结

`IScreenManager` 是 `mine.platform.abi` 模块的多显示器管理接口，具备：

1. **4 个方法**：screen_count、screen_at、primary_screen、screen_for_point
2. **自动刷新**：DPI 改变或显示器热插拔时自动刷新信息
3. **主线程调用**：所有方法均应在主线程调用
4. **逻辑像素坐标**：所有坐标和尺寸使用逻辑像素

**使用建议**：
- 使用 `primary_screen()` 而非 `screen_at(0)` 获取主显示器
- 使用 `screen_for_point()` 查找窗口所在显示器
- 检查索引范围避免越界
- 使用 `work_area` 而非 `bounds` 居中窗口
- DPI 变化时重新查询显示器信息
- 不缓存显示器信息（显示器热插拔后过时）
- 不假设所有显示器 DPI 相同
