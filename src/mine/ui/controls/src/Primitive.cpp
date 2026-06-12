/**
 * @file Primitive.cpp
 * @brief Primitive 基元控件基类实现。
 */

#include <mine/ui/controls/Primitive.h>

namespace mine::ui {

void Primitive::invalidate_appearance()
{
    invalidate_render();
}

} // namespace mine::ui
