/**
 * @file Paint.h
 * @brief mine.paint 模块伞形头文件。
 *
 * 一次 include 引入全部 2D 绘制 API。
 *
 * @code
 *   #include <mine/paint/Paint.h>
 *
 *   mine::paint::Canvas  canvas;
 *   mine::paint::Brush   brush = mine::paint::Brush::solid_rgb(0x4096FF);
 *   canvas.fill_rounded_rect({{20, 20, 300, 180}, 12.f}, brush);
 *   mine::paint::DisplayList dl = canvas.end();
 * @endcode
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
