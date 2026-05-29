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
add_requires("freetype", {system = false})

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
    "src/mine/platform/abi/xmake.lua",
    "src/mine/platform/win32/xmake.lua",
    "src/mine/gfx/rhi/xmake.lua",
    "src/mine/gfx/d3d11/xmake.lua",
    "src/mine/paint/xmake.lua",
    "src/mine/text/xmake.lua",
    "src/mine/reflect/xmake.lua",
    "src/mine/di/xmake.lua",
    "src/mine/ui/property/xmake.lua",
    "src/mine/ui/binding/xmake.lua",
    "src/mine/ui/event/xmake.lua",
    "src/mine/ui/visual/xmake.lua",
    "src/mine/ui/input/xmake.lua",
    "src/mine/ui/layout/xmake.lua",
    "src/mine/ui/window/xmake.lua",
    "src/mine/ui/app/xmake.lua",
    "src/mine/ui/controls/xmake.lua",
    "src/mine/ui/style/xmake.lua",
    "src/mine/ui/animation/xmake.lua",
    "src/mine/mvvm/xmake.lua",
    "samples/00-hello-rect/xmake.lua",
    "samples/00-blank-window/xmake.lua",
    "samples/02-controls-demo/xmake.lua",
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