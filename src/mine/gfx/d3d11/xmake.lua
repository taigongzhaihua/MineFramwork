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
        "d3dcompiler"   -- HLSL 运行时编译（保留作为回退，主路径使用预编译字节码）
    )
    add_includedirs("api/include", {public = true})
    add_headerfiles("api/include/(**.h)")

    -- 预编译 HLSL 着色器 → ShaderBytecode.h
    local shaderDir  = path.join(os.projectdir(), "src/mine/gfx/d3d11/shaders")
    local genDir     = path.join(os.projectdir(), "build/.generated/mine.gfx.d3d11")
    local outHeader  = path.join(genDir, "ShaderBytecode.h")

    before_build(function (target)
        local script = path.join(os.projectdir(), "scripts/build_shaders.ps1")
        os.runv("powershell", {"-ExecutionPolicy", "Bypass", "-File", script,
                                "-ShaderDir", shaderDir,
                                "-OutputHeader", outHeader})
    end)

    -- 将生成目录加入 include 路径
    add_includedirs(genDir, {public = false})
target_end()
