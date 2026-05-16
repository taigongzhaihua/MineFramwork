# 06 — 合成、文本与图像

## 6.1 视觉树与合成树

UI 树（逻辑，由控件构成）与合成树（视觉，由 `Visual` / `Layer` 构成）**分离**：

```
UIElement (mine.ui.visual)
    └─ owns ──► Visual (mine.compose)
                    └─ Layer / Surface / Effect
```

合成树：

* `Visual` 持有变换、不透明度、裁剪、合成模式、子节点。
* 离屏 `Layer`：当节点开启透明度、变换、滤镜、缓存时下沉为独立纹理。
* 脏区追踪：每节点维护本地脏标志 + 边界，合成阶段聚合最小重绘区域。
* `compositor` 将合成树转为 `DisplayList`（不可变中间表示），交给 `mine.paint::IRenderer` 执行。

## 6.2 DisplayList

不可变、可序列化的绘制命令序列：

```cpp
struct DrawCmd {
    enum Kind { Rect, RoundedRect, Path, Text, Image, Layer, ClipPush, ClipPop, ... } kind;
    /* payload union */
};
```

* 支持**跨线程**搬运。
* 可被复用（同一帧在两台显示器投影）。
* 是合成器与渲染器的稳定边界。

## 6.3 文本

```
TextBlock / TextBox  (UI 控件)
       │
       ▼
TextLayout (text.layout)        ◄── 段落、对齐、缩进、列表、富文本
       │
       ▼
TextShaper (text.shape)         ◄── HarfBuzz 整形 → Glyph runs
       │
       ▼
GlyphAtlas / SDF Renderer (paint)
```

* 支持 BiDi、字距调整、连字、上下标、emoji、组合字符。
* 富文本块以**结构化 inline**（Run、Span）描述，不依赖 HTML。
* 行高、行距、行包装策略可配置。
* 选区、Hit-test API 暴露在 `TextLayout`。

## 6.4 图像

`mine.image`：

* 抽象 `Decoder` 接口，PNG / JPG / WebP / BMP / GIF / ICO 各自插件。
* `BitmapCache`：按 (URI, target size, dpi) 缓存解码结果。
* 图像源（`ImageSource`）支持：
  * 文件路径
  * 内存 buffer
  * 资源包（VFS）
  * 网络 URL（异步加载，落到 `mine.net.http`）
  * 自定义 `IImageProvider`

## 6.5 SVG（可选）

`mine.svg`：

* 解析子集（不含脚本/动画/滤镜的高级特性，足以支持图标级用例）。
* 转换为 `mine.paint::Path` + `Brush`，支持光栅化与矢量缩放。

## 6.6 合成性能要点

| 优化 | 触发条件 |
|------|----------|
| Layer 缓存 | 节点开启 `cache: true` 或满足启发（透明度变换、动画、视频等） |
| 脏区合并 | 同帧多次 invalidate 取并集 |
| 批合并 | DisplayList 中相邻同 pipeline 的命令融合 |
| 后向遮挡剔除 | 完全不透明覆盖的节点跳过 |
| 滚动复用 | `ScrollViewer` 使用纹理平移 + 增量重绘 |

## 6.7 自定义合成扩展

* 用户可实现 `IVisualEffect`（着色器+参数）并挂在 `Visual` 上。
* 用户可实现 `IRenderer` 接管整条 paint 管线（例如转 Skia）。
* 用户可挂 `IFrameObserver`（拿到 DisplayList，在外部做镜像/截图/录屏）。
