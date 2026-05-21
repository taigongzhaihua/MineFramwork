/**
 * @file AnimationAll.h
 * @brief mine.ui.animation 模块伞形头文件。
 *
 * 包含此文件即可访问动画模块的所有公开 API。
 *
 * Task #16 （缓动函数库）：
 *   - Duration        时间段描述
 *   - EasingFunction  缓动函数库（Linear / Quad / Cubic / Quart / Quint /
 *                     Expo / Sine / Back / Elastic / Bounce + CubicBezier）
 *   - SpringEasing    弹簧物理缓动模拟器
 *
 * Task #17 （Storyboard + 属性动画）：
 *   - PropertyAnimation 单个依赖属性动画描述结构体
 *   - Storyboard        属性动画时间线容器（与 VisualStateManager 集成）
 */

#pragma once

#include <mine/ui/animation/Api.h>
#include <mine/ui/animation/Duration.h>
#include <mine/ui/animation/EasingFunction.h>
#include <mine/ui/animation/SpringEasing.h>
#include <mine/ui/animation/PropertyAnimation.h>
#include <mine/ui/animation/Storyboard.h>
