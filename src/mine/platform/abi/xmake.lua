-- mine.platform.abi：平台抽象接口层（纯头文件，无实现）
-- 依赖 mine.core（StringView、OwnedPtr）和 mine.math（Rect、Size、Point）
mine_module("mine.platform.abi", {
    short_name = "platform.abi",
    deps = {"mine.core", "mine.math"},
})
