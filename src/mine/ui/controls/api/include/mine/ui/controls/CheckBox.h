#pragma once
#include <mine/ui/controls/Api.h>
#include <mine/ui/visual/Control.h>
#include <mine/ui/event/RoutedEvent.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/paint/Brush.h>
#include <mine/math/Rect.h>

namespace mine::ui {

class MINE_UI_CONTROLS_API CheckBox : public Control {
public:
    static const RoutedEvent& CheckedChangedEvent();
    static const DependencyProperty& IsCheckedProperty;

    CheckBox();
    ~CheckBox() override;

    [[nodiscard]] bool is_checked() const noexcept;
    void set_checked(bool checked);

    [[nodiscard]] core::StringView text() const noexcept;
    void set_text(core::StringView text);

    void set_font_face(void* font_face) noexcept;
    void set_font_size(float size_px) noexcept;

    static void on_mouse_enter_router(void*, RoutedEventArgs&, void*);
    static void on_mouse_leave_router(void*, RoutedEventArgs&, void*);
    static void on_mouse_down_router(void*, RoutedEventArgs&, void*);
    static void on_mouse_up_router  (void*, RoutedEventArgs&, void*);

protected:
    void on_measure(math::Size available) override;
    void on_render(paint::Canvas& canvas) override;
    [[nodiscard]] ControlVisualState compute_visual_state() const noexcept override;

private:
    containers::InlineString text_;
    bool is_checked_  = false;
    bool is_hovered_  = false;
    bool is_pressed_  = false;
    void* font_face_  = nullptr;
    float font_size_  = 14.0f;
};

}
