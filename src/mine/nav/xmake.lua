-- mine.nav 模块构建配置
-- 任务 20：导航框架（Frame 控件 + INavigationService 接口 + 路由表 + 历史栈）

mine_module("mine.nav", {
    short_name = "nav",
    deps = {
        "mine.core",
        "mine.containers",
        "mine.math",
        "mine.ui.property",
        "mine.ui.visual",
        "mine.ui.controls",   -- Page、ContentControl、UserControl
    },
})
