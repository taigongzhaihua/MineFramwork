// ps_sdf.hlsl 鈥?浠?src/mine/paint/src/RhiRenderer.cpp 鑷姩鎻愬彇
// 鍘熷鍙橀噺鍚嶏細k_sdf_pixel_shader_hlsl
// 鐢熸垚鏃堕棿锛?(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
// 姝ゆ枃浠剁敱 scripts/extract_hlsl.ps1 鐢熸垚锛岃鍕挎墜鍔ㄧ紪杈戙€?
// 淇敼鐫€鑹插櫒璇风紪杈?RhiRenderer.cpp 鍚庨噸鏂拌繍琛屾彁鍙栬剼鏈€?
// 视口常量缓冲（b0 槽，与顶点着色器共享）
cbuffer ViewportCB : register(b0) {
    float2 viewport_size;       // 逻辑像素尺寸（NDC 变换用）
    float2 phys_viewport_size;  // 物理像素尺寸（SV_Position → UV 用）
};

// 画刷数据常量缓冲（b2 槽，每个 DrawCall 更新一次）
// 总大小：128 字节（8 × float4，16 字节对齐）
cbuffer BrushDataCB : register(b2) {
    uint   brush_kind;       // 画刷类型：0=纯色，1=线性渐变，2=径向渐变，3=亚克力
    uint   stop_count;       // 渐变色标数量 [2..4]（brush_kind=1/2 时有效）
    float  _brush_pad0;
    float  _brush_pad1;
    //   linear:  xy=起点 local 坐标（像素，相对形状中心），zw=渐变方向向量 local
    //   radial:  xy=圆心 local 坐标（像素），z=外径（像素），w=内径（像素）
    //   acrylic: xyzw=色调颜色（rgba，线性空间）
    float4 grad_params;
    //   acrylic: x=色调混合比例 [0,1]，y=饱和度增益（1.0=原始）
    float4 grad_extra;
    float4 stop_colors[4];   // 渐变色标颜色（rgba，线性空间，最多 4 个）
    float4 stop_offsets;     // 色标偏移量（x=off0, y=off1, z=off2, w=off3）
};

// 裁剪 SDF 常量缓冲（b3 槽，每次裁剪状态变化时更新）
// 最多 4 层嵌套裁剪，每层含内联顶点数据（多边形裁剪用）
struct ClipSdfLayer {
    float4 bounds;          // (cx, cy, half_w, half_h)：裁剪形状的中心和半尺寸（逻辑像素）
    float4 kind_p0_p1_p2;  // (kind, p0, p1, p2)：形状类型及圆角参数
    float4 p3_nv_pad;       // (p3, poly_vert_count_f, _, _)：左上圆角/多边形顶点数/填充
    float4 poly_verts[64];  // 多边形顶点（xy=相对裁剪区域中心的本地坐标，zw=0）
};
cbuffer ClipSdfCB : register(b3) {
    int   clip_count;   // 当前活跃裁剪层数（0=无裁剪）
    float _cpad0;
    float _cpad1;
    float _cpad2;
    ClipSdfLayer clip_layers[4];  // 最多 4 层嵌套裁剪
};

// 亚克力模糊背景纹理（t0 槽，仅亚克力 DrawCall 绑定时有效）
Texture2D    acrylic_backdrop : register(t0);
SamplerState acrylic_sampler  : register(s0);  // 线性双线性采样器

// 根据归一化参数 t [0,1] 在色标数组中插值，返回 rgba 颜色
float4 sample_gradient(float t) {
    t = saturate(t);
    float o0 = stop_offsets.x;
    float o1 = stop_offsets.y;
    float o2 = stop_offsets.z;
    float o3 = stop_offsets.w;
    float t01 = saturate((t - o0) / max(o1 - o0, 1e-6f));
    float t12 = saturate((t - o1) / max(o2 - o1, 1e-6f));
    float t23 = saturate((t - o2) / max(o3 - o2, 1e-6f));
    float4 c01 = lerp(stop_colors[0], stop_colors[1], t01);
    float4 c12 = lerp(stop_colors[1], stop_colors[2], t12);
    float4 c23 = lerp(stop_colors[2], stop_colors[3], t23);
    float4 c = c01;
    if (stop_count >= 3u && t > o1) c = c12;
    if (stop_count >= 4u && t > o2) c = c23;
    return c;
}

// 矩形 SDF：p=本地坐标，b=半尺寸
float box_sdf(float2 p, float2 b) {
    float2 q = abs(p) - b;
    return length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f);
}

// 均匀圆角矩形 SDF：r = 统一圆角半径
float rounded_box_sdf(float2 p, float2 b, float r) {
    float2 q = abs(p) - b + r;
    return length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f) - r;
}

// 四角独立圆角矩形 SDF
// r = (右下, 右上, 左下, 左上) 各向同性圆角半径
float complex_rounded_box_sdf(float2 p, float2 b, float4 r) {
    r.xy = (p.x > 0.0f) ? r.xy : r.zw;
    r.x  = (p.y > 0.0f) ? r.x  : r.y;
    float2 q = abs(p) - b + r.x;
    return length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f) - r.x;
}

// 椭圆精确 SDF（IQ 解析公式，通过三次方程求最近点，任意椭圆率均精确）
// 来源：https://iquilezles.org/articles/ellipsedist/
// 算法：将椭圆最近点问题转化为一元三次方程，分两个分支求解（d<0 用 acos，d>=0 用立方根）。
// 约定：外正内负（返回值正=外部，负=内部），与其余 SDF 保持一致。
float ellipse_sdf(float2 p, float2 ab) {
    // 将问题折叠到第一象限；确保 ab.y >= ab.x（长轴在 y 方向）
    // 若 a > b（x 轴为长轴），交换两轴，将问题等价变换为 b' > a' 的情况
    p = abs(p);
    if (ab.x > ab.y) { p = p.yx; ab = ab.yx; }
    // 圆形特殊处理（l = 0 → m/n 分母为零）
    float l = ab.y*ab.y - ab.x*ab.x;
    if (l < 1e-4f) return length(p) - ab.x;
    float m  = ab.x*p.x / l;   float m2 = m*m;
    float n  = ab.y*p.y / l;   float n2 = n*n;
    float c  = (m2 + n2 - 1.0f) / 3.0f;
    float c3 = c*c*c;
    float q  = c3 + m2*n2*2.0f;
    float d  = c3 + m2*n2;
    float g  = m + m*n2;

    float co;
    if (d < 0.0f) {
        // 三个实根分支：用 acos 公式
        // d<0 分支中 c<0（故 c3<0），直接除（无零值风险），再 clamp 至 [-1,1] 保证 acos 稳定
        float h  = acos(clamp(q / c3, -1.0f, 1.0f)) / 3.0f;
        float s  = cos(h);
        float t  = sin(h) * 1.7320508f;  // sqrt(3)
        float rx = sqrt(max(-c*(s + t + 2.0f) + m2, 0.0f));
        float ry = sqrt(max(-c*(s - t + 2.0f) + m2, 0.0f));
        co = (ry + sign(l)*rx + abs(g) / max(rx*ry, 1e-7f) - m) / 2.0f;
    } else {
        // 一个实根分支：用立方根公式
        float h  = 2.0f*m*n*sqrt(d);
        float s  = sign(q + h) * pow(abs(q + h), 1.0f/3.0f);
        float u  = sign(q - h) * pow(abs(q - h), 1.0f/3.0f);
        float rx = -s - u - c*4.0f + 2.0f*m2;
        float ry = (s - u) * 1.7320508f;
        float rm = sqrt(rx*rx + ry*ry);
        // ry/sqrt(rm-rx) = sign(ry)*sqrt(rm+rx)（代数等价，避免 rm≈rx 时的下溢）
        co = (sign(ry)*sqrt(max(rm + rx, 0.0f)) + 2.0f*g / max(rm, 1e-7f) - m) / 2.0f;
    }

    // co 为最近椭圆点的 x 归一化坐标，clamp 防止数值漂移超出 [0,1]
    co = clamp(co, 0.0f, 1.0f);
    float2 r = ab * float2(co, sqrt(max(1.0f - co*co, 0.0f)));
    // 用椭圆方程判断内外：(p/ab)² - 1 < 0 = 内部（负）；> 0 = 外部（正）
    // 比 sign(p.y-r.y) 更稳健，避免 p 在 x 轴时两者均为 0 导致 sign=0 的歧义
    float inside = dot(p / ab, p / ab) - 1.0f;
    return length(r - p) * sign(inside);
}

// ── 像素着色器主函数 ─────────────────────────────────────────────────────────

// ── 多边形顶点常量缓冲（b1 槽，仅多边形 DrawCall 绑定）────────────────────
// 内存布局（16 字节对齐）：
//   偏移   0: poly_vert_count（int）— 实际顶点数（3..64）
//   偏移   4: _poly_pad0/1/2（填充）
//   偏移  16: poly_verts[0..63]（float4，每顶点 xy=本地坐标，zw 填充为0）
cbuffer PolygonVertsCB : register(b1) {
    int   poly_vert_count;  // 多边形实际顶点数（3..64）
    float _poly_pad0;
    float _poly_pad1;
    float _poly_pad2;
    float4 poly_verts[64];  // 顶点本地坐标（x=local_x, y=local_y，zw 不使用）
};

// 多边形 SDF（IQ 绕数法，支持凸多边形和凹多边形，任意简单多边形均正确）
// 算法来源：https://iquilezles.org/articles/distfunctions2d/
// 参数 p：当前像素的本地坐标（以 AABB 中心为原点，单位像素）
// 返回值：外正内负的有向距离（像素单位）
float sdPolygon(float2 p) {
    int n = poly_vert_count;
    // 以第一条边到 p 的最近点距离平方作为初始最小距离
    float d = dot(p - poly_verts[0].xy, p - poly_verts[0].xy);
    float s = 1.0f;  // 符号（1=外部，-1=内部）
    [loop]
    for (int i = 0, j = n - 1; i < n; j = i, i++) {
        float2 vi = poly_verts[i].xy;
        float2 vj = poly_verts[j].xy;
        // 点 p 到边 (vi, vj) 的最近点投影，计算距离平方
        float2 e = vj - vi;
        float2 w = p - vi;
        float2 b = w - e * clamp(dot(w, e) / dot(e, e), 0.0f, 1.0f);
        d = min(d, dot(b, b));
        // 用射线法（绕数法变体）判断 p 是否在多边形内部，确定 SDF 符号
        // 三个条件同为 true 或同为 false 时翻转符号（等价于 all(cond) || all(!cond)）
        bool c0 = (p.y >= vi.y);
        bool c1 = (p.y <  vj.y);
        bool c2 = (e.x * w.y > e.y * w.x);
        if ((c0 && c1 && c2) || (!c0 && !c1 && !c2)) s = -s;
    }
    return s * sqrt(d);
}

struct SdfPSIn {
    float4 pos     : SV_Position;
    float4 color   : COLOR;
    float2 local   : TEXCOORD0;
    float4 params0 : TEXCOORD1;  // (kind, half_w, half_h, p0)
    float4 params1 : TEXCOORD2;  // (p1, p2, p3, stroke_w)
    float4 extra   : TEXCOORD3;  // 扩展参数（圆角四边独立描边：x=r_tl, y=r_tr, z=r_br, w=r_bl）
};

// ── 裁剪 SDF 辅助函数 ──────────────────────────────────────────────────────────

// 内联多边形 SDF（使用 ClipSdfCB 层内顶点，以裁剪 AABB 中心为原点）
// li = 裁剪层索引，nv = 多边形顶点数
float sdClipPolygon(float2 p, int li, int nv) {
    float d2 = dot(p - clip_layers[li].poly_verts[0].xy,
                   p - clip_layers[li].poly_verts[0].xy);
    float s = 1.0f;
    [loop]
    for (int vi = 0, vj = nv - 1; vi < nv; vj = vi, vi++) {
        float2 vvi = clip_layers[li].poly_verts[vi].xy;
        float2 vvj = clip_layers[li].poly_verts[vj].xy;
        float2 e = vvj - vvi;
        float2 w = p - vvi;
        float2 b2 = w - e * clamp(dot(w, e) / dot(e, e), 0.0f, 1.0f);
        d2 = min(d2, dot(b2, b2));
        bool c0 = (p.y >= vvi.y);
        bool c1 = (p.y <  vvj.y);
        bool c2 = (e.x * w.y > e.y * w.x);
        if ((c0 && c1 && c2) || (!c0 && !c1 && !c2)) s = -s;
    }
    return s * sqrt(d2);
}

// 评估所有裁剪层的综合覆盖率（0=完全裁剪，1=完全可见，[0,1]=抗锯齿过渡）
// sv_pos：像素着色器 SV_Position.xy（物理像素坐标）
float evaluate_clip_coverage(float2 sv_pos) {
    if (clip_count <= 0) return 1.0f;
    // 物理像素坐标 → 逻辑像素坐标（使用视口缩放比）
    float2 lp = sv_pos * viewport_size / phys_viewport_size;
    float coverage = 1.0f;
    [loop]
    for (int ci = 0; ci < clip_count; ci++) {
        float2 cp  = lp - clip_layers[ci].bounds.xy;  // 相对裁剪中心的偏移
        float  chw = clip_layers[ci].bounds.z;
        float  chh = clip_layers[ci].bounds.w;
        int    ck  = (int)(clip_layers[ci].kind_p0_p1_p2.x + 0.5f);
        float  dc;
        if (ck == 1) {
            // 均匀圆角矩形
            dc = rounded_box_sdf(cp, float2(chw, chh), clip_layers[ci].kind_p0_p1_p2.y);
        } else if (ck == 2) {
            // 四角独立圆角矩形（p0=r_br, p1=r_tr, p2=r_bl, p3=r_tl）
            float r0 = clip_layers[ci].kind_p0_p1_p2.y;
            float r1 = clip_layers[ci].kind_p0_p1_p2.z;
            float r2 = clip_layers[ci].kind_p0_p1_p2.w;
            float r3 = clip_layers[ci].p3_nv_pad.x;
            dc = complex_rounded_box_sdf(cp, float2(chw, chh), float4(r0, r1, r2, r3));
        } else if (ck == 10) {
            // 多边形
            int nv = clamp((int)(clip_layers[ci].p3_nv_pad.y + 0.5f), 3, 64);
            dc = sdClipPolygon(cp, ci, nv);
        } else {
            // 矩形（ck == 0）
            dc = box_sdf(cp, float2(chw, chh));
        }
        float fw_c = max(fwidth(dc), 0.5f);
        coverage *= 1.0f - smoothstep(-fw_c, fw_c, dc);
    }
    return coverage;
}

float4 main(SdfPSIn i) : SV_Target {
    // 解包 SDF 参数
    int   kind     = (int)(i.params0.x + 0.5f);
    float half_w   = i.params0.y;
    float half_h   = i.params0.z;
    float p0       = i.params0.w;  // 圆角半径参数 0（均匀圆角/右下）
    float p1       = i.params1.x;  // 圆角半径参数 1（右上）
    float p2       = i.params1.y;  // 圆角半径参数 2（左下）
    float p3       = i.params1.z;  // 圆角半径参数 3（左上）
    float stroke_w = i.params1.w;  // 描边总宽度（0 = 填充模式）

    float2 p = i.local;
    float2 b = float2(half_w, half_h);

    // 根据形状类型计算有向距离场（外正内负）
    float d;
    if (kind == 0) {
        // 矩形
        d = box_sdf(p, b);
    } else if (kind == 1) {
        // 均匀圆角矩形
        d = rounded_box_sdf(p, b, p0);
    } else if (kind == 2) {
        // 四角独立圆角矩形（p0=右下, p1=右上, p2=左下, p3=左上）
        d = complex_rounded_box_sdf(p, b, float4(p0, p1, p2, p3));
    } else if (kind == 4) {
        // 四边不等宽矩形内侧描边（p0=top_w, p1=right_w, p2=bottom_w, p3=left_w）
        //
        // 算法：对四条边各自独立计算描边遮罩，取联集（max）。
        //   - 到上边缘的距离：dist_top    = p.y + b.y  （0 在上边缘，正值向下）
        //   - 到下边缘的距离：dist_bottom = b.y - p.y  （0 在下边缘，正值向上）
        //   - 到左边缘的距离：dist_left   = p.x + b.x
        //   - 到右边缘的距离：dist_right  = b.x - p.x
        // 当 dist < edge_width 时该像素在对应边的描边带内；用 smoothstep AA 过渡。
        // 宽度为 0 的边直接返回 0，不产生任何描边，彻底避免宽度相等时的 SDF 叠乘伪影。
        float d_outer = box_sdf(p, b);
        float fw_o = max(fwidth(d_outer), 0.5f);
        float a_outer = 1.0f - smoothstep(-fw_o, fw_o, d_outer);  // 1=矩形内，外侧 AA

        float fy = max(fwidth(p.y), 0.5f);
        float fx = max(fwidth(p.x), 0.5f);
        // 各边遮罩：dist < edge_width → 1.0（在带内），dist > edge_width → 0.0（带外），smoothstep AA
        float a_top    = p0 > 0.0f ? 1.0f - smoothstep(p0 - fy, p0 + fy, p.y + b.y) : 0.0f;
        float a_bottom = p2 > 0.0f ? 1.0f - smoothstep(p2 - fy, p2 + fy, b.y - p.y) : 0.0f;
        float a_left   = p3 > 0.0f ? 1.0f - smoothstep(p3 - fx, p3 + fx, p.x + b.x) : 0.0f;
        float a_right  = p1 > 0.0f ? 1.0f - smoothstep(p1 - fx, p1 + fx, b.x - p.x) : 0.0f;
        // 联集：任意一边有描边即显示；乘以外矩形遮罩确保超出矩形外的部分透明
        float a_stroke = max(max(a_top, a_bottom), max(a_left, a_right));
        float al = a_outer * a_stroke;
        float4 c2 = i.color;
        al *= evaluate_clip_coverage(i.pos.xy);
        return float4(c2.rgb * c2.a * al, c2.a * al);
    } else if (kind == 5) {
        // 四边不等宽 + 四角独立圆角内侧描边
        //
        // 算法：外轮廓圆角矩形 SDF 减去内轮廓圆角矩形 SDF，描边区域 = 外内之差。
        // 内矩形由 Thickness 向各边收缩：
        //   inner_b.x = b.x - (left + right) / 2
        //   inner_b.y = b.y - (top  + bottom) / 2
        //   inner_p   = p - float2((left - right) / 2, (top - bottom) / 2)
        // 内圆角（仿 ComplexRoundedRect::inner_rect 公式，各向同性化后取 min）：
        //   ir_tl = max(0, r_tl - max(left, top))   ← x=r_tl-left, y=r_tl-top 的标量化
        //   其余角依此类推
        //
        // p0=top_w, p1=right_w, p2=bottom_w, p3=left_w（math::Thickness 字段顺序：left,top,right,bottom）
        // extra.x=r_tl（左上）, extra.y=r_tr（右上）, extra.z=r_br（右下）, extra.w=r_bl（左下）
        float r_tl = i.extra.x;
        float r_tr = i.extra.y;
        float r_br = i.extra.z;
        float r_bl = i.extra.w;

        // 外轮廓（含圆角）
        float d_outer = complex_rounded_box_sdf(p, b, float4(r_br, r_tr, r_bl, r_tl));
        float fw_o = max(fwidth(d_outer), 0.5f);
        float a_outer = 1.0f - smoothstep(-fw_o, fw_o, d_outer);

        // 内矩形：各边内缩，中心随非对称边宽偏移。
        //
        // 问题：当某边宽为0时，内矩形该侧边界与外矩形边界重合，
        //       两个 smoothstep 过渡同时激活，产生 a*(1-a)≤0.25 的 ghost 细线。
        //
        // 修复：对宽度为0的边，将内矩形对应侧边界向外扩展 (fw_o+1)px，
        //       超出外矩形 AA 区，使该侧 a_inner→1，消除 ghost。
        //       扩展时只移动零宽那侧边界，通过同步调整内矩形中心保证对侧边界不变。
        //
        // 设 la/ra/ta/ba 为各侧扩展量（零宽边取 fw_o+1，否则取 0）：
        //   inner_center_x 偏移 = (p3-p1-la+ra)/2  （向右为正）
        //   inner_b.x = b.x - (p3+p1)/2 + (la+ra)/2
        //   inner_p.x = p.x - (p3-p1-la+ra)/2
        float ghost = fw_o + 1.0f;
        float la = p3 < 0.0001f ? ghost : 0.0f;  // 左边宽=0时，内矩形左侧外扩
        float ra = p1 < 0.0001f ? ghost : 0.0f;  // 右边宽=0时，内矩形右侧外扩
        float ta = p0 < 0.0001f ? ghost : 0.0f;  // 上边宽=0时，内矩形上侧外扩
        float ba = p2 < 0.0001f ? ghost : 0.0f;  // 下边宽=0时，内矩形下侧外扩
        float2 inner_b = max(float2(b.x - (p3 + p1) * 0.5f + (la + ra) * 0.5f,
                                    b.y - (p0 + p2) * 0.5f + (ta + ba) * 0.5f),
                             float2(0.0f, 0.0f));
        float2 inner_p = p - float2((p3 - p1 - la + ra) * 0.5f, (p0 - p2 - ta + ba) * 0.5f);

        // 内圆角（对应 ComplexRoundedRect::inner_rect：x 分量减水平边宽，y 分量减垂直边宽）
        // 各向同性化后：min(r - left, r - top) = r - max(left, top)，再 clamp 到 0
        float ir_tl = max(0.0f, r_tl - max(p3, p0));  // 左上：left=p3, top=p0
        float ir_tr = max(0.0f, r_tr - max(p1, p0));  // 右上：right=p1, top=p0
        float ir_br = max(0.0f, r_br - max(p1, p2));  // 右下：right=p1, bottom=p2
        float ir_bl = max(0.0f, r_bl - max(p3, p2));  // 左下：left=p3, bottom=p2

        // 内轮廓
        float d_inner = complex_rounded_box_sdf(inner_p, inner_b, float4(ir_br, ir_tr, ir_bl, ir_tl));
        float fw_i = max(fwidth(d_inner), 0.5f);
        float a_inner = 1.0f - smoothstep(-fw_i, fw_i, d_inner);

        // 外轮廓内 且 内轮廓外 → 描边带
        float al = a_outer * (1.0f - a_inner);
        float4 c3 = i.color;
        al *= evaluate_clip_coverage(i.pos.xy);
        return float4(c3.rgb * c3.a * al, c3.a * al);
    } else if (kind == 6) {
        // ── 线段 SDF 描边（天然抗锯齿，含独立起止端点样式）──────────────
        //
        // 参数映射：
        //   extra.xy = 端点 A 本地坐标（起点，以线段中心为原点）
        //   extra.zw = 端点 B 本地坐标（终点，以线段中心为原点）
        //   p0       = start_cap（0=Flat 截断, 1=Round 圆形, 2=Square 方形延伸）
        //   p1       = end_cap（同上）
        //   stroke_w = 线宽（总宽度，非半宽）
        //
        // 算法：将像素投影到线段方向和法线方向，
        //   按投影位置分三段（A 端/主体/B 端）分别计算 SDF，
        //   最终用 smoothstep AA 过渡。
        float2 a = i.extra.xy;             // 端点 A（本地坐标）
        float2 b = i.extra.zw;             // 端点 B（本地坐标）
        float half_sw = stroke_w * 0.5f;  // 线宽一半

        float2 ba = b - a;
        float seg_len = length(ba);
        // 线段单位方向向量（零长线段退化为水平方向）
        float2 dir = seg_len > 1e-6f ? ba / seg_len : float2(1.0f, 0.0f);
        // 法线单位向量（左旋 90°）
        float2 nor = float2(-dir.y, dir.x);

        float2 pa = p - a;                      // 像素相对于端点 A 的向量
        float proj_along = dot(pa, dir);        // 沿线段方向的投影（0=A点, seg_len=B点）
        float v = abs(dot(pa, nor));            // 垂直线段方向的距离（取绝对值）

        int scap = (int)(p0 + 0.5f);           // start_cap 类型
        int ecap = (int)(p1 + 0.5f);           // end_cap 类型

        float d6;
        if (proj_along < 0.0f) {
            // A 端 cap 区域（投影超出 A 点之外）
            if (scap == 1) {
                // Round：圆形端点，到端点 A 的欧氏距离 - 半线宽
                d6 = length(pa) - half_sw;
            } else if (scap == 2) {
                // Square：从 A 点向外延伸 half_sw 的矩形角
                d6 = length(float2(max(-proj_along - half_sw, 0.0f),
                                   max(v - half_sw, 0.0f)));
            } else {
                // Flat（默认）：在 A 端面精确截断，超出端面即外部
                d6 = length(float2(-proj_along, max(v - half_sw, 0.0f)));
            }
        } else if (proj_along > seg_len) {
            // B 端 cap 区域（投影超出 B 点之外）
            float2 pb = p - b;                  // 像素相对于端点 B 的向量
            float pb_over = proj_along - seg_len; // 超出 B 端的量（>0）
            if (ecap == 1) {
                // Round：圆形端点，到端点 B 的欧氏距离 - 半线宽
                d6 = length(pb) - half_sw;
            } else if (ecap == 2) {
                // Square：从 B 点向外延伸 half_sw 的矩形角
                d6 = length(float2(max(pb_over - half_sw, 0.0f),
                                   max(v - half_sw, 0.0f)));
            } else {
                // Flat（默认）：在 B 端面精确截断
                d6 = length(float2(pb_over, max(v - half_sw, 0.0f)));
            }
        } else {
            // 主体区域（投影在线段内 [0, seg_len]）：仅垂直方向距离
            d6 = v - half_sw;
        }

        float fw6 = max(fwidth(d6), 0.5f);
        float al6 = 1.0f - smoothstep(-fw6, fw6, d6);
        float4 cs = i.color;
        al6 *= evaluate_clip_coverage(i.pos.xy);
        return float4(cs.rgb * cs.a * al6, cs.a * al6);
    }

    else if (kind == 7) {
        // ── 圆弧 SDF 描边（IQ 旋转坐标系法，支持 Flat/Round 端点）────────
        //
        // 参数映射：
        //   p0       = 圆弧半径 r
        //   p1       = 弧中心角（弧度，= start_angle + sweep * 0.5）
        //   p2       = 半张角（弧度，= |sweep| * 0.5）
        //   p3       = cap 样式（0=Flat 截断, 1=Round 圆形；两端使用相同 cap）
        //   stroke_w = 线宽（总宽度，非半宽）
        //   extra.xy = 圆心本地坐标（Quad 中心通常即为圆心，故通常为 (0,0)）
        //
        // 算法（IQ arc SDF）：
        //   1. 将 p 旋转到以弧中心角方向为 y 轴正方向的坐标系（q）
        //   2. 利用弧对称性，折叠 q.x → abs(q.x)（化为单侧端点判断）
        //   3. 条件 sc.y*qa.x > sc.x*qa.y 判断 qa 是否在端点扇形区域外
        //      是 → 端点 cap SDF；否 → 圆环 SDF（abs(|qa|-r) - half_sw）
        //   Flat cap：用端点处切线截断矩形截面 SDF（类比线段 Flat cap）
        //   Round cap：到端点的欧氏距离 - half_sw
        float arc_r   = p0;
        float mid     = p1;                    // 弧中心角（弧度）
        float hs      = p2;                    // 半张角（弧度，= |sweep| * 0.5）
        float half_sw7 = stroke_w * 0.5f;

        // 相对圆心的本地向量（圆心本地坐标 = extra.xy，通常为 (0,0)）
        float2 pc7 = p - i.extra.xy;

        // 旋转坐标系：令弧中心角方向 (cos(mid), sin(mid)) 对齐 y 轴正方向
        //   新 x 轴 = (-sin(mid), cos(mid))（左旋 90°）
        //   新 y 轴 = ( cos(mid), sin(mid))（弧中心角方向）
        float cm = cos(mid), sm = sin(mid);
        float2 q7;
        q7.x = -pc7.x * sm + pc7.y * cm;  // 垂直弧中心角方向
        q7.y =  pc7.x * cm + pc7.y * sm;  // 沿弧中心角方向

        // 端点方向向量（旋转坐标系中）：sc = (sin(hs), cos(hs))
        //   弧端点在 ±hs 处，旋转坐标系中为 (±sin(hs), cos(hs))
        float2 sc = float2(sin(hs), cos(hs));

        // 利用弧的左右对称性（关于 y 轴），折叠 q7.x → abs(q7.x)
        float2 qa = float2(abs(q7.x), q7.y);

        float d7;
        if (sc.y * qa.x > sc.x * qa.y) {
            // qa 在弧端点外侧扇形区域（角度超出 hs）→ 端点 cap 处理
            // 端点坐标（折叠坐标系）：Pend = sc * r = (sin(hs)*r, cos(hs)*r)
            float2 Pend = sc * arc_r;

            int cap7 = (int)(p3 + 0.5f);  // abs 折叠后两端对称，使用统一 cap
            if (cap7 == 1) {
                // Round cap：到端点的欧氏距离 - 半线宽
                d7 = length(qa - Pend) - half_sw7;
            } else {
                // Flat cap：在端点处沿切线方向截断（矩形截面 SDF）
                // 端点处切线方向（沿弧增大方向）：tc = (cos(hs), -sin(hs)) = (sc.y, -sc.x)
                // 沿切线超出端点的量（在端点外侧区域内，此值 > 0）
                float along7 = qa.x * sc.y - qa.y * sc.x;
                // 到圆弧的径向距离（在半线宽内为负，超出为正）
                float rdist7 = abs(length(qa) - arc_r) - half_sw7;
                // 类比线段 Flat cap：length(along, max(rdist, 0))
                d7 = length(float2(along7, max(rdist7, 0.0f)));
            }
        } else {
            // qa 在弧段范围内（角度 ≤ hs）→ 圆环 SDF
            d7 = abs(length(qa) - arc_r) - half_sw7;
        }

        float fw7 = max(fwidth(d7), 0.5f);
        float al7 = 1.0f - smoothstep(-fw7, fw7, d7);
        float4 cs7 = i.color;
        al7 *= evaluate_clip_coverage(i.pos.xy);
        return float4(cs7.rgb * cs7.a * al7, cs7.a * al7);
    } else if (kind == 8) {
        // ── 二次贝塞尔曲线描边 SDF（IQ 解析法，支持 Flat/Round cap）────
        //
        // 参数映射：
        //   half_w, half_h = P2 本地坐标（终点）← 借用"半尺寸"槽
        //   p0        = start_cap（0=Flat 截断, 1=Round 圆形）
        //   p1        = end_cap（同上）
        //   stroke_w  = 线宽（总宽度）
        //   extra.xy  = P0 本地坐标（起点，t=0）
        //   extra.zw  = P1 本地坐标（控制点）
        //
        // 算法（IQ sdBezier，闭合解析解）：
        //   将"点到曲线最近点"问题转化为解三次方程，通过 Cardano 公式求解。
        //   clamp(t,0,1) 天然处理端点 → Round cap（端点圆形延伸）。
        //   Flat cap：额外用端点处切线半平面截断（CSG max 操作）。
        float2 Bz_A = i.extra.xy;              // P0（起点）
        float2 Bz_B = i.extra.zw;              // P1（控制点）
        float2 Bz_C = float2(half_w, half_h);  // P2（终点，借用 half_w/half_h 槽）
        float half_sw8 = stroke_w * 0.5f;

        // IQ sdBezier 核心：求 p 到二次贝塞尔曲线的最小距离
        float2 bz_a = Bz_B - Bz_A;                         // 一次项系数 / 2
        float2 bz_b = Bz_A - 2.0f * Bz_B + Bz_C;          // 二次项系数（曲率向量）
        float2 bz_c = bz_a * 2.0f;                         // 切线系数
        float2 bz_d = Bz_A - p;                             // 常数项

        // 三次方程系数（通过换元 u = t - kx 消去二次项）
        float kk = 1.0f / max(dot(bz_b, bz_b), 1e-10f);    // 防除零（曲线退化为直线）
        float kx = kk * dot(bz_a, bz_b);
        float ky = kk * (2.0f * dot(bz_a, bz_a) + dot(bz_d, bz_b)) / 3.0f;
        float kz = kk * dot(bz_d, bz_a);

        // 压缩三次方程为标准形式 u³ + p*u + q = 0
        float bz_p = ky - kx * kx;
        float bz_p3 = bz_p * bz_p * bz_p;
        float bz_q = kx * (2.0f * kx * kx - 3.0f * ky) + kz;
        float bz_h = bz_q * bz_q + 4.0f * bz_p3;          // 判别式

        float d8_sq;
        if (bz_h >= 0.0f) {
            // 判别式 ≥ 0：一个实根（另两个复根）
            float sq_h = sqrt(bz_h);
            float2 x_vec = (float2(sq_h, -sq_h) - bz_q) * 0.5f;
            // 实数立方根（保留符号）
            float2 uv = sign(x_vec) * pow(abs(x_vec), float2(1.0f / 3.0f, 1.0f / 3.0f));
            float t_cand = clamp(uv.x + uv.y - kx, 0.0f, 1.0f);
            float2 pt_cand = Bz_A + (bz_c + bz_b * t_cand) * t_cand;
            d8_sq = dot(p - pt_cand, p - pt_cand);
        } else {
            // 判别式 < 0：三个实根，逐一计算取最小距离
            float bz_z = sqrt(-bz_p);
            float bz_v = acos(clamp(bz_q / (bz_p * bz_z * 2.0f), -1.0f, 1.0f)) / 3.0f;
            float bz_m = cos(bz_v);
            float bz_n = sin(bz_v) * 1.7320508075688772f;  // √3
            float3 t3  = clamp(float3(bz_m + bz_m, -bz_n - bz_m, bz_n - bz_m) * bz_z - kx,
                               0.0f, 1.0f);
            float2 pt0 = Bz_A + (bz_c + bz_b * t3.x) * t3.x;
            float2 pt1 = Bz_A + (bz_c + bz_b * t3.y) * t3.y;
            float2 pt2 = Bz_A + (bz_c + bz_b * t3.z) * t3.z;
            float d0_sq = dot(p - pt0, p - pt0);
            float d1_sq = dot(p - pt1, p - pt1);
            float d2_sq = dot(p - pt2, p - pt2);
            d8_sq = min(min(d0_sq, d1_sq), d2_sq);
        }

        float d8 = sqrt(d8_sq) - half_sw8;

        // Flat cap 截断（CSG max：用端点切线半平面限制描边区域）
        //   P0 处切线方向（指向曲线内侧）：normalize(Bz_B - Bz_A)
        //   当 p 在 P0 外侧（与切线方向相反），dot(A - p, t0) > 0，截断生效
        int scap8 = (int)(p0 + 0.5f);
        int ecap8 = (int)(p1 + 0.5f);
        if (scap8 == 0) {
            // P0 Flat cap：切线 = Bz_B - Bz_A（归一化）
            float2 t0_len = Bz_B - Bz_A;
            float t0_l = length(t0_len);
            float2 t0  = t0_l > 1e-6f ? t0_len / t0_l : float2(1.0f, 0.0f);
            // dot(Bz_A - p, t0) > 0 表示 p 在 P0 外侧（切线背面），截断
            d8 = max(d8, dot(Bz_A - p, t0));
        }
        if (ecap8 == 0) {
            // P2 Flat cap：切线 = Bz_C - Bz_B（归一化）
            float2 t1_len = Bz_C - Bz_B;
            float t1_l = length(t1_len);
            float2 t1  = t1_l > 1e-6f ? t1_len / t1_l : float2(1.0f, 0.0f);
            // dot(p - Bz_C, t1) > 0 表示 p 在 P2 外侧（切线正面），截断
            d8 = max(d8, dot(p - Bz_C, t1));
        }

        float fw8 = max(fwidth(d8), 0.5f);
        float al8 = 1.0f - smoothstep(-fw8, fw8, d8);
        float4 cs8 = i.color;
        al8 *= evaluate_clip_coverage(i.pos.xy);
        return float4(cs8.rgb * cs8.a * al8, cs8.a * al8);
    } else if (kind == 9) {
        // ── 三次贝塞尔曲线描边 SDF（四区间候选 + Newton 精化）──
        //
        // 参数映射：
        //   half_w, half_h = P3 本地坐标；p2, p3 = P2 本地坐标
        //   p0=start_cap, p1=end_cap, stroke_w=线宽
        //   extra.xy=P0, extra.zw=P1
        //
        // 三次贝塞尔点到曲线的距离函数最多有 3 个局部极小值。
        // 三段方案的中段 [1/3,2/3] 对于 S 形曲线可能包含两个极小值（拐点
        // 附近像素），两者都在中段内时候选 tb 只能覆盖其一，导致孤立亮斑。
        // 改为四段方案 [0,1/4]、[1/4,1/2]、[1/2,3/4]、[3/4,1]，将中间区域
        // 分成两段，保证两个极小值各自被不同候选覆盖。
        float2 Cb_A = i.extra.xy;               // P0（起点）
        float2 Cb_B = i.extra.zw;               // P1（第一控制点）
        float2 Cb_C = float2(p2, p3);           // P2（第二控制点）← 借用 p2/p3 槽
        float2 Cb_D = float2(half_w, half_h);   // P3（终点）← 借用 half_w/half_h 槽
        float half_sw9 = stroke_w * 0.5f;

        // 三次贝塞尔多项式系数（Horner 展开）
        float2 cb3 = -Cb_A + 3.0f * Cb_B - 3.0f * Cb_C + Cb_D;
        float2 cb2 =  3.0f * Cb_A - 6.0f * Cb_B + 3.0f * Cb_C;
        float2 cb1 = -3.0f * Cb_A + 3.0f * Cb_B;
        float2 cb0 =  Cb_A;

        // ── 阶段 1：四区间各 12 步采样，找四个独立候选 t ────────────────────
        float ta = 0.0f, tb = 0.375f, tc = 0.625f, td = 1.0f;
        float da_sq = 1e20f, db_sq = 1e20f, dc_sq = 1e20f, dd_sq = 1e20f;
        [unroll]
        for (int si = 0; si <= 48; si++) {
            float t = float(si) / 48.0f;
            float2 q = ((cb3 * t + cb2) * t + cb1) * t + cb0;
            float  dd = dot(p - q, p - q);
            if (t <= 0.25f) {
                if (dd < da_sq) { ta = t; da_sq = dd; }
            } else if (t <= 0.5f) {
                if (dd < db_sq) { tb = t; db_sq = dd; }
            } else if (t <= 0.75f) {
                if (dd < dc_sq) { tc = t; dc_sq = dd; }
            } else {
                if (dd < dd_sq) { td = t; dd_sq = dd; }
            }
        }

        // ── 阶段 2：四个候选各做 6 步 Newton-Raphson 精化 ──────────────────
        // f(t) = dot(Q(t)-p, Q'(t)) = 0，牛顿步：t -= f / f'
        [unroll]
        for (int ni = 0; ni < 6; ni++) {
            // 候选 a
            float2 qa   = ((cb3 * ta + cb2) * ta + cb1) * ta + cb0;
            float2 dqa  = (3.0f * cb3 * ta + 2.0f * cb2) * ta + cb1;
            float2 d2qa = 6.0f * cb3 * ta + 2.0f * cb2;
            float2 dfa  = qa - p;
            float  fa   = dot(dfa, dqa);
            float  fpa  = dot(dqa, dqa) + dot(dfa, d2qa);
            if (abs(fpa) > 1e-10f) ta -= fa / fpa;
            ta = clamp(ta, 0.0f, 1.0f);
            // 候选 b
            float2 qb   = ((cb3 * tb + cb2) * tb + cb1) * tb + cb0;
            float2 dqb  = (3.0f * cb3 * tb + 2.0f * cb2) * tb + cb1;
            float2 d2qb = 6.0f * cb3 * tb + 2.0f * cb2;
            float2 dfb  = qb - p;
            float  fb   = dot(dfb, dqb);
            float  fpb  = dot(dqb, dqb) + dot(dfb, d2qb);
            if (abs(fpb) > 1e-10f) tb -= fb / fpb;
            tb = clamp(tb, 0.0f, 1.0f);
            // 候选 c
            float2 qc   = ((cb3 * tc + cb2) * tc + cb1) * tc + cb0;
            float2 dqc  = (3.0f * cb3 * tc + 2.0f * cb2) * tc + cb1;
            float2 d2qc = 6.0f * cb3 * tc + 2.0f * cb2;
            float2 dfc  = qc - p;
            float  fc   = dot(dfc, dqc);
            float  fpc  = dot(dqc, dqc) + dot(dfc, d2qc);
            if (abs(fpc) > 1e-10f) tc -= fc / fpc;
            tc = clamp(tc, 0.0f, 1.0f);
            // 候选 d
            float2 qd   = ((cb3 * td + cb2) * td + cb1) * td + cb0;
            float2 dqd  = (3.0f * cb3 * td + 2.0f * cb2) * td + cb1;
            float2 d2qd = 6.0f * cb3 * td + 2.0f * cb2;
            float2 dfd  = qd - p;
            float  fd   = dot(dfd, dqd);
            float  fpd  = dot(dqd, dqd) + dot(dfd, d2qd);
            if (abs(fpd) > 1e-10f) td -= fd / fpd;
            td = clamp(td, 0.0f, 1.0f);
        }

        // ── 阶段 3：取四候选精化结果中距离最小者 ─────────────────────────
        float2 cla = ((cb3 * ta + cb2) * ta + cb1) * ta + cb0;
        float2 clb = ((cb3 * tb + cb2) * tb + cb1) * tb + cb0;
        float2 clc = ((cb3 * tc + cb2) * tc + cb1) * tc + cb0;
        float2 cld = ((cb3 * td + cb2) * td + cb1) * td + cb0;
        float d9 = min(min(length(p - cla), length(p - clb)),
                       min(length(p - clc), length(p - cld))) - half_sw9;

        // Flat cap 截断（CSG max：端点切线半平面截断）
        int scap9 = (int)(p0 + 0.5f);
        int ecap9 = (int)(p1 + 0.5f);
        if (scap9 == 0) {
            float2 t0_vec = Cb_B - Cb_A;
            float  t0_l   = length(t0_vec);
            float2 t0     = t0_l > 1e-6f ? t0_vec / t0_l : float2(1.0f, 0.0f);
            d9 = max(d9, dot(Cb_A - p, t0));
        }
        if (ecap9 == 0) {
            float2 t1_vec = Cb_D - Cb_C;
            float  t1_l   = length(t1_vec);
            float2 t1     = t1_l > 1e-6f ? t1_vec / t1_l : float2(1.0f, 0.0f);
            d9 = max(d9, dot(p - Cb_D, t1));
        }

        float fw9 = max(fwidth(d9), 0.5f);
        float al9 = 1.0f - smoothstep(-fw9, fw9, d9);
        float4 cs9 = i.color;
        al9 *= evaluate_clip_coverage(i.pos.xy);
        return float4(cs9.rgb * cs9.a * al9, cs9.a * al9);
    }

    else if (kind == 10 || kind == 11) {
        // ── kind=10：FillPolygon（填充多边形 SDF）─────────────────────────
        // ── kind=11：StrokePolygon（描边多边形 SDF）──────────────────────
        //
        // 顶点数据来自 PolygonVertsCB（b1 槽），坐标为本地坐标（以 AABB 中心为原点）。
        // sdPolygon() 使用 IQ 绕数法，同时支持凸多边形和凹多边形。
        float dpoly = sdPolygon(p);

        float fpoly = max(fwidth(dpoly), 0.5f);
        float apoly;
        if (kind == 10) {
            // 填充：多边形内部（d<0）不透明，边界处平滑过渡
            apoly = 1.0f - smoothstep(-fpoly, fpoly, dpoly);
        } else {
            // 描边：围绕多边形轮廓向内外各扩展 stroke_w/2
            float half_sw11 = stroke_w * 0.5f;
            float a_outer   = 1.0f - smoothstep(-fpoly, fpoly, dpoly - half_sw11);
            float a_inner   = smoothstep(-fpoly, fpoly, dpoly + half_sw11);
            apoly = a_outer * a_inner;
        }
        float4 cpoly = i.color;
        float  apre  = cpoly.a * apoly;
        apre *= evaluate_clip_coverage(i.pos.xy);
        return float4(cpoly.rgb * apre, apre);
    } else {
        // 椭圆（half_w = X 半径, half_h = Y 半径）
        d = ellipse_sdf(p, b);
    }

    // 基于 fwidth 的像素级抗锯齿过渡宽度
    float fw = max(fwidth(d), 0.5f);

    // 计算不透明度
    float alpha;
    if (stroke_w <= 0.0f) {
        // 填充模式：d < 0 为内部，smoothstep 在边界处平滑过渡
        alpha = 1.0f - smoothstep(-fw, fw, d);
    } else {
        // 描边模式（中心对齐）：描边覆盖 [-half_sw, half_sw] 区间
        float half_sw = stroke_w * 0.5f;
        // 外边缘 AA：d < half_sw 时显示
        float a_outer = 1.0f - smoothstep(-fw, fw, d - half_sw);
        // 内边缘 AA：d > -half_sw 时显示（往内消失）
        float a_inner = smoothstep(-fw, fw, d + half_sw);
        alpha = a_outer * a_inner;
    }

    // ── 画刷颜色计算（根据 brush_kind 决定颜色来源）──────────────────────────
    float4 c;
    if (brush_kind == 1u) {
        // 线性渐变：t = 像素本地坐标在渐变轴上的投影（归一化）
        float2 grad_start = grad_params.xy;
        float2 grad_dir   = grad_params.zw;  // 终点 - 起点（local 坐标，像素）
        float  len_sq     = dot(grad_dir, grad_dir);
        float  t_lin      = (len_sq > 1e-8f)
            ? dot(p - grad_start, grad_dir) / len_sq
            : 0.0f;
        c = sample_gradient(t_lin);
    } else if (brush_kind == 2u) {
        // 径向渐变：t = (像素到圆心距离 - 内径) / (外径 - 内径)
        float2 center  = grad_params.xy;
        float  outer_r = grad_params.z;
        float  inner_r = grad_params.w;
        float  dist    = length(p - center);
        float  range   = outer_r - inner_r;
        float  t_rad   = (range > 1e-6f) ? (dist - inner_r) / range : 0.0f;
        c = sample_gradient(t_rad);
    } else if (brush_kind == 3u) {
        // 亚克力：采样模糊背景纹理，叠加饱和度调整和色调
        float2 backdrop_uv = i.pos.xy / phys_viewport_size;
        float4 backdrop    = acrylic_backdrop.Sample(acrylic_sampler, backdrop_uv);
        // 饱和度调整（基于亮度保留法）
        float  luma = dot(backdrop.rgb, float3(0.2126f, 0.7152f, 0.0722f));
        backdrop.rgb = lerp(float3(luma, luma, luma), backdrop.rgb, grad_extra.y);
        // 色调叠加（线性混合）
        float4 tint   = grad_params;               // rgba 色调颜色
        float  tint_a = grad_extra.x * tint.a;    // 实际混合比例
        c = float4(lerp(backdrop.rgb, tint.rgb, tint_a), 1.0f);
    } else {
        // 纯色（brush_kind == 0）：使用顶点插值颜色
        c = i.color;
    }

    // 预乘 Alpha 输出（匹配预乘混合模式 ONE / INV_SRC_ALPHA）
    alpha *= evaluate_clip_coverage(i.pos.xy);
    float  a = c.a * alpha;
    return float4(c.rgb * a, a);
}
