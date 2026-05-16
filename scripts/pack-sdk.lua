local outputdir = path.join(os.projectdir(), "build", "sdk")
local includedir = path.join(outputdir, "include")

os.rm(outputdir)
os.mkdir(includedir)

for _, file in ipairs({"README.md", "LICENSE", "CHANGELOG.md"}) do
    local source = path.join(os.projectdir(), file)
    if os.isfile(source) then
        os.cp(source, outputdir)
    end
end

os.cp(path.join(os.projectdir(), "src", "mine", "*", "api", "include", "*"), includedir)

print("MineFramework SDK 骨架已导出到：" .. outputdir)