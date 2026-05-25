-- mine.ui.animation 模块构建配置
-- F2.2 任务 #16：缓动函数库（Linear / Ease / Spring）
-- F2.2 任务 #17：Storyboard、属性动画 + VisualStateManager 过渡集成

mine_module("mine.ui.animation", {
    short_name = "ui.anim",
    deps = {
        "mine.math",         -- lerp / clamp / Color / Thickness 等数学辅助
        "mine.core",         -- Variant / Function / OwnedPtr 等核心类型
        "mine.containers",   -- SmallVector
        "mine.ui.property",  -- DependencyObject + DependencyProperty + ValuePriority
        "mine.paint",        -- Brush 类型插值（lerp_variant 支持 paint::Brush）
    },
})
