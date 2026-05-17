/**
 * @file CoreTest.cpp
 * @brief mine.core 模块单元测试（doctest 框架）。
 *
 * 覆盖以下类型：
 *   - Status / StatusCode
 *   - Result<T, E>
 *   - StringView
 *   - Span<T>
 *   - TypeId
 *   - OwnedPtr<T> / make_owned / MINE_NEW / MINE_DELETE
 *   - Pimpl<T>
 *   - Variant / any_cast
 */

// doctest 框架：测试可执行文件提供 main
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <mine/core/Core.h>

using namespace mine::core;

// ══════════════════════════════════════════════════════════════════════════════
// Status
// ══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Status_DefaultConstructed_IsOk") {
    Status s;
    CHECK(s.ok());
    CHECK(static_cast<bool>(s));
    CHECK(s.code() == StatusCode::Ok);
}

TEST_CASE("Status_FromErrorCode_IsNotOk") {
    Status s{StatusCode::InvalidArg};
    CHECK(!s.ok());
    CHECK(!static_cast<bool>(s));
    CHECK(s.code() == StatusCode::InvalidArg);
}

TEST_CASE("Status_Success_Factory") {
    Status s = Status::success();
    CHECK(s.ok());
}

TEST_CASE("Status_Equality") {
    CHECK(Status{} == Status{});
    CHECK(Status{StatusCode::IoError} == Status{StatusCode::IoError});
    CHECK(Status{} != Status{StatusCode::NotFound});
    CHECK(Status{} == StatusCode::Ok);
    CHECK(Status{StatusCode::Timeout} != StatusCode::Ok);
}

// ══════════════════════════════════════════════════════════════════════════════
// Result<T>
// ══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Result_OkTag_ConstructsValue") {
    Result<int> r{ok_tag, 42};
    CHECK(r.ok());
    CHECK(*r == 42);
    CHECK(r.value() == 42);
}

TEST_CASE("Result_ErrTag_ConstructsError") {
    Result<int> r{err_tag, Status{StatusCode::NotFound}};
    CHECK(!r.ok());
    CHECK(r.error() == StatusCode::NotFound);
}

TEST_CASE("Result_MakeOk_Factory") {
    auto r = make_ok<int>(100);
    CHECK(r.ok());
    CHECK(r.value() == 100);
}

TEST_CASE("Result_MakeErr_Factory") {
    auto r = make_err<int>(StatusCode::OutOfMemory);
    CHECK(!r.ok());
    CHECK(r.error().code() == StatusCode::OutOfMemory);
}

TEST_CASE("Result_CopyConstruct") {
    auto r1 = make_ok<int>(7);
    auto r2 = r1;  // NOLINT 拷贝构造
    CHECK(r2.ok());
    CHECK(r2.value() == 7);
}

TEST_CASE("Result_MoveConstruct") {
    auto r1 = make_ok<int>(55);
    auto r2 = std::move(r1);
    CHECK(r2.ok());
    CHECK(r2.value() == 55);
}

TEST_CASE("Result_ValueOr") {
    Result<int> ok{ok_tag, 10};
    Result<int> err{err_tag, Status{StatusCode::Unknown}};
    CHECK(ok.value_or(0) == 10);
    CHECK(err.value_or(-1) == -1);
}

TEST_CASE("Result_OperatorArrow") {
    struct Point { int x{3}, y{4}; };
    Result<Point> r{ok_tag};
    CHECK(r->x == 3);
    CHECK(r->y == 4);
}

// ══════════════════════════════════════════════════════════════════════════════
// StringView
// ══════════════════════════════════════════════════════════════════════════════

TEST_CASE("StringView_DefaultEmpty") {
    StringView sv;
    CHECK(sv.empty());
    CHECK(sv.size() == 0);
    CHECK(sv.data() == nullptr);
}

TEST_CASE("StringView_FromLiteral") {
    StringView sv{"hello"};
    CHECK(sv.size() == 5);
    CHECK(!sv.empty());
    CHECK(sv[0] == 'h');
    CHECK(sv[4] == 'o');
}

TEST_CASE("StringView_FromPtrLen") {
    const char buf[] = "world";
    StringView sv{buf, 3};  // "wor"
    CHECK(sv.size() == 3);
    CHECK(sv[2] == 'r');
}

TEST_CASE("StringView_Substr") {
    StringView sv{"hello world"};
    StringView sub = sv.substr(6, 5);
    CHECK(sub == StringView{"world"});
}

TEST_CASE("StringView_Find_Char") {
    StringView sv{"abcabc"};
    CHECK(sv.find('a') == 0);
    CHECK(sv.find('a', 1) == 3);
    CHECK(sv.find('z') == StringView::npos);
}

TEST_CASE("StringView_Find_SubStr") {
    StringView sv{"hello world"};
    CHECK(sv.find("world") == 6);
    CHECK(sv.find("xyz") == StringView::npos);
}

TEST_CASE("StringView_StartsEndsWith") {
    StringView sv{"foobar"};
    CHECK(sv.starts_with("foo"));
    CHECK(!sv.starts_with("bar"));
    CHECK(sv.ends_with("bar"));
    CHECK(!sv.ends_with("foo"));
}

TEST_CASE("StringView_Equality") {
    StringView a{"abc"};
    StringView b{"abc"};
    StringView c{"xyz"};
    CHECK(a == b);
    CHECK(a != c);
    CHECK(a < c);
}

// ══════════════════════════════════════════════════════════════════════════════
// Span<T>
// ══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Span_Default_Empty") {
    Span<int> s;
    CHECK(s.empty());
    CHECK(s.size() == 0);
    CHECK(s.data() == nullptr);
}

TEST_CASE("Span_FromArray") {
    int arr[] = {1, 2, 3, 4, 5};
    Span<int> s{arr};
    CHECK(s.size() == 5);
    CHECK(s[0] == 1);
    CHECK(s[4] == 5);
}

TEST_CASE("Span_FromPtrLen") {
    int arr[] = {10, 20, 30};
    Span<int> s{arr, 2};
    CHECK(s.size() == 2);
    CHECK(s[1] == 20);
}

TEST_CASE("Span_Subspan") {
    int arr[] = {1, 2, 3, 4, 5};
    Span<int> s{arr};
    Span<int> sub = s.subspan(1, 3);
    CHECK(sub.size() == 3);
    CHECK(sub[0] == 2);
    CHECK(sub[2] == 4);
}

TEST_CASE("Span_FirstLast") {
    int arr[] = {1, 2, 3, 4, 5};
    Span<int> s{arr};
    CHECK(s.first(2)[1] == 2);
    CHECK(s.last(2)[0] == 4);
}

TEST_CASE("Span_ConvertToConst") {
    int arr[] = {7, 8};
    Span<int> s{arr};
    Span<const int> cs = s;  // 隐式转换
    CHECK(cs[0] == 7);
}

TEST_CASE("Span_Iterate") {
    int arr[] = {1, 2, 3};
    Span<int> s{arr};
    int sum = 0;
    for (int x : s) sum += x;
    CHECK(sum == 6);
}

// ══════════════════════════════════════════════════════════════════════════════
// TypeId
// ══════════════════════════════════════════════════════════════════════════════

TEST_CASE("TypeId_SameType_Equal") {
    CHECK(TypeId::of<int>() == TypeId::of<int>());
    CHECK(TypeId::of<float>() == TypeId::of<float>());
}

TEST_CASE("TypeId_DifferentTypes_NotEqual") {
    CHECK(TypeId::of<int>() != TypeId::of<float>());
    CHECK(TypeId::of<int>() != TypeId::of<double>());
    CHECK(TypeId::of<int>() != TypeId::of<unsigned int>());
}

TEST_CASE("TypeId_IgnoresCV") {
    // const/volatile 修饰符不影响 TypeId
    CHECK(TypeId::of<int>() == TypeId::of<const int>());
    CHECK(TypeId::of<int>() == TypeId::of<volatile int>());
    CHECK(TypeId::of<int>() == TypeId::of<const volatile int>());
}

TEST_CASE("TypeId_Default_Invalid") {
    TypeId id;
    CHECK(!id.valid());
}

TEST_CASE("TypeId_OfT_Valid") {
    CHECK(TypeId::of<int>().valid());
}

TEST_CASE("TypeId_Hash_ConsistentForSameType") {
    CHECK(TypeId::of<int>().hash() == TypeId::of<int>().hash());
    CHECK(TypeId::of<int>().hash() != TypeId::of<double>().hash());
}

// ══════════════════════════════════════════════════════════════════════════════
// OwnedPtr<T> / make_owned / MINE_NEW / MINE_DELETE
// ══════════════════════════════════════════════════════════════════════════════

namespace {
/// 析构计数辅助类
struct LifetimeTracker {
    int& destroyed_count;
    explicit LifetimeTracker(int& c) : destroyed_count{c} {}
    ~LifetimeTracker() noexcept { ++destroyed_count; }
};
} // anonymous namespace

TEST_CASE("OwnedPtr_Default_Null") {
    OwnedPtr<int> p;
    CHECK(!p);
    CHECK(p.get() == nullptr);
}

TEST_CASE("OwnedPtr_MakeOwned_Valid") {
    auto p = make_owned<int>(42);
    CHECK(p);
    CHECK(*p == 42);
}

TEST_CASE("OwnedPtr_Destructor_CallsDeleter") {
    int count = 0;
    {
        auto p = make_owned<LifetimeTracker>(count);
        CHECK(count == 0);
    }
    CHECK(count == 1);
}

TEST_CASE("OwnedPtr_Move_TransfersOwnership") {
    int count = 0;
    auto p1 = make_owned<LifetimeTracker>(count);
    {
        auto p2 = std::move(p1);
        CHECK(!p1);
        CHECK(p2);
        CHECK(count == 0);
    }
    CHECK(count == 1);
}

TEST_CASE("OwnedPtr_Release_NoDelete") {
    int count = 0;
    auto p = make_owned<LifetimeTracker>(count);
    LifetimeTracker* raw = p.release();
    CHECK(!p);
    CHECK(count == 0);
    // 手动清理（测试中直接 delete，正常代码用 MINE_DELETE）
    delete raw;
}

TEST_CASE("OwnedPtr_Reset_Releases") {
    int count = 0;
    auto p = make_owned<LifetimeTracker>(count);
    p.reset();
    CHECK(!p);
    CHECK(count == 1);
}

TEST_CASE("MINE_NEW_MINE_DELETE_RoundTrip") {
    int count = 0;
    LifetimeTracker* ptr = MINE_NEW(LifetimeTracker, count);
    CHECK(ptr != nullptr);
    CHECK(count == 0);
    MINE_DELETE(ptr);
    CHECK(count == 1);
}

// ══════════════════════════════════════════════════════════════════════════════
// Pimpl<T>
// ══════════════════════════════════════════════════════════════════════════════

namespace {
/// Pimpl 测试用的完整实现类型
struct TestImpl {
    int value{99};
    float factor{2.5f};
};
} // anonymous namespace

TEST_CASE("Pimpl_MakePimpl_Valid") {
    auto p = make_pimpl<TestImpl>();
    CHECK(p);
    CHECK(p->value == 99);
    CHECK(p->factor == doctest::Approx(2.5f));
}

TEST_CASE("Pimpl_WithArgs") {
    struct CtorImpl {
        int x;
        explicit CtorImpl(int v) : x{v} {}
    };
    auto p = make_pimpl<CtorImpl>(42);
    CHECK(p->x == 42);
}

TEST_CASE("Pimpl_Move") {
    auto p1 = make_pimpl<TestImpl>();
    auto p2 = std::move(p1);
    CHECK(!p1);
    CHECK(p2);
    CHECK(p2->value == 99);
}

// ══════════════════════════════════════════════════════════════════════════════
// Variant / any_cast
// ══════════════════════════════════════════════════════════════════════════════

TEST_CASE("Variant_Default_Empty") {
    Variant v;
    CHECK(!v.has_value());
    CHECK(!v);
    CHECK(!v.type_id().valid());
}

TEST_CASE("Variant_ConstructFromInt") {
    Variant v = 42;
    CHECK(v.has_value());
    CHECK(v.has<int>());
    CHECK(v.get<int>() == 42);
    CHECK(v.type_id() == TypeId::of<int>());
}

TEST_CASE("Variant_ConstructFromFloat") {
    Variant v = 3.14f;
    CHECK(v.has<float>());
    CHECK(v.get<float>() == doctest::Approx(3.14f));
    CHECK(!v.has<int>());
}

TEST_CASE("Variant_ConstructFromDouble") {
    Variant v = 2.718;
    CHECK(v.has<double>());
    CHECK(v.get<double>() == doctest::Approx(2.718));
}

TEST_CASE("Variant_ConstructFromBool") {
    Variant v = true;
    CHECK(v.has<bool>());
    CHECK(v.get<bool>() == true);
}

TEST_CASE("Variant_ConstructFromStringView") {
    StringView sv{"hello"};
    Variant v = sv;
    CHECK(v.has<StringView>());
    CHECK(v.get<StringView>() == StringView{"hello"});
}

TEST_CASE("Variant_CopyConstruct") {
    Variant v1 = 123;
    Variant v2 = v1;
    CHECK(v2.has<int>());
    CHECK(v2.get<int>() == 123);
}

TEST_CASE("Variant_MoveConstruct") {
    Variant v1 = 456;
    Variant v2 = std::move(v1);
    CHECK(!v1.has_value());  // 移动后 v1 为空
    CHECK(v2.has<int>());
    CHECK(v2.get<int>() == 456);
}

TEST_CASE("Variant_CopyAssign") {
    Variant v1 = 10;
    Variant v2 = 20;
    v2 = v1;
    CHECK(v2.get<int>() == 10);
}

TEST_CASE("Variant_MoveAssign") {
    Variant v1 = 77;
    Variant v2;
    v2 = std::move(v1);
    CHECK(!v1.has_value());
    CHECK(v2.get<int>() == 77);
}

TEST_CASE("Variant_AssignFromValue") {
    Variant v = 1;
    v = 3.14f;
    CHECK(v.has<float>());
    CHECK(!v.has<int>());
}

TEST_CASE("Variant_Reset") {
    Variant v = 42;
    v.reset();
    CHECK(!v.has_value());
}

TEST_CASE("Variant_Emplace") {
    Variant v;
    v.emplace<int>(99);
    CHECK(v.has<int>());
    CHECK(v.get<int>() == 99);
}

TEST_CASE("Variant_Swap") {
    Variant v1 = 1;
    Variant v2 = 2.0f;
    v1.swap(v2);
    CHECK(v1.has<float>());
    CHECK(v2.has<int>());
}

TEST_CASE("Variant_AnyCast_Success") {
    Variant v = 100;
    CHECK(any_cast<int>(v) == 100);
}

TEST_CASE("Variant_AnyCast_Pointer_NotFound") {
    Variant v = 3.14f;
    CHECK(any_cast<int>(&v) == nullptr);
    CHECK(any_cast<float>(&v) != nullptr);
}

TEST_CASE("Variant_LargeType_HeapAllocated") {
    // 超过 SBO 阈值（16 字节）的类型必须走堆路径
    struct BigStruct {
        int a, b, c, d, e;  // 20 字节，超过 SBO 的 16 字节阈值
    };
    BigStruct bs{1, 2, 3, 4, 5};
    Variant v = bs;
    CHECK(v.has<BigStruct>());
    CHECK(v.get<BigStruct>().c == 3);

    // 拷贝应深拷贝
    Variant v2 = v;
    CHECK(v2.get<BigStruct>().e == 5);
}

TEST_CASE("Variant_Destructor_FreesHeapObject") {
    // BigTracker 超过 SBO 阈值（16 字节），强制走堆分配路径
    struct BigTracker {
        int* count;
        char padding[20];  ///< 总大小 = sizeof(int*) + 20 > 16 字节
        explicit BigTracker(int* c) : count{c}, padding{} {}
        ~BigTracker() noexcept { ++(*count); }
        // 拷贝构造：两个实例共享同一个计数器指针
        BigTracker(const BigTracker& o) : count{o.count}, padding{} {}
    };
    int count = 0;
    {
        BigTracker src{&count};  // 栈上原始对象
        {
            Variant v{src};      // 拷贝到 Variant（堆路径分配新对象）
            CHECK(count == 0);   // 原始 src 仍存活，Variant 内对象也存活
        }
        // v 析构：Variant 内堆上的 BigTracker 被释放 → count == 1
        CHECK(count == 1);
    }
    // src 析构 → count == 2
    CHECK(count == 2);
}
