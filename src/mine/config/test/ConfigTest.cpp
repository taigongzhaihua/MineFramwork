/**
 * @file ConfigTest.cpp
 * @brief mine.config 模块单元测试。
 *
 * 覆盖：
 *   - ConfigValue：Null/Bool/Integer/Float/String 五种类型的构造、访问
 *   - ConfigValue：拷贝/移动语义（字符串 SSO 和堆分配）
 *   - ConfigValue：to_xxx() 类型转换（带回退值）
 *   - ConfigManager：set_default / get / has / which_layer
 *   - ConfigManager：load_string（JSON 简单键值）
 *   - ConfigManager：load_string（JSON 嵌套对象展平）
 *   - ConfigManager：load_string（JSON 数组跳过）
 *   - ConfigManager：load_string（TOML 简单键值）
 *   - ConfigManager：load_string（TOML 节头）
 *   - ConfigManager：load_string（TOML 注释和字面字符串）
 *   - ConfigManager：配置层优先级（File > Default）
 *   - ConfigManager：clear_layer / clear_all
 *   - ConfigManager：get_bool/get_integer/get_float/get_string 带回退
 *   - ConfigManager：无效 JSON 返回 ParseError
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <mine/config/Config.h>

using namespace mine::config;
using mine::core::StringView;
using mine::core::StatusCode;

// ─────────────────────────────────────────────────────────────────────────────
// ConfigValue 单元测试
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("ConfigValue") {

    TEST_CASE("默认构造为 Null 类型") {
        ConfigValue v;
        CHECK(v.is_null());
        CHECK(v.type() == ConfigValueType::Null);
        CHECK(!v.is_bool());
        CHECK(!v.is_integer());
        CHECK(!v.is_float());
        CHECK(!v.is_string());
    }

    TEST_CASE("Bool 类型构造与访问") {
        ConfigValue vt{true};
        CHECK(vt.is_bool());
        CHECK(vt.as_bool() == true);

        ConfigValue vf{false};
        CHECK(vf.is_bool());
        CHECK(vf.as_bool() == false);
    }

    TEST_CASE("Integer 类型构造与访问") {
        ConfigValue v{int64_t(42)};
        CHECK(v.is_integer());
        CHECK(v.as_integer() == 42);

        ConfigValue vneg{int64_t(-9999)};
        CHECK(vneg.as_integer() == -9999);

        // int → int64_t 提升
        ConfigValue vi{100};
        CHECK(vi.is_integer());
        CHECK(vi.as_integer() == 100);
    }

    TEST_CASE("Float 类型构造与访问") {
        ConfigValue v{3.14};
        CHECK(v.is_float());
        CHECK(v.as_float() == doctest::Approx(3.14));

        ConfigValue vn{-1.5};
        CHECK(vn.as_float() == doctest::Approx(-1.5));
    }

    TEST_CASE("String 类型构造与访问（SSO，短字符串）") {
        ConfigValue v{StringView{"hello"}};
        CHECK(v.is_string());
        CHECK(v.as_string() == "hello");
    }

    TEST_CASE("String 类型构造与访问（堆分配，超过 23 字节）") {
        // InlineString SSO 缓冲区最多存 23 字节
        StringView long_str{"this_is_a_very_long_config_key_value"};
        ConfigValue v{long_str};
        CHECK(v.is_string());
        CHECK(v.as_string() == long_str);
    }

    TEST_CASE("String 从 const char* 隐式构造") {
        ConfigValue v{"world"};
        CHECK(v.is_string());
        CHECK(v.as_string() == "world");
    }

    TEST_CASE("ConfigValue 拷贝构造") {
        ConfigValue orig{StringView{"copy me"}};
        ConfigValue copy{orig};
        CHECK(copy.is_string());
        CHECK(copy.as_string() == "copy me");
        // 原始对象不受影响
        CHECK(orig.is_string());
        CHECK(orig.as_string() == "copy me");
    }

    TEST_CASE("ConfigValue 移动构造") {
        ConfigValue orig{StringView{"move me"}};
        ConfigValue moved{std::move(orig)};
        CHECK(moved.is_string());
        CHECK(moved.as_string() == "move me");
        // 移动后原始对象变为 Null
        CHECK(orig.is_null());
    }

    TEST_CASE("ConfigValue 拷贝赋值") {
        ConfigValue a{int64_t(10)};
        ConfigValue b{StringView{"text"}};
        a = b;
        CHECK(a.is_string());
        CHECK(a.as_string() == "text");
    }

    TEST_CASE("ConfigValue 移动赋值") {
        ConfigValue a{int64_t(10)};
        ConfigValue b{StringView{"moved text"}};
        a = std::move(b);
        CHECK(a.is_string());
        CHECK(a.as_string() == "moved text");
        CHECK(b.is_null());
    }

    TEST_CASE("ConfigValue 等值比较") {
        CHECK(ConfigValue{}        == ConfigValue{});
        CHECK(ConfigValue{true}    == ConfigValue{true});
        CHECK(ConfigValue{true}    != ConfigValue{false});
        CHECK(ConfigValue{int64_t(1)} == ConfigValue{int64_t(1)});
        CHECK(ConfigValue{3.14}    == ConfigValue{3.14});
        CHECK(ConfigValue{"abc"}   == ConfigValue{StringView{"abc"}});
        CHECK(ConfigValue{"abc"}   != ConfigValue{StringView{"xyz"}});
        CHECK(ConfigValue{true}    != ConfigValue{int64_t(1)});  // 类型不同
    }

    // ── to_bool 转换测试 ──────────────────────────────────────────────

    TEST_CASE("to_bool：各类型转换") {
        // Bool → 直接返回
        CHECK(ConfigValue{true}.to_bool()  == true);
        CHECK(ConfigValue{false}.to_bool() == false);

        // Integer → 非零为 true
        CHECK(ConfigValue{int64_t(1)}.to_bool()  == true);
        CHECK(ConfigValue{int64_t(0)}.to_bool()  == false);
        CHECK(ConfigValue{int64_t(-1)}.to_bool() == true);

        // Float → 非零为 true
        CHECK(ConfigValue{1.0}.to_bool()  == true);
        CHECK(ConfigValue{0.0}.to_bool()  == false);

        // String → "true"/"1" / "false"/"0"
        CHECK(ConfigValue{"true"}.to_bool()  == true);
        CHECK(ConfigValue{"1"}.to_bool()     == true);
        CHECK(ConfigValue{"false"}.to_bool() == false);
        CHECK(ConfigValue{"0"}.to_bool()     == false);
        CHECK(ConfigValue{"other"}.to_bool(true)  == true);   // 无法识别 → fallback

        // Null → fallback
        CHECK(ConfigValue{}.to_bool()       == false);  // fallback 默认 false
        CHECK(ConfigValue{}.to_bool(true)   == true);
    }

    TEST_CASE("to_integer：各类型转换") {
        CHECK(ConfigValue{int64_t(42)}.to_integer()  == 42);
        CHECK(ConfigValue{true}.to_integer()          == 1);
        CHECK(ConfigValue{false}.to_integer()         == 0);
        CHECK(ConfigValue{3.9}.to_integer()           == 3);   // 截断
        CHECK(ConfigValue{"123"}.to_integer()         == 123);
        CHECK(ConfigValue{"-7"}.to_integer()          == -7);
        CHECK(ConfigValue{"abc"}.to_integer(99)       == 99);  // 无法解析 → fallback
        CHECK(ConfigValue{}.to_integer(10)            == 10);  // Null → fallback
    }

    TEST_CASE("to_float：各类型转换") {
        CHECK(ConfigValue{2.5}.to_float()             == doctest::Approx(2.5));
        CHECK(ConfigValue{int64_t(3)}.to_float()      == doctest::Approx(3.0));
        CHECK(ConfigValue{true}.to_float()            == doctest::Approx(1.0));
        CHECK(ConfigValue{false}.to_float()           == doctest::Approx(0.0));
        CHECK(ConfigValue{"3.14"}.to_float()          == doctest::Approx(3.14));
        CHECK(ConfigValue{"bad"}.to_float(9.9)        == doctest::Approx(9.9));
        CHECK(ConfigValue{}.to_float(1.1)             == doctest::Approx(1.1));
    }

    TEST_CASE("to_string：仅 String 类型有效，其他返回 fallback") {
        CHECK(ConfigValue{"abc"}.to_string() == "abc");
        CHECK(ConfigValue{}.to_string("fb")             == "fb");
        CHECK(ConfigValue{int64_t(1)}.to_string("fb")   == "fb");
        CHECK(ConfigValue{true}.to_string("fb")         == "fb");
    }

} // TEST_SUITE("ConfigValue")

// ─────────────────────────────────────────────────────────────────────────────
// ConfigManager 单元测试
// ─────────────────────────────────────────────────────────────────────────────

TEST_SUITE("ConfigManager") {

    TEST_CASE("默认值层：set_default 和 get") {
        ConfigManager mgr;
        mgr.set_default("app.width",  ConfigValue{int64_t(1280)});
        mgr.set_default("app.height", ConfigValue{int64_t(720)});
        mgr.set_default("app.title",  ConfigValue{"My App"});
        mgr.set_default("app.debug",  ConfigValue{false});

        CHECK(mgr.get_integer("app.width")  == 1280);
        CHECK(mgr.get_integer("app.height") == 720);
        CHECK(mgr.get_string("app.title")   == "My App");
        CHECK(mgr.get_bool("app.debug")     == false);
        // 不存在的键返回 Null
        CHECK(mgr.get("nonexistent").is_null());
    }

    TEST_CASE("has：键存在性检测") {
        ConfigManager mgr;
        CHECK(!mgr.has("x"));

        mgr.set_default("x", ConfigValue{int64_t(1)});
        CHECK(mgr.has("x"));
    }

    TEST_CASE("which_layer：来源层查询") {
        ConfigManager mgr;
        // 键不存在时返回 Default（但 has() 为 false）
        CHECK(mgr.which_layer("z") == ConfigLayer::Default);

        mgr.set_default("z", ConfigValue{int64_t(0)});
        CHECK(mgr.which_layer("z") == ConfigLayer::Default);

        mgr.load_string(R"({"z": 1})", ConfigFormat::Json);
        CHECK(mgr.which_layer("z") == ConfigLayer::File);  // File 优先于 Default
    }

    // ── JSON 解析 ────────────────────────────────────────────────────────

    TEST_CASE("JSON：简单键值对") {
        ConfigManager mgr;
        auto st = mgr.load_string(
            R"({"count": 42, "ratio": 1.5, "active": true, "name": "test", "empty": null})",
            ConfigFormat::Json);

        CHECK(st.ok());
        CHECK(mgr.get_integer("count")  == 42);
        CHECK(mgr.get_float("ratio")    == doctest::Approx(1.5));
        CHECK(mgr.get_bool("active")    == true);
        CHECK(mgr.get_string("name")    == "test");
        CHECK(mgr.has("empty"));
        CHECK(mgr.get("empty").is_null());
    }

    TEST_CASE("JSON：嵌套对象展平为点分隔键") {
        ConfigManager mgr;
        auto st = mgr.load_string(
            R"({"window": {"width": 800, "height": 600}, "app": {"theme": "dark"}})",
            ConfigFormat::Json);

        CHECK(st.ok());
        CHECK(mgr.get_integer("window.width")  == 800);
        CHECK(mgr.get_integer("window.height") == 600);
        CHECK(mgr.get_string("app.theme")      == "dark");
    }

    TEST_CASE("JSON：深层嵌套展平") {
        ConfigManager mgr;
        auto st = mgr.load_string(
            R"({"a": {"b": {"c": 999}}})",
            ConfigFormat::Json);

        CHECK(st.ok());
        CHECK(mgr.get_integer("a.b.c") == 999);
    }

    TEST_CASE("JSON：数组跳过（不报错）") {
        ConfigManager mgr;
        // 包含数组的 JSON 应该跳过数组项，不影响其他键
        auto st = mgr.load_string(
            R"({"items": [1, 2, 3], "count": 5})",
            ConfigFormat::Json);

        CHECK(st.ok());
        CHECK(!mgr.has("items"));  // 数组被跳过
        CHECK(mgr.get_integer("count") == 5);
    }

    TEST_CASE("JSON：字符串转义处理") {
        ConfigManager mgr;
        auto st = mgr.load_string(
            R"({"msg": "hello\nworld", "path": "C:\\Users\\test"})",
            ConfigFormat::Json);

        CHECK(st.ok());
        auto msg = mgr.get_string("msg");
        // 确认 \n 已被解码（字符串含换行符）
        CHECK(msg.size() == 11);  // "hello" + '\n' + "world"
    }

    TEST_CASE("JSON：负数和浮点数解析") {
        ConfigManager mgr;
        auto st = mgr.load_string(
            R"({"temp": -273, "pi": 3.14159, "exp": 1.5e3})",
            ConfigFormat::Json);

        CHECK(st.ok());
        CHECK(mgr.get_integer("temp") == -273);
        CHECK(mgr.get_float("pi")     == doctest::Approx(3.14159));
        CHECK(mgr.get_float("exp")    == doctest::Approx(1500.0));
    }

    TEST_CASE("JSON：空对象解析成功") {
        ConfigManager mgr;
        auto st = mgr.load_string("{}", ConfigFormat::Json);
        CHECK(st.ok());
    }

    TEST_CASE("JSON：格式错误返回 ParseError") {
        ConfigManager mgr;
        auto st = mgr.load_string("not json at all", ConfigFormat::Json);
        CHECK(!st.ok());
        CHECK(st.code() == StatusCode::ParseError);
    }

    // ── TOML 解析 ────────────────────────────────────────────────────────

    TEST_CASE("TOML：简单键值对") {
        ConfigManager mgr;
        const char* toml =
            "count = 100\n"
            "ratio = 2.5\n"
            "active = true\n"
            "name = \"app\"\n";

        auto st = mgr.load_string(toml, ConfigFormat::Toml);
        CHECK(st.ok());
        CHECK(mgr.get_integer("count") == 100);
        CHECK(mgr.get_float("ratio")   == doctest::Approx(2.5));
        CHECK(mgr.get_bool("active")   == true);
        CHECK(mgr.get_string("name")   == "app");
    }

    TEST_CASE("TOML：节头作为键前缀") {
        ConfigManager mgr;
        const char* toml =
            "[window]\n"
            "width = 1280\n"
            "height = 720\n"
            "\n"
            "[app]\n"
            "name = \"my-app\"\n"
            "version = 3\n";

        auto st = mgr.load_string(toml, ConfigFormat::Toml);
        CHECK(st.ok());
        CHECK(mgr.get_integer("window.width")  == 1280);
        CHECK(mgr.get_integer("window.height") == 720);
        CHECK(mgr.get_string("app.name")       == "my-app");
        CHECK(mgr.get_integer("app.version")   == 3);
    }

    TEST_CASE("TOML：注释行被忽略") {
        ConfigManager mgr;
        const char* toml =
            "# 这是注释\n"
            "x = 1  # 行尾注释\n"
            "# 另一行注释\n"
            "y = 2\n";

        auto st = mgr.load_string(toml, ConfigFormat::Toml);
        CHECK(st.ok());
        CHECK(mgr.get_integer("x") == 1);
        CHECK(mgr.get_integer("y") == 2);
    }

    TEST_CASE("TOML：字面字符串（单引号，不处理转义）") {
        ConfigManager mgr;
        const char* toml = "path = 'C:\\Users\\test'\n";

        auto st = mgr.load_string(toml, ConfigFormat::Toml);
        CHECK(st.ok());
        // 字面字符串不处理 \，所以 \U 和 \t 保持原样
        auto path = mgr.get_string("path");
        CHECK(path == R"(C:\Users\test)");
    }

    TEST_CASE("TOML：负数和浮点数") {
        ConfigManager mgr;
        const char* toml =
            "temp = -40\n"
            "gravity = -9.81\n"
            "pi = 3.14159\n";

        auto st = mgr.load_string(toml, ConfigFormat::Toml);
        CHECK(st.ok());
        CHECK(mgr.get_integer("temp")  == -40);
        CHECK(mgr.get_float("gravity") == doctest::Approx(-9.81));
        CHECK(mgr.get_float("pi")      == doctest::Approx(3.14159));
    }

    TEST_CASE("TOML：布尔值 true/false") {
        ConfigManager mgr;
        const char* toml =
            "enabled = true\n"
            "disabled = false\n";

        auto st = mgr.load_string(toml, ConfigFormat::Toml);
        CHECK(st.ok());
        CHECK(mgr.get_bool("enabled")  == true);
        CHECK(mgr.get_bool("disabled") == false);
    }

    // ── 配置层优先级 ─────────────────────────────────────────────────────

    TEST_CASE("优先级：File 层覆盖 Default 层") {
        ConfigManager mgr;

        mgr.set_default("key", ConfigValue{"default_val"});
        CHECK(mgr.get_string("key") == "default_val");

        // File 层覆盖
        mgr.load_string(R"({"key": "file_val"})", ConfigFormat::Json);
        CHECK(mgr.get_string("key") == "file_val");
        CHECK(mgr.which_layer("key") == ConfigLayer::File);
    }

    TEST_CASE("优先级：Default 层在 File 层之后回退") {
        ConfigManager mgr;

        mgr.set_default("a", ConfigValue{int64_t(1)});
        mgr.set_default("b", ConfigValue{int64_t(2)});
        mgr.load_string(R"({"a": 10})", ConfigFormat::Json);

        // "a" 来自 File 层
        CHECK(mgr.get_integer("a") == 10);
        // "b" 仍来自 Default 层（File 层没有 "b"）
        CHECK(mgr.get_integer("b") == 2);
        CHECK(mgr.which_layer("b") == ConfigLayer::Default);
    }

    // ── 层管理 ───────────────────────────────────────────────────────────

    TEST_CASE("clear_layer：清除 File 层后回退 Default") {
        ConfigManager mgr;
        mgr.set_default("x", ConfigValue{int64_t(1)});
        mgr.load_string(R"({"x": 2})", ConfigFormat::Json);

        CHECK(mgr.get_integer("x") == 2);

        mgr.clear_layer(ConfigLayer::File);
        CHECK(mgr.get_integer("x") == 1);  // 回退 Default
    }

    TEST_CASE("clear_layer：清除 Default 层") {
        ConfigManager mgr;
        mgr.set_default("x", ConfigValue{int64_t(1)});

        CHECK(mgr.has("x"));
        mgr.clear_layer(ConfigLayer::Default);
        CHECK(!mgr.has("x"));
    }

    TEST_CASE("clear_all：清除所有层") {
        ConfigManager mgr;
        mgr.set_default("a", ConfigValue{int64_t(1)});
        mgr.load_string(R"({"b": 2})", ConfigFormat::Json);

        CHECK(mgr.has("a"));
        CHECK(mgr.has("b"));

        mgr.clear_all();
        CHECK(!mgr.has("a"));
        CHECK(!mgr.has("b"));
    }

    // ── 便捷读取方法（带回退值） ─────────────────────────────────────────

    TEST_CASE("get_xxx 便捷读取：键不存在时返回 fallback") {
        ConfigManager mgr;
        mgr.load_string(
            R"({"count": 42, "ratio": 0.5, "enabled": false})",
            ConfigFormat::Json);

        // 已存在的键
        CHECK(mgr.get_integer("count",   0)    == 42);
        CHECK(mgr.get_float("ratio",     0.0)  == doctest::Approx(0.5));
        CHECK(mgr.get_bool("enabled",   true)  == false);

        // 不存在的键 → 返回 fallback
        CHECK(mgr.get_integer("missing", 99)   == 99);
        CHECK(mgr.get_float("missing",   3.14) == doctest::Approx(3.14));
        CHECK(mgr.get_bool("missing",   true)  == true);
        CHECK(mgr.get_string("missing",  "fb") == "fb");
    }

    TEST_CASE("多次 load_string 合并到 File 层") {
        ConfigManager mgr;
        mgr.load_string(R"({"a": 1})", ConfigFormat::Json);
        mgr.load_string(R"({"b": 2})", ConfigFormat::Json);

        CHECK(mgr.get_integer("a") == 1);
        CHECK(mgr.get_integer("b") == 2);
    }

    TEST_CASE("相同键多次写入使用最后值（insert_or_assign）") {
        ConfigManager mgr;
        mgr.set_default("x", ConfigValue{int64_t(1)});
        mgr.set_default("x", ConfigValue{int64_t(99)});  // 覆盖

        CHECK(mgr.get_integer("x") == 99);
    }

} // TEST_SUITE("ConfigManager")
