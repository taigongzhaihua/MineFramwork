/**
 * @file TextLayout.h
 * @brief 文本行布局（折行、行分割）工具函数。
 *
 * 提供与字体无关的 UTF-8 文本行分割逻辑，供 TextBox、TextBlock 等控件复用。
 * 调用方通过测量回调注入各自的宽度计算（如含字符间距 / 不含）。
 */

#pragma once

#include <mine/containers/Vector.h>
#include <mine/text/Utf8.h>

#include <cstdint>

namespace mine::text {

/**
 * @brief 文本行信息。
 */
struct LineInfo {
    uint32_t start_offset = 0;  ///< 行起始字节偏移（相对输入文本开头）
    uint32_t byte_length  = 0;  ///< 行字节长度（不含换行符）
    float    disp_width   = 0.0f; ///< 行显示宽度（像素）
};

/**
 * @brief 文本宽度测量回调类型。
 *
 * @param ctx  调用方上下文指针（如控件 this）
 * @param data UTF-8 字节序列
 * @param len  字节长度
 * @return     宽度（像素）
 */
using LineMeasureFn = float (*)(void* ctx, const char* data, uint32_t len);

/**
 * @brief 将 UTF-8 文本按换行符和可用宽度分割为多行。
 *
 * 先按 \n 分割段落，再对每个段落按字符边界折叠（符合 Unicode 字形簇安全断行）。
 * 当 enable_width_wrap=false 时，每个段落整段作为一行（NoWrap 行为）。
 *
 * @param text              输入文本（UTF-8）
 * @param text_len          文本字节长度
 * @param max_width         最大行宽（像素），enable_width_wrap=false 时忽略
 * @param measure           宽度测量回调
 * @param measure_ctx       回调上下文指针
 * @param enable_width_wrap 是否按宽度自动折叠（等价 TextWrapping != NoWrap）
 * @return                  行信息数组（至少一行，空文本返回一个空行）
 */
containers::Vector<LineInfo> split_lines(
    const char*    text,
    size_t         text_len,
    float          max_width,
    LineMeasureFn  measure,
    void*          measure_ctx,
    bool           enable_width_wrap);

/**
 * @brief 根据字节偏移查找所在行号和行内偏移。
 *
 * @param lines           行数组
 * @param offset          目标字节偏移
 * @param out_line_idx    输出：所在行号
 * @param out_line_offset 输出：行内字节偏移
 * @return                true=找到，false=偏移越界
 */
bool find_line_by_offset(
    const containers::Vector<LineInfo>& lines,
    uint32_t                            offset,
    uint32_t*                           out_line_idx,
    uint32_t*                           out_line_offset);

}  // namespace mine::text
