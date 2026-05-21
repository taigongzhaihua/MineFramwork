/**
 * @file EasingFunction.cpp
 * @brief CubicBezier 缓动曲线求解实现。
 *
 * CubicBezier::solve() 使用牛顿迭代法求解三次贝塞尔曲线的逆映射，
 * 即给定 x 分量的值（进度 t），通过迭代找到参数 u 使得 Bx(u) ≈ t，
 * 再求 By(u) 作为缓动后的输出值。
 */

#include <mine/ui/animation/EasingFunction.h>
#include <cmath>

namespace mine::ui::animation {

// ── CubicBezier 静态常量定义 ────────────────────────────────────────────────

// CSS 标准缓动曲线预置
const CubicBezier CubicBezier::Ease      { 0.25f, 0.1f,  0.25f, 1.0f  };
const CubicBezier CubicBezier::EaseIn    { 0.42f, 0.0f,  1.0f,  1.0f  };
const CubicBezier CubicBezier::EaseOut   { 0.0f,  0.0f,  0.58f, 1.0f  };
const CubicBezier CubicBezier::EaseInOut { 0.42f, 0.0f,  0.58f, 1.0f  };

// ── 内部辅助：三次贝塞尔计算 ────────────────────────────────────────────────

/**
 * @brief 计算三次贝塞尔曲线的 x 分量。
 *
 * 曲线的 x 分量由控制点 (0, x1, x2, 1) 决定：
 *   Bx(u) = 3(1-u)²u·x1 + 3(1-u)u²·x2 + u³
 */
float CubicBezier::bezier_x(float u) const noexcept
{
    const float inv_u = 1.0f - u;
    // 展开三次伯恩斯坦基函数（P0=0, P3=1 时的化简式）
    return 3.0f * inv_u * inv_u * u * x1_
         + 3.0f * inv_u * u * u  * x2_
         + u * u * u;
}

/**
 * @brief 计算三次贝塞尔曲线的 y 分量。
 *
 * 曲线的 y 分量由控制点 (0, y1, y2, 1) 决定：
 *   By(u) = 3(1-u)²u·y1 + 3(1-u)u²·y2 + u³
 */
float CubicBezier::bezier_y(float u) const noexcept
{
    const float inv_u = 1.0f - u;
    return 3.0f * inv_u * inv_u * u * y1_
         + 3.0f * inv_u * u * u  * y2_
         + u * u * u;
}

/**
 * @brief 计算 x 分量对参数 u 的导数。
 *
 * dBx/du = 3(1-u)²·x1 + 6(1-u)u·(x2-x1) + 3u²·(1-x2)
 *         = 3[(1-u)²·x1 + 2(1-u)u·x2 + u²] - ...（展开化简）
 *
 * 导数的标准形式（一阶差分形式）：
 *   dBx/du = 3(x1 - 2x1·u + x1·u² + x2·u - 2x2·u² + x2·u² ... ）
 * 实际使用等效的直接形式：
 *   dBx/du = 3[(1-u)²·x1 + 2(1-u)u·x2 + u²·1 - (1-u)²·0]
 * 化简后：
 *   dBx/du = 3[x1 + (2x2 - 4x1)u + (3x1 - 3x2 + 1)u²]
 */
float CubicBezier::bezier_x_derivative(float u) const noexcept
{
    const float inv_u = 1.0f - u;
    // 二次贝塞尔导数 = 3 * (B(P0,P1,P2)(u) 的一阶贝塞尔值)
    // P0=0, P1=x1, P2=x2, P3=1：导数 = 3*(差分贝塞尔)
    return 3.0f * (inv_u * inv_u * x1_
                 + 2.0f * inv_u * u * x2_
                 + u * u * 1.0f)
           - 3.0f * (inv_u * inv_u * 0.0f
                   + 2.0f * inv_u * u * x1_
                   + u * u * x2_);
    // 等价化简后：
    // = 3 * [x1(1-u)^2 + 2*x2*u(1-u) + u^2 - 2*x1*u(1-u) - x2*u^2]
    // = 3 * [x1*(1-u)^2 + 2*(x2-x1)*u*(1-u) + (1-x2)*u^2]
}

// ── CubicBezier::solve() ────────────────────────────────────────────────────

/**
 * @brief 求解给定 x 值对应的 y 值（缓动函数的主接口）。
 *
 * 算法：
 *   1. 初始猜测 u = t（利用 x 参数与 u 接近的特性）
 *   2. 牛顿迭代最多 8 次，精度达到 1e-6 时提前退出
 *   3. 对边界值（t=0 或 t=1）直接返回，跳过迭代
 *
 * 时间复杂度：O(1)（常数次迭代）
 */
float CubicBezier::solve(float t) const noexcept
{
    // 边界值直接返回（同时覆盖超出 [0,1] 的情况）
    if (t <= 0.0f) { return 0.0f; }
    if (t >= 1.0f) { return 1.0f; }

    // 初始猜测：令参数 u = t（当 x1≈x2 时精度高）
    float u = t;

    // 牛顿迭代求解 Bx(u) = t
    constexpr int   kMaxIter  = 8;
    constexpr float kEpsilon  = 1.0e-6f;

    for (int i = 0; i < kMaxIter; ++i) {
        const float x    = bezier_x(u) - t;
        const float dxdu = bezier_x_derivative(u);

        // 避免除以极小值（导数接近 0 时停止迭代）
        if (std::fabs(dxdu) < 1.0e-9f) {
            break;
        }

        u -= x / dxdu;  // 牛顿步

        // 收敛判定
        if (std::fabs(x) < kEpsilon) {
            break;
        }
    }

    // 将迭代结果限制到合法范围（浮点精度可能微小越界）
    if (u < 0.0f) { u = 0.0f; }
    if (u > 1.0f) { u = 1.0f; }

    return bezier_y(u);
}

} // namespace mine::ui::animation
