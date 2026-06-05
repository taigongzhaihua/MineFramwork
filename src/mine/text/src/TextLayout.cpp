/**
 * @file TextLayout.cpp
 * @brief 文本行布局实现。
 */

#include <mine/text/TextLayout.h>

namespace mine::text {

containers::Vector<LineInfo> split_lines(
    const char*    text,
    size_t         text_len,
    float          max_width,
    LineMeasureFn  measure,
    void*          measure_ctx,
    bool           enable_width_wrap)
{
    containers::Vector<LineInfo> lines;

    const bool use_width_limit = enable_width_wrap
                              && (max_width > 0.0f)
                              && (max_width < 1.0e9f);

    // 字符级折叠辅助 lambda：将段落 [seg_start, seg_end) 按字符边界折叠
    auto append_char_wrap = [&](uint32_t seg_start, uint32_t seg_end) {
        if (seg_start == seg_end) {
            lines.push_back({seg_start, 0u, 0.0f});
            return;
        }
        uint32_t pos = seg_start;
        while (pos < seg_end) {
            uint32_t line_end = pos;
            float    accum_w  = 0.0f;

            while (line_end < seg_end) {
                const uint32_t next_b = utf8_next(text, line_end, seg_end);
                const float    cw     = measure(measure_ctx, text + line_end, next_b - line_end);
                if (line_end > pos && accum_w + cw > max_width) break;
                accum_w  += cw;
                line_end  = next_b;
            }
            // 保证至少放入一个字符（防死循环）
            if (line_end == pos) {
                line_end = utf8_next(text, pos, seg_end);
                accum_w  = measure(measure_ctx, text + pos, line_end - pos);
            }

            lines.push_back({pos, line_end - pos, accum_w});
            pos = line_end;
        }
    };

    // 按 '\n' 将文本分割为段落，逐段构建行
    uint32_t seg_start   = 0u;
    bool     last_was_nl = false;

    const auto total = static_cast<uint32_t>(text_len);
    for (uint32_t i = 0u; i <= total; ++i) {
        const bool is_nl  = (i < total && text[i] == '\n');
        const bool is_end = (i == total);
        if (!is_nl && !is_end) continue;

        const uint32_t seg_end = i;
        last_was_nl = is_nl;

        if (!use_width_limit) {
            const float w = measure(measure_ctx, text + seg_start, seg_end - seg_start);
            lines.push_back({seg_start, seg_end - seg_start, w});
        } else {
            append_char_wrap(seg_start, seg_end);
        }

        seg_start = i + 1u;
    }

    // 文本以 '\n' 结尾时追加空行
    if (last_was_nl) {
        lines.push_back({total, 0u, 0.0f});
    }

    // 保证至少一行
    if (lines.empty()) {
        lines.push_back({0u, 0u, 0.0f});
    }

    return lines;
}

bool find_line_by_offset(
    const containers::Vector<LineInfo>& lines,
    uint32_t                            offset,
    uint32_t*                           out_line_idx,
    uint32_t*                           out_line_offset)
{
    for (uint32_t i = 0; i < static_cast<uint32_t>(lines.size()); ++i) {
        const uint32_t line_start = lines[i].start_offset;
        const uint32_t line_end   = line_start + lines[i].byte_length;
        if (offset >= line_start && offset <= line_end) {
            *out_line_idx    = i;
            *out_line_offset = offset - line_start;
            return true;
        }
    }
    return false;
}

}  // namespace mine::text
