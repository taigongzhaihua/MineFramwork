/**
 * @file PaintFwd.h
 * @brief mine.paint 模块前向声明。
 *
 * 仅包含此头文件即可引用 paint 类型而无需完整定义，
 * 避免头文件互相循环包含。
 */

#pragma once

namespace mine::paint {

// ── 值类型（完整定义在各自头文件中）─────────────────────────────────────────

class  Brush;         ///< 填充画刷（纯色、渐变等）
struct Pen;           ///< 描边样式（线宽、连接、端点）
class  Path;          ///< 不可变几何路径
class  PathBuilder;   ///< 路径命令构造器
struct DrawCmd;       ///< 单条绘制命令（带 kind 标签的结构体）
class  DisplayList;   ///< 不可变绘制命令序列
class  Canvas;        ///< 绘制上下文（录制模式）

// ── 接口（纯虚，由后端库实现）───────────────────────────────────────────────

class IRenderer;      ///< 渲染后端接口（实现由 mine.paint.d3d11 等提供）

} // namespace mine::paint
