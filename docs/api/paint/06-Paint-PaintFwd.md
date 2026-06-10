# Paint 与 PaintFwd 详细接口文档

## 概述

`Paint` 和 `PaintFwd` 是 `mine.paint` 模块的**伞状头文件和前向声明头文件**。

**核心特性：**
- **Paint.h**：伞状头文件，包含所有其他头文件（一次性引入整个模块）
- **PaintFwd.h**：前向声明头文件，仅声明类型名称（减少编译依赖）

---

## 文件位置

```
src/mine/paint/api/include/mine/paint/Paint.h
src/mine/paint/api/include/mine/paint/PaintFwd.h
```

---

## Paint.h（伞状头文件）

### 文件内容

```cpp
/**
 * @file Paint.h
 * @brief mine.paint 模块伞状头文件。
 *
 * 一次性引入整个模块的所有公开类型：
 *   #include <mine/paint/Paint.h>
 *
 * 包含：
 *   - Brush, Pen（绘制属性）
 *   - Path, PathBuilder（几何路径）
 *   - DrawCmd, DisplayList（命令序列）
 *   - Canvas（绘制上下文）
 *   - IRenderer（渲染后端接口）
 */

#pragma once

#include <mine/paint/PaintFwd.h>
#include <mine/paint/Brush.h>
#include <mine/paint/Pen.h>
#include <mine/paint/Path.h>
#include <mine/paint/PathBuilder.h>
#include <mine/paint/DrawCmd.h>
#include <mine/paint/DisplayList.h>
#include <mine/paint/Canvas.h>
#include <mine/paint/IRenderer.h>
```

**描述**：伞状头文件，包含所有其他头文件。

**特征**：
- 一次性引入整个模块的所有公开类型
- 适用于用户代码（应用程序/控件代码）
- 不适用于头文件（会增加编译依赖）

---

## PaintFwd.h（前向声明头文件）

### 文件内容

```cpp
/**
 * @file PaintFwd.h
 * @brief mine.paint 模块前向声明。
 */

#pragma once

namespace mine::paint {

// ── 绘制属性 ────────────────────────────────────────────────────────

class  Brush;         ///< 填充画刷（纯色、渐变等）
struct Pen;           ///< 描边样式（线宽、线连接、线端点）

// ── 几何路径 ────────────────────────────────────────────────────────

class  Path;          ///< 不可变几何路径
class  PathBuilder;   ///< 路径命令构造器

// ── 绘制命令 ────────────────────────────────────────────────────────

class  DisplayList;   ///< 不可变绘制命令序列
class  Canvas;        ///< 绘制上下文（录制模式）

// ── 渲染器 ──────────────────────────────────────────────────────────

class IRenderer;      ///< 渲染后端接口（实现由 mine.paint.d3d11 等提供）

} // namespace mine::paint
```

**描述**：前向声明头文件，仅声明类型名称。

**特征**：
- 仅声明类型名称，不包含完整定义
- 适用于头文件（减少编译依赖）
- 不适用于需要访问类成员的代码

---

## 使用场景

### 1. 应用程序代码：使用 Paint.h

```cpp
// main.cpp
#include <mine/paint/Paint.h>  // 引入整个模块

using namespace mine::paint;

int main() {
    Canvas canvas;
    canvas.fill_rect({10, 10, 200, 100}, Brush::solid_rgb(0xFF0000));
    DisplayList dl = canvas.end();
    
    auto renderer = create_renderer(device);
    renderer->render(dl, back_buffer);
    
    return 0;
}
```

---

### 2. 头文件：使用 PaintFwd.h

```cpp
// MyWidget.h
#pragma once

#include <mine/paint/PaintFwd.h>  // 前向声明，减少编译依赖

namespace my_app {

class MyWidget {
public:
    void render(mine::paint::Canvas& canvas);
    
private:
    mine::paint::DisplayList cached_dl_;  // 可以声明成员变量
};

} // namespace my_app
```

```cpp
// MyWidget.cpp
#include "MyWidget.h"
#include <mine/paint/Paint.h>  // 完整定义，在 .cpp 中引入

namespace my_app {

void MyWidget::render(mine::paint::Canvas& canvas) {
    canvas.fill_rect({0, 0, 100, 100}, mine::paint::Brush::solid_rgb(0xFF0000));
}

} // namespace my_app
```

---

### 3. 指针/引用参数：使用 PaintFwd.h

```cpp
// Renderer.h
#pragma once

#include <mine/paint/PaintFwd.h>  // 前向声明

namespace my_app {

class Renderer {
public:
    void render(const mine::paint::DisplayList& dl);
    void set_canvas(mine::paint::Canvas* canvas);
    
private:
    mine::paint::IRenderer* renderer_;  // 指针，前向声明即可
};

} // namespace my_app
```

---

## 最佳实践

### 1. 头文件使用 PaintFwd.h

```cpp
// ✅ 推荐：头文件使用 PaintFwd.h
// MyWidget.h
#pragma once
#include <mine/paint/PaintFwd.h>

class MyWidget {
public:
    void render(mine::paint::Canvas& canvas);
};

// ❌ 不推荐：头文件使用 Paint.h
// MyWidget.h
#pragma once
#include <mine/paint/Paint.h>  // 增加编译依赖

class MyWidget {
public:
    void render(mine::paint::Canvas& canvas);
};
```

---

### 2. 源文件使用 Paint.h

```cpp
// ✅ 推荐：源文件使用 Paint.h
// MyWidget.cpp
#include "MyWidget.h"
#include <mine/paint/Paint.h>

void MyWidget::render(mine::paint::Canvas& canvas) {
    canvas.fill_rect({0, 0, 100, 100}, mine::paint::Brush::solid_rgb(0xFF0000));
}

// ❌ 不推荐：源文件也使用 PaintFwd.h
// MyWidget.cpp
#include "MyWidget.h"
#include <mine/paint/PaintFwd.h>  // 缺少完整定义，编译错误

void MyWidget::render(mine::paint::Canvas& canvas) {
    canvas.fill_rect({0, 0, 100, 100}, mine::paint::Brush::solid_rgb(0xFF0000));  // 编译错误
}
```

---

### 3. 按需引入单个头文件

```cpp
// ✅ 推荐：按需引入单个头文件
// MyWidget.cpp
#include "MyWidget.h"
#include <mine/paint/Canvas.h>
#include <mine/paint/Brush.h>

void MyWidget::render(mine::paint::Canvas& canvas) {
    canvas.fill_rect({0, 0, 100, 100}, mine::paint::Brush::solid_rgb(0xFF0000));
}

// ✅ 也可以：使用 Paint.h 一次性引入
// MyWidget.cpp
#include "MyWidget.h"
#include <mine/paint/Paint.h>

void MyWidget::render(mine::paint::Canvas& canvas) {
    canvas.fill_rect({0, 0, 100, 100}, mine::paint::Brush::solid_rgb(0xFF0000));
}
```

---

## 常见陷阱

### 1. 头文件使用 Paint.h 导致编译依赖增加

```cpp
// ❌ 问题：头文件使用 Paint.h
// MyWidget.h
#pragma once
#include <mine/paint/Paint.h>  // 增加编译依赖

class MyWidget {
public:
    void render(mine::paint::Canvas& canvas);
};

// ✅ 解决：使用 PaintFwd.h
// MyWidget.h
#pragma once
#include <mine/paint/PaintFwd.h>  // 减少编译依赖

class MyWidget {
public:
    void render(mine::paint::Canvas& canvas);
};
```

---

### 2. 源文件使用 PaintFwd.h 导致编译错误

```cpp
// ❌ 问题：源文件使用 PaintFwd.h
// MyWidget.cpp
#include "MyWidget.h"
#include <mine/paint/PaintFwd.h>  // 缺少完整定义

void MyWidget::render(mine::paint::Canvas& canvas) {
    canvas.fill_rect({0, 0, 100, 100}, mine::paint::Brush::solid_rgb(0xFF0000));  // 编译错误
}

// ✅ 解决：使用 Paint.h 或单个头文件
// MyWidget.cpp
#include "MyWidget.h"
#include <mine/paint/Paint.h>  // 完整定义

void MyWidget::render(mine::paint::Canvas& canvas) {
    canvas.fill_rect({0, 0, 100, 100}, mine::paint::Brush::solid_rgb(0xFF0000));
}
```

---

### 3. 忘记引入头文件

```cpp
// ❌ 问题：忘记引入头文件
// MyWidget.cpp
#include "MyWidget.h"
// 缺少 #include <mine/paint/Paint.h>

void MyWidget::render(mine::paint::Canvas& canvas) {
    canvas.fill_rect({0, 0, 100, 100}, mine::paint::Brush::solid_rgb(0xFF0000));  // 编译错误
}

// ✅ 解决：引入头文件
// MyWidget.cpp
#include "MyWidget.h"
#include <mine/paint/Paint.h>

void MyWidget::render(mine::paint::Canvas& canvas) {
    canvas.fill_rect({0, 0, 100, 100}, mine::paint::Brush::solid_rgb(0xFF0000));
}
```

---

## 完整示例

### 示例 1：应用程序代码

```cpp
// main.cpp
#include <mine/paint/Paint.h>  // 引入整个模块
#include <mine/gfx/rhi/Device.h>

using namespace mine::paint;
using namespace mine::gfx::rhi;

int main() {
    // 创建 RHI 设备
    auto device = create_device();
    
    // 创建渲染器
    auto renderer = create_renderer(device.get());
    renderer->set_dpi_scale(1.0f);
    
    // 创建后台缓冲
    auto back_buffer = device->create_back_buffer({800, 600}, TextureFormat::RGBA8);
    
    // 绘制
    Canvas canvas;
    canvas.fill_rect({10, 10, 200, 100}, Brush::solid_rgb(0xFF0000));
    DisplayList dl = canvas.end();
    
    // 渲染
    renderer->begin_frame();
    renderer->render(dl, back_buffer.get());
    renderer->end_frame();
    
    // 显示
    device->present();
    
    return 0;
}
```

---

### 示例 2：头文件 + 源文件

```cpp
// MyWidget.h
#pragma once

#include <mine/paint/PaintFwd.h>  // 前向声明，减少编译依赖

namespace my_app {

class MyWidget {
public:
    void render(mine::paint::Canvas& canvas);
    
private:
    mine::paint::DisplayList cached_dl_;  // 可以声明成员变量
};

} // namespace my_app
```

```cpp
// MyWidget.cpp
#include "MyWidget.h"
#include <mine/paint/Paint.h>  // 完整定义，在 .cpp 中引入

namespace my_app {

void MyWidget::render(mine::paint::Canvas& canvas) {
    // 使用缓存的 DisplayList
    if (cached_dl_.empty()) {
        Canvas temp_canvas;
        temp_canvas.fill_rect({0, 0, 100, 100}, mine::paint::Brush::solid_rgb(0xFF0000));
        cached_dl_ = temp_canvas.end();
    }
    
    // 重放缓存的 DisplayList
    for (const auto& cmd : cached_dl_.cmds()) {
        // 处理命令
    }
}

} // namespace my_app
```

---

## 总结

`Paint` 和 `PaintFwd` 是 `mine.paint` 模块的伞状头文件和前向声明头文件，具备：

1. **Paint.h**：伞状头文件，包含所有其他头文件（一次性引入整个模块）
2. **PaintFwd.h**：前向声明头文件，仅声明类型名称（减少编译依赖）

**使用建议**：
- 头文件使用 PaintFwd.h（减少编译依赖）
- 源文件使用 Paint.h（完整定义）
- 按需引入单个头文件（可选）
- 应用程序代码直接使用 Paint.h
