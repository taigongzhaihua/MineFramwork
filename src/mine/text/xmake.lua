mine_module("mine.text", {
    short_name  = "text",
    deps        = {"mine.core", "mine.containers"},
    packages    = {"freetype"},
})

-- HarfBuzz：使用本地预装版本（绕过 xmake-repo 网络下载）
target("mine.text")
    add_includedirs(path.join(
        os.getenv("LOCALAPPDATA") or "",
        ".xmake/packages/h/harfbuzz/8.5.0/a55c189be5824e40849bde9f19288c10/include/harfbuzz"
    ))
    add_linkdirs(path.join(
        os.getenv("LOCALAPPDATA") or "",
        ".xmake/packages/h/harfbuzz/8.5.0/a55c189be5824e40849bde9f19288c10/lib"
    ))
    add_links("harfbuzz")