-- mine.ui.style 模块构建配置
-- F2.1 任务 #10：ResourceDictionary（树形作用域、resource_changed 信号）
-- F2.1 任务 #11：Style + StyleSetter（依赖属性值优先级链，样式层）
-- F2.1 任务 #13：VisualStateManager（状态定义、过渡配置、go_to_state）

mine_module("mine.ui.style", {
    short_name = "ui.style",
    deps = {
        "mine.core",
        "mine.containers",
        "mine.ui.property",  -- Style::apply() 需要 DependencyObject + ValuePriority
    },
})
