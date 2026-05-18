-- mine.gfx.d3d11：Direct3D 11 图形后端实现
-- 仅 Windows 平台编译，依赖 mine.gfx.rhi、mine.core、mine.math

if not is_plat("windows") then
    return
end

mine_module("mine.gfx.d3d11", {
    short_name = "gfx.d3d11",
    deps = {"mine.gfx.rhi", "mine.core", "mine.math"},
})

target("mine.gfx.d3d11")
    -- D3D11 / DXGI 系统链接库
    add_syslinks(
        "d3d11",        -- Direct3D 11 运行时
        "dxgi",         -- DXGI 交换链、适配器枚举
        "d3dcompiler"   -- HLSL 运行时编译（M0 阶段，后续改为预编译字节码）
    )
    add_includedirs("api/include", {public = true})
    add_headerfiles("api/include/(**.h)")
target_end()
