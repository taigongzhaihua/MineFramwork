/**
 * @file Math.h
 * @brief mine.math 模块伞形头文件。
 *
 * 包含此头文件即可使用 mine.math 模块的全部基础 API：
 *   - Vec2 / Vec3 / Vec4       — 向量
 *   - Mat3 / Mat4              — 矩阵
 *   - Point / Size / Rect      — 二维几何基础类型
 *   - Thickness               — 四边各自独立的厚度（left/top/right/bottom）
 *   - RoundedRect              — 圆角矩形（全局统一圆角）
 *   - CornerRadii              — 四角各自独立的橢圆圆角半径
 *   - ComplexRoundedRect       — 四角各自独立的圆角矩形，inner_rect 支持边框内层推导
 *   - Color                    — 线性空间颜色
 *   - Transform2D              — 二维仿射变换
 */

#pragma once

#include <mine/math/Api.h>
#include <mine/math/ModuleTag.h>
#include <mine/math/Common.h>
#include <mine/math/Vec2.h>
#include <mine/math/Vec3.h>
#include <mine/math/Vec4.h>
#include <mine/math/Point.h>
#include <mine/math/Size.h>
#include <mine/math/Rect.h>
#include <mine/math/Thickness.h>
#include <mine/math/RoundedRect.h>
#include <mine/math/CornerRadii.h>
#include <mine/math/ComplexRoundedRect.h>
#include <mine/math/Mat3.h>
#include <mine/math/Mat4.h>
#include <mine/math/Color.h>
#include <mine/math/Transform2D.h>