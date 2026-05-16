local doc_files = os.files(path.join(os.projectdir(), "docs", "*.md"))
local outputdir = path.join(os.projectdir(), "build", "docs")
local outputfile = path.join(outputdir, "index.md")
local lines = {
    "# 文档索引",
    ""
}

table.sort(doc_files)
for _, file in ipairs(doc_files) do
    table.insert(lines, "- " .. path.filename(file))
end

os.mkdir(outputdir)
io.writefile(outputfile, table.concat(lines, "\n"))

print("文档索引已生成：" .. outputfile)