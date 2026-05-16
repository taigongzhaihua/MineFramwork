set_project("MineFramework")
set_version("0.1.0")
set_xmakever("2.9.9")

add_rules("mode.debug", "mode.release")

includes("xmake/options.lua")
includes("xmake/toolchains.lua")
includes("xmake/rules/mine_module.lua")
includes("xmake/rules/mml_compile.lua")
includes("xmake/rules/pkg_export.lua")

add_requires("doctest", {system = false})

local function include_if_exists(script)
    if os.isfile(path.join(os.scriptdir(), script)) then
        includes(script)
    end
end

for _, script in ipairs({
    "modules/mineui.lua",
    "src/mine/core/xmake.lua",
    "src/mine/containers/xmake.lua",
    "src/mine/diag/xmake.lua",
    "src/mine/async/xmake.lua",
    "src/mine/io/xmake.lua",
    "src/mine/math/xmake.lua",
    "src/mine/text/xmake.lua",
    "src/mine/reflect/xmake.lua",
    "samples/00-hello-rect/xmake.lua",
    "tools/mmlc/xmake.lua",
    "tools/mmlfmt/xmake.lua",
    "tools/mmlls/xmake.lua"
}) do
    include_if_exists(script)
end

target("mine.bootstrap")
    set_kind("phony")
    set_default(false)
target_end()