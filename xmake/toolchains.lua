set_languages("cxx20")

if is_mode("debug") then
    set_symbols("debug")
    set_optimize("none")
else
    set_optimize("faster")
end

if is_plat("windows") then
    add_defines("UNICODE", "_UNICODE", "NOMINMAX", "WIN32_LEAN_AND_MEAN")
    add_cxflags("/utf-8", {tools = {"cl", "clang_cl"}})
    add_cxxflags("/Zc:__cplusplus", "/permissive-", {tools = {"cl", "clang_cl"}})
else
    add_cxxflags("-fvisibility=hidden", {tools = {"clang", "gcc"}})
end