local function module_kind(source_files)
    if #source_files == 0 then
        return "headeronly"
    end
    if get_config("mine.shared") then
        return "shared"
    end
    return "static"
end

local function module_option_enabled(module_name)
    local value = get_config("mine.module." .. module_name)
    return value == nil or value == "auto" or value == "enabled"
end

local function apply_platform_defines(public_visibility)
    if is_plat("windows") then
        add_defines("UNICODE", "_UNICODE", "NOMINMAX", "WIN32_LEAN_AND_MEAN", {
            public = public_visibility
        })
        add_cxflags("/utf-8", {tools = {"cl", "clang_cl"}})
    end
end

function mine_module(name, config)
    config = config or {}

    local short_name = config.short_name or name:match("^mine%.(.+)$") or name
    if not module_option_enabled(short_name) then
        return
    end

    local source_pattern = config.sources or "src/**.cpp"
    local test_pattern = config.tests or "test/**.cpp"
    local bench_pattern = config.benchmarks or "bench/**.cpp"
    local source_files = os.files(source_pattern)
    local test_files = os.files(test_pattern)
    local bench_files = os.files(bench_pattern)
    local kind = module_kind(source_files)

    target(name)
        set_group("modules")
        set_kind(kind)
        set_languages("cxx20")
        set_warnings("allextra")
        add_headerfiles("api/include/(**.h)")
        add_includedirs("api/include", {public = true})
        apply_platform_defines(true)

        if get_config("mine.exceptions") then
            set_exceptions("cxx")
        else
            set_exceptions("no-cxx")
        end

        if not get_config("mine.rtti") then
            if is_plat("windows") then
                add_cxxflags("/GR-")
            else
                add_cxxflags("-fno-rtti", {tools = {"clang", "gcc"}})
            end
        end

        if config.deps and #config.deps > 0 then
            add_deps(table.unpack(config.deps))
        end

        if config.options and #config.options > 0 then
            add_options(table.unpack(config.options))
        end

        if config.defines and #config.defines > 0 then
            add_defines(table.unpack(config.defines))
        end

        if kind ~= "headeronly" then
            add_files(source_pattern)
            add_defines("MINE_BUILDING_" .. name:upper():gsub("[^%w]", "_"), {private = true})
        end
    target_end()

    if #test_files > 0 then
        target(name .. ".test")
            set_group("tests")
            set_kind("binary")
            set_default(false)
            set_languages("cxx20")
            set_warnings("allextra")
            add_files(test_pattern)
            add_deps(name)
            add_packages("doctest")
            add_includedirs("api/include")
            apply_platform_defines(false)
        target_end()
    end

    if #bench_files > 0 then
        target(name .. ".bench")
            set_group("benchmarks")
            set_kind("binary")
            set_default(false)
            set_languages("cxx20")
            set_warnings("allextra")
            add_files(bench_pattern)
            add_deps(name)
            add_includedirs("api/include")
            apply_platform_defines(false)
        target_end()
    end
end