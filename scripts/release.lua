local changelog = path.join(os.projectdir(), "CHANGELOG.md")
if not os.isfile(changelog) then
    raise("缺少 CHANGELOG.md，无法执行发布预检查。")
end

local xmakefile = io.readfile(path.join(os.projectdir(), "xmake.lua"))
local version = xmakefile and xmakefile:match("set_version%(%\"([^\"]+)\"%)") or "0.0.0"

print("MineFramework 预发布检查通过，当前版本：" .. version)
print("如需导出 SDK 骨架，请执行 scripts/pack-sdk.lua。")