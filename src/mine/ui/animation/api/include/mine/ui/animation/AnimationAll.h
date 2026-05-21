/**
 * @file AnimationAll.h
 * @brief mine.ui.animation 模块伞形头文件。
 *
 * 包含此文件即可访问动画模块的所有公开 API。
 *
 * 当前包含（Task #16，缓动函数库）：
 *   - Duration        时间段描述
 *   - EasingFunction  缓动函数库（Linear / Quad / Cubic / Quart / Quint /
 *                     Expo / Sine / Back / Elastic / Bounce + CubicBezier）
 *   - SpringEasing    弹簧物理缓动模拟器
 *
 * 未来版本（Task #17）将添加：
 *   - Storyboard      属性动画轨道与时间线管理
 *   - Timeline        动画时间轴
 */

#pragma once

#include <mine/ui/animation/Api.h>
#include <mine/ui/animation/Duration.h>
#include <mine/ui/animation/EasingFunction.h>
#include <mine/ui/animation/SpringEasing.h>
