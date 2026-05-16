option("mine.profile")
    set_default("std")
    set_showmenu(true)
    set_values("min", "std", "full")
    set_description("MineFramework 体积与功能裁剪预设")
option_end()

option("mine.shared")
    set_default(false)
    set_showmenu(true)
    set_description("输出共享库而不是静态库")
option_end()

option("mine.lto")
    set_default("auto")
    set_showmenu(true)
    set_values("auto", "on", "off")
    set_description("控制链接时优化开关")
option_end()

option("mine.exceptions")
    set_default(false)
    set_showmenu(true)
    set_description("为需要的平台边界模块启用异常")
option_end()

option("mine.rtti")
    set_default(false)
    set_showmenu(true)
    set_description("为需要的模块启用 RTTI")
option_end()

option("mine.gfx.backends")
    if is_plat("windows") then
        set_default("d3d12,d3d11")
    elseif is_plat("macosx", "iphoneos") then
        set_default("metal")
    else
        set_default("vulkan")
    end
    set_showmenu(true)
    set_description("图形后端列表，逗号分隔")
option_end()

option("mine.tls.backend")
    set_default("mbedtls")
    set_showmenu(true)
    set_values("mbedtls", "openssl")
    set_description("TLS 后端选择")
option_end()

option("mine.hot_reload")
    set_default(false)
    set_showmenu(true)
    set_description("启用 MML 运行期反射与热重载")
option_end()

local bootstrap_modules = {
    "core",
    "containers",
    "diag",
    "async",
    "io",
    "math",
    "text",
    "reflect"
}

for _, name in ipairs(bootstrap_modules) do
    option("mine.module." .. name)
        set_default("auto")
        set_showmenu(true)
        set_values("auto", "enabled", "disabled")
        set_description("控制模块 mine." .. name .. " 是否参与构建")
    option_end()
end