-- mine.ui.animation 模块构建配置
-- F2.2 任务 #16：缓动函数库（Linear / Ease / Spring）
-- F2.2 任务 #17（待实现）：Storyboard、Timeline、属性动画

mine_module("mine.ui.animation", {
    short_name = "ui.anim",
    deps = {
        "mine.math",   -- 可能需要 lerp / clamp 等数学辅助函数
    },
})
