/**
 * @file WindowChromeDesc.h
 * @brief 自定义窗口标题栏（Chrome）配置描述符。
 *
 * 通过 IWindow::set_chrome() 将此描述符传递给平台实现层，
 * 平台层负责调用相应的系统 API（DWM / Compositor 等）完成配置。
 * 所有尺寸字段均为逻辑像素（96 DPI 基准），由平台层自行换算为物理像素。
 */

#pragma once

#include <mine/math/Thickness.h>
#include <mine/platform/WindowCornerPreference.h>

namespace mine::platform {

/**
 * @brief 自定义窗口 Chrome 配置描述符。
 *
 * 设计对标 WPF WindowChrome，并做了以下优化：
 *   1. 使用 enabled 字段统一开关，无需为"禁用状态"维护特殊值
 *   2. 将 WPF 四个独立属性合并为单一结构体，通过 IWindow::set_chrome() 一次性提交，
 *      避免平台层收到多次零散更新
 *   3. glass_frame_thickness 默认为零厚度（不延伸），而非负值"全窗口"模式，
 *      降低误操作风险；需全窗口玻璃效果时显式传 Thickness::uniform(-1.0f)
 */
struct WindowChromeDesc {
    /// 是否启用自定义 Chrome（false 时所有字段均被忽略，恢复系统默认标题栏行为）
    bool enabled{false};

    /**
     * @brief 可拖拽标题栏区域高度（逻辑像素）。
     *
     * 窗口顶部该高度范围内的区域将被识别为标题栏（HTCAPTION），
     * 用户可在此处拖拽移动窗口、双击最大化/还原。
     * 设为 0 时表示不提供任何可拖拽区域（应用完全接管命中测试）。
     */
    float caption_height{32.0f};

    /**
     * @brief 可调整大小的边框厚度（逻辑像素，四边各自独立）。
     *
     * 窗口四边各自 resize_border_thickness 宽度内的区域将被识别为 resize 边缘
     * （HTLEFT / HTTOP / HTRIGHT / HTBOTTOM 及四角）。
     * 当窗口最大化或 resizable = false 时，此设置自动失效。
     */
    math::Thickness resize_border_thickness{math::Thickness::uniform(4.0f)};

    /**
     * @brief 窗口圆角首选项。
     *
     * 由平台层映射到对应的系统 API（Windows 11: DWM_WINDOW_CORNER_PREFERENCE）。
     * 在不支持圆角首选项的旧系统上（Windows 10 及以下），此字段被忽略。
     */
    WindowCornerPreference corner_preference{WindowCornerPreference::SystemDefault};

    /**
     * @brief DWM 玻璃帧延伸厚度（逻辑像素）。
     *
     * 将 DWM 玻璃帧（毛玻璃半透明效果）向客户区延伸指定厚度。
     *   - 全为 0：不延伸（默认），客户区无玻璃效果
     *   - 正值：向客户区延伸对应宽度的玻璃帧
     *   - Thickness::uniform(-1.0f)：延伸覆盖整个客户区（全窗口玻璃效果）
     *
     * 仅在 DWM 组合（桌面合成）开启时生效；Windows 8 以上默认开启。
     */
    math::Thickness glass_frame_thickness{};
};

} // namespace mine::platform
