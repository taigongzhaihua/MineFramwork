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
    -- 生成到 build/.generated/mine/gfx/d3d11/ 下以匹配 #include <mine/gfx/d3d11/ShaderBytecode.h>
    local shaderDir  = path.join(os.projectdir(), "src/mine/gfx/d3d11/shaders")
    local outDir     = path.join(os.projectdir(), "build/.generated/mine/gfx/d3d11")
    local outHeader  = path.join(outDir, "ShaderBytecode.h")

    before_build(function (target)
        import("core.base.task")
        -- 确保输出目录存在
        os.mkdir(outDir)
        local script = path.join(os.projectdir(), "scripts/build_shaders.ps1")
        os.runv("powershell", {"-ExecutionPolicy", "Bypass", "-File", script,
                                "-ShaderDir", shaderDir,
                                "-OutputHeader", outHeader})
    end)

    -- 将 build/.generated/ 加入公开 include 路径
    -- 使得 <mine/gfx/d3d11/ShaderBytecode.h> 能正确解析
    add_includedirs(path.join(os.projectdir(), "build/.generated"), {public = true})
target_end()
