/**
 * @file Utf8.h
 * @brief UTF-8 编解码与光标移动工具函数。
 *
 * 提供独立于平台的 UTF-8 字符边界遍历、UTF-32↔UTF-8 转换，
 * 供 UI 控件（TextBox 等）和其他需要处理 UTF-8 文本的模块使用。
 */

#pragma once

#include <cstdint>

namespace mine::text {

/**
 * @brief 向前移动一个 UTF-8 字符，返回下一个字符起始的字节偏移。
 *
 * @param data UTF-8 字节序列
 * @param pos  当前字符起始字节偏移
 * @param end  字节序列末尾偏移（不含）
 * @return     下一个字符起始偏移；pos 已在末尾时返回 end
 */
inline uint32_t utf8_next(const char* data, uint32_t pos, uint32_t end) noexcept
{
    if (pos >= end) {
        return end;
    }
    const auto c = static_cast<uint8_t>(data[pos]);
    uint32_t skip = 1u;
    if      ((c & 0xF8u) == 0xF0u) { skip = 4u; }
    else if ((c & 0xF0u) == 0xE0u) { skip = 3u; }
    else if ((c & 0xE0u) == 0xC0u) { skip = 2u; }
    const uint32_t next = pos + skip;
    return next <= end ? next : end;
}

/**
 * @brief 向后移动一个 UTF-8 字符，返回前一个字符起始的字节偏移。
 *
 * 从 pos 向前查找，跳过连续字节（0x80–0xBF）直到找到前导字节。
 *
 * @param data UTF-8 字节序列
 * @param pos  当前位置（通常为光标位置）
 * @return     前一个字符起始偏移；pos 为 0 时返回 0
 */
inline uint32_t utf8_prev(const char* data, uint32_t pos) noexcept
{
    if (pos == 0u) {
        return 0u;
    }
    uint32_t p = pos - 1u;
    while (p > 0u && (static_cast<uint8_t>(data[p]) & 0xC0u) == 0x80u) {
        --p;
    }
    return p;
}

/**
 * @brief 将 UTF-32 码点编码为 UTF-8 字节序列。
 *
 * @param cp  Unicode 码点（≤ 0x10FFFF）
 * @param out 输出缓冲区（至少 4 字节）
 * @return    写入的字节数（1–4）
 */
inline uint32_t utf32_to_utf8(uint32_t cp, char out[4]) noexcept
{
    if (cp < 0x80u) {
        out[0] = static_cast<char>(cp);
        return 1u;
    }
    if (cp < 0x800u) {
        out[0] = static_cast<char>(0xC0u | (cp >> 6));
        out[1] = static_cast<char>(0x80u | (cp & 0x3Fu));
        return 2u;
    }
    if (cp < 0x10000u) {
        out[0] = static_cast<char>(0xE0u | (cp >> 12));
        out[1] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = static_cast<char>(0x80u | (cp & 0x3Fu));
        return 3u;
    }
    out[0] = static_cast<char>(0xF0u | (cp >> 18));
    out[1] = static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu));
    out[2] = static_cast<char>(0x80u | ((cp >> 6)  & 0x3Fu));
    out[3] = static_cast<char>(0x80u | (cp         & 0x3Fu));
    return 4u;
}

/**
 * @brief 统计 UTF-8 字节序列中的完整字符数。
 *
 * @param p   字节序列起始指针
 * @param len 字节长度
 * @return    完整字符数（截断的不完整尾字节不计入）
 */
inline uint32_t count_utf8_chars(const char* p, uint32_t len) noexcept
{
    const char* end   = p + len;
    uint32_t    count = 0;
    while (p < end) {
        const auto c    = static_cast<uint8_t>(*p);
        uint32_t   step = (c < 0x80u) ? 1u
                        : (c < 0xE0u) ? 2u
                        : (c < 0xF0u) ? 3u
                        :               4u;
        if (static_cast<uint32_t>(end - p) < step) break;
        p += step;
        ++count;
    }
    return count;
}

}  // namespace mine::text
