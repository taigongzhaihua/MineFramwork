/**
 * @file ContainersTest.cpp
 * @brief mine.containers 模块单元测试（doctest）
 *
 * 测试覆盖：
 *   - Vector<T>：构造/析构/push_back/emplace_back/insert/erase/resize/reserve/
 *               swap/拷贝/移动/平凡与非平凡类型/迭代器/Span 转换
 *   - SmallVector<T,N>：内联与堆路径/push_back/shrink_to_fit/移动/is_small
 *   - HashMap<K,V>：insert/find/contains/erase/operator[]/rehash/迭代/移动
 *   - InlineString：SSO/push_back/append/运算符/比较/substr/find/hash
 *   - IntrusiveList<T>：push_back/push_front/pop/insert/erase/unlink/移动
 *   - Hash<T>：整数/指针/StringView/InlineString 哈希
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <mine/containers/Containers.h>

using namespace mine::containers;
using mine::core::StringView;

// ─────────────────────────────────────────────────────────────────────────────
// 工具：追踪构造/析构次数的非平凡类型
// ─────────────────────────────────────────────────────────────────────────────

struct Tracker {
    int  value = 0;
    int* dtor_count = nullptr;

    Tracker() = default;
    explicit Tracker(int v, int* cnt = nullptr) : value{v}, dtor_count{cnt} {}

    Tracker(const Tracker& o) : value{o.value}, dtor_count{o.dtor_count} {}
    Tracker(Tracker&& o) noexcept : value{o.value}, dtor_count{o.dtor_count} {
        o.value = -1;
    }
    Tracker& operator=(const Tracker& o) {
        value = o.value;
        dtor_count = o.dtor_count;
        return *this;
    }
    Tracker& operator=(Tracker&& o) noexcept {
        value = o.value;
        dtor_count = o.dtor_count;
        o.value = -1;
        return *this;
    }
    ~Tracker() noexcept {
        if (dtor_count) ++(*dtor_count);
    }
    bool operator==(const Tracker& o) const noexcept { return value == o.value; }
    bool operator!=(const Tracker& o) const noexcept { return value != o.value; }
};

// ─────────────────────────────────────────────────────────────────────────────
// Vector<T> — 基础功能
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Vector_DefaultConstruct") {
    Vector<int> v;
    CHECK(v.empty());
    CHECK(v.size() == 0);
    CHECK(v.capacity() == 0);
    CHECK(v.data() == nullptr);
}

TEST_CASE("Vector_PushBack_Int") {
    Vector<int> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    CHECK(v.size() == 3);
    CHECK(v[0] == 1);
    CHECK(v[1] == 2);
    CHECK(v[2] == 3);
}

TEST_CASE("Vector_PopBack") {
    Vector<int> v{1, 2, 3};
    v.pop_back();
    CHECK(v.size() == 2);
    CHECK(v.back() == 2);
}

TEST_CASE("Vector_EmplaceBack") {
    Vector<Tracker> v;
    int cnt = 0;
    v.emplace_back(10, &cnt);
    v.emplace_back(20, &cnt);
    CHECK(v.size() == 2);
    CHECK(v[0].value == 10);
    CHECK(v[1].value == 20);
}

TEST_CASE("Vector_Destructor_CallsDtor") {
    int cnt = 0;
    {
        Vector<Tracker> v;
        v.emplace_back(1, &cnt);
        v.emplace_back(2, &cnt);
    }
    CHECK(cnt == 2);
}

TEST_CASE("Vector_Reserve_And_Capacity") {
    Vector<int> v;
    v.reserve(100);
    CHECK(v.capacity() >= 100);
    CHECK(v.size() == 0);
    CHECK(v.empty());
}

TEST_CASE("Vector_Resize_DefaultInit") {
    Vector<int> v;
    v.resize(5);
    CHECK(v.size() == 5);
    for (int x : v) CHECK(x == 0);
}

TEST_CASE("Vector_Resize_WithValue") {
    Vector<int> v;
    v.resize(4, 42);
    CHECK(v.size() == 4);
    for (int x : v) CHECK(x == 42);
}

TEST_CASE("Vector_Resize_Shrink") {
    int cnt = 0;
    Vector<Tracker> v;
    v.emplace_back(1, &cnt);
    v.emplace_back(2, &cnt);
    v.emplace_back(3, &cnt);
    v.resize(1);
    CHECK(v.size() == 1);
    CHECK(cnt == 2);  // 第 2、3 个元素析构
}

TEST_CASE("Vector_Insert_At_Begin") {
    Vector<int> v{2, 3, 4};
    v.insert(v.begin(), 1);
    CHECK(v.size() == 4);
    CHECK(v[0] == 1);
    CHECK(v[1] == 2);
}

TEST_CASE("Vector_Insert_Count") {
    Vector<int> v{1, 4};
    v.insert(v.begin() + 1, 2, 99);
    CHECK(v.size() == 4);
    CHECK(v[1] == 99);
    CHECK(v[2] == 99);
    CHECK(v[3] == 4);
}

TEST_CASE("Vector_Erase_Single") {
    Vector<int> v{10, 20, 30};
    auto it = v.erase(v.begin() + 1);
    CHECK(v.size() == 2);
    CHECK(v[0] == 10);
    CHECK(v[1] == 30);
    CHECK(*it == 30);
}

TEST_CASE("Vector_Erase_Range") {
    Vector<int> v{1, 2, 3, 4, 5};
    v.erase(v.begin() + 1, v.begin() + 4);
    CHECK(v.size() == 2);
    CHECK(v[0] == 1);
    CHECK(v[1] == 5);
}

TEST_CASE("Vector_Clear") {
    int cnt = 0;
    Vector<Tracker> v;
    v.emplace_back(1, &cnt);
    v.emplace_back(2, &cnt);
    v.clear();
    CHECK(v.empty());
    CHECK(cnt == 2);
    CHECK(v.capacity() > 0);  // 清空不释放内存
}

TEST_CASE("Vector_ShrinkToFit") {
    Vector<int> v;
    v.reserve(100);
    v.push_back(1);
    v.push_back(2);
    v.shrink_to_fit();
    CHECK(v.size() == 2);
    CHECK(v.capacity() == 2);
}

TEST_CASE("Vector_CopyConstruct") {
    Vector<int> a{1, 2, 3};
    Vector<int> b{a};
    CHECK(b.size() == 3);
    CHECK(b[0] == 1);
    // 修改 a 不影响 b
    a[0] = 99;
    CHECK(b[0] == 1);
}

TEST_CASE("Vector_MoveConstruct") {
    Vector<int> a{1, 2, 3};
    int* old_data = a.data();
    Vector<int> b{std::move(a)};
    CHECK(b.size() == 3);
    CHECK(b.data() == old_data);
    CHECK(a.empty());
}

TEST_CASE("Vector_CopyAssign") {
    Vector<int> a{1, 2, 3};
    Vector<int> b{4, 5};
    b = a;
    CHECK(b.size() == 3);
    CHECK(b[2] == 3);
}

TEST_CASE("Vector_MoveAssign") {
    Vector<int> a{1, 2, 3};
    Vector<int> b;
    b = std::move(a);
    CHECK(b.size() == 3);
    CHECK(a.empty());
}

TEST_CASE("Vector_Swap") {
    Vector<int> a{1, 2};
    Vector<int> b{3, 4, 5};
    a.swap(b);
    CHECK(a.size() == 3);
    CHECK(b.size() == 2);
    CHECK(a[0] == 3);
    CHECK(b[0] == 1);
}

TEST_CASE("Vector_InitializerList") {
    Vector<int> v = {10, 20, 30};
    CHECK(v.size() == 3);
    CHECK(v[1] == 20);
}

TEST_CASE("Vector_SpanConversion") {
    Vector<int> v{1, 2, 3};
    mine::core::Span<int> s = v;
    CHECK(s.size() == 3);
    CHECK(s[0] == 1);
}

TEST_CASE("Vector_Equality") {
    Vector<int> a{1, 2, 3};
    Vector<int> b{1, 2, 3};
    Vector<int> c{1, 2, 4};
    CHECK(a == b);
    CHECK(a != c);
}

TEST_CASE("Vector_Iterator") {
    Vector<int> v{5, 10, 15};
    int sum = 0;
    for (auto it = v.begin(); it != v.end(); ++it) sum += *it;
    CHECK(sum == 30);
}

TEST_CASE("Vector_NonTrivial_Insert_Erase") {
    int cnt = 0;
    Vector<Tracker> v;
    v.emplace_back(1, &cnt);
    v.emplace_back(3, &cnt);
    v.insert(v.begin() + 1, Tracker{2, &cnt});
    CHECK(v.size() == 3);
    CHECK(v[0].value == 1);
    CHECK(v[1].value == 2);
    CHECK(v[2].value == 3);
    v.erase(v.begin() + 1);
    CHECK(v.size() == 2);
}

// ─────────────────────────────────────────────────────────────────────────────
// SmallVector<T, N>
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("SmallVector_InlineStorage") {
    SmallVector<int, 4> v;
    v.push_back(1);
    v.push_back(2);
    CHECK(v.is_small());
    CHECK(v.size() == 2);
    CHECK(v[0] == 1);
}

TEST_CASE("SmallVector_HeapFallback") {
    SmallVector<int, 4> v;
    for (int i = 0; i < 10; ++i) v.push_back(i);
    CHECK(!v.is_small());
    CHECK(v.size() == 10);
    CHECK(v[5] == 5);
}

TEST_CASE("SmallVector_MoveFromInline") {
    SmallVector<int, 4> a{1, 2, 3};
    CHECK(a.is_small());
    SmallVector<int, 4> b{std::move(a)};
    CHECK(b.size() == 3);
    CHECK(b[0] == 1);
    CHECK(a.empty());
}

TEST_CASE("SmallVector_MoveFromHeap") {
    SmallVector<int, 2> a{1, 2, 3, 4, 5};
    CHECK(!a.is_small());
    SmallVector<int, 2> b{std::move(a)};
    CHECK(b.size() == 5);
    CHECK(a.empty());
}

TEST_CASE("SmallVector_ShrinkToFit_BackToInline") {
    SmallVector<int, 8> v;
    for (int i = 0; i < 10; ++i) v.push_back(i);
    CHECK(!v.is_small());
    v.resize(4);
    v.shrink_to_fit();
    CHECK(v.is_small());
    CHECK(v.size() == 4);
}

TEST_CASE("SmallVector_Destructor_Dtor") {
    int cnt = 0;
    {
        SmallVector<Tracker, 4> v;
        v.emplace_back(1, &cnt);
        v.emplace_back(2, &cnt);
    }
    CHECK(cnt == 2);
}

TEST_CASE("SmallVector_InitializerList") {
    SmallVector<int, 8> v = {5, 10, 15};
    CHECK(v.size() == 3);
    CHECK(v[2] == 15);
    CHECK(v.is_small());
}

TEST_CASE("SmallVector_SpanConversion") {
    SmallVector<int, 4> v{1, 2, 3};
    mine::core::Span<const int> s = v;
    CHECK(s.size() == 3);
    CHECK(s[2] == 3);
}

// ─────────────────────────────────────────────────────────────────────────────
// HashMap<K, V>
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("HashMap_DefaultConstruct") {
    HashMap<int, int> m;
    CHECK(m.empty());
    CHECK(m.size() == 0);
}

TEST_CASE("HashMap_Insert_And_Find") {
    HashMap<int, int> m;
    auto [ptr, ins] = m.insert(1, 100);
    CHECK(ins);
    CHECK(*ptr == 100);
    CHECK(m.size() == 1);
    CHECK(m.contains(1));

    int* found = m.find(1);
    REQUIRE(found != nullptr);
    CHECK(*found == 100);
}

TEST_CASE("HashMap_Insert_DuplicateKey") {
    HashMap<int, int> m;
    m.insert(1, 100);
    auto [ptr, ins] = m.insert(1, 200);
    CHECK(!ins);
    CHECK(*ptr == 100);  // 原始值保持不变
    CHECK(m.size() == 1);
}

TEST_CASE("HashMap_Find_Missing") {
    HashMap<int, int> m;
    CHECK(m.find(99) == nullptr);
}

TEST_CASE("HashMap_Contains") {
    HashMap<int, int> m;
    m.insert(42, 0);
    CHECK(m.contains(42));
    CHECK(!m.contains(43));
}

TEST_CASE("HashMap_Erase") {
    HashMap<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);
    bool ok = m.erase(1);
    CHECK(ok);
    CHECK(m.size() == 1);
    CHECK(!m.contains(1));
    CHECK(m.contains(2));
}

TEST_CASE("HashMap_Erase_Missing") {
    HashMap<int, int> m;
    CHECK(!m.erase(999));
}

TEST_CASE("HashMap_BracketOperator_Insert") {
    HashMap<int, int> m;
    m[5] = 55;
    CHECK(m.contains(5));
    CHECK(*m.find(5) == 55);
}

TEST_CASE("HashMap_BracketOperator_Update") {
    HashMap<int, int> m;
    m[1] = 10;
    m[1] = 20;  // 通过 operator= 更新
    CHECK(*m.find(1) == 20);
}

TEST_CASE("HashMap_InsertOrAssign") {
    HashMap<int, int> m;
    m.insert(1, 10);
    m.insert_or_assign(1, 99);
    CHECK(*m.find(1) == 99);
    CHECK(m.size() == 1);
}

TEST_CASE("HashMap_Rehash") {
    // 插入大量元素，触发多次 rehash
    HashMap<int, int> m;
    for (int i = 0; i < 100; ++i) {
        m.insert(i, i * 10);
    }
    CHECK(m.size() == 100);
    for (int i = 0; i < 100; ++i) {
        int* v = m.find(i);
        REQUIRE(v != nullptr);
        CHECK(*v == i * 10);
    }
}

TEST_CASE("HashMap_Iterator") {
    HashMap<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);
    m.insert(3, 30);
    int key_sum = 0, val_sum = 0;
    for (auto it = m.begin(); it != m.end(); ++it) {
        key_sum += it.key();
        val_sum += it.value();
    }
    CHECK(key_sum == 6);
    CHECK(val_sum == 60);
}

TEST_CASE("HashMap_MoveConstruct") {
    HashMap<int, int> a;
    a.insert(1, 10);
    a.insert(2, 20);
    HashMap<int, int> b{std::move(a)};
    CHECK(b.size() == 2);
    CHECK(a.empty());
    CHECK(*b.find(1) == 10);
}

TEST_CASE("HashMap_StringView_Key") {
    HashMap<StringView, int> m;
    m.insert("hello", 1);
    m.insert("world", 2);
    CHECK(m.contains("hello"));
    CHECK(*m.find("world") == 2);
    CHECK(m.find("xxx") == nullptr);
}

TEST_CASE("HashMap_Clear") {
    HashMap<int, int> m;
    m.insert(1, 1);
    m.insert(2, 2);
    m.clear();
    CHECK(m.empty());
    CHECK(m.size() == 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// InlineString
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("InlineString_DefaultConstruct") {
    InlineString s;
    CHECK(s.empty());
    CHECK(s.size() == 0);
    CHECK(s.is_small());
    CHECK(std::string_view{s.c_str()} == "");
}

TEST_CASE("InlineString_FromCStr_Short") {
    InlineString s{"hello"};
    CHECK(s.size() == 5);
    CHECK(s.is_small());
    CHECK(s == "hello");
}

TEST_CASE("InlineString_FromCStr_Long") {
    // 超过 23 字节，触发堆分配
    const char* long_str = "this is a longer string that exceeds sso";
    InlineString s{long_str};
    CHECK(s.size() == std::strlen(long_str));
    CHECK(!s.is_small());
    CHECK(s == long_str);
}

TEST_CASE("InlineString_FromStringView") {
    StringView sv{"world"};
    InlineString s{sv};
    CHECK(s == "world");
    CHECK(s.is_small());
}

TEST_CASE("InlineString_PushBack") {
    InlineString s{"abc"};
    s.push_back('d');
    CHECK(s.size() == 4);
    CHECK(s == "abcd");
}

TEST_CASE("InlineString_PopBack") {
    InlineString s{"hello"};
    s.pop_back();
    CHECK(s == "hell");
}

TEST_CASE("InlineString_Append_StringView") {
    InlineString s{"foo"};
    s.append(StringView{"bar"});
    CHECK(s == "foobar");
}

TEST_CASE("InlineString_PlusEquals") {
    InlineString s{"ab"};
    s += 'c';
    s += StringView{"de"};
    CHECK(s == "abcde");
}

TEST_CASE("InlineString_CopyConstruct") {
    InlineString a{"test"};
    InlineString b{a};
    CHECK(b == "test");
    b.push_back('!');
    CHECK(a == "test");  // a 不受影响
}

TEST_CASE("InlineString_MoveConstruct_Small") {
    InlineString a{"hi"};
    InlineString b{std::move(a)};
    CHECK(b == "hi");
    CHECK(a.empty());
}

TEST_CASE("InlineString_MoveConstruct_Heap") {
    const char* long_str = "this is quite a long string definitely heap";
    InlineString a{long_str};
    InlineString b{std::move(a)};
    CHECK(b == long_str);
    CHECK(a.empty());
}

TEST_CASE("InlineString_AssignOperator") {
    InlineString s{"old"};
    s = StringView{"new value"};
    CHECK(s == "new value");
}

TEST_CASE("InlineString_Clear") {
    InlineString s{"hello"};
    s.clear();
    CHECK(s.empty());
    CHECK(s == "");
}

TEST_CASE("InlineString_Substr") {
    InlineString s{"hello world"};
    StringView sub = s.substr(6, 5);
    CHECK(sub == "world");
}

TEST_CASE("InlineString_Find_Char") {
    InlineString s{"abcabc"};
    CHECK(s.find('b') == 1);
    CHECK(s.find('d') == InlineString::npos);
}

TEST_CASE("InlineString_StartsWith_EndsWith") {
    InlineString s{"hello world"};
    CHECK(s.starts_with("hello"));
    CHECK(s.ends_with("world"));
    CHECK(!s.starts_with("world"));
}

TEST_CASE("InlineString_ToStringView") {
    InlineString s{"mine"};
    StringView sv = s;
    CHECK(sv.size() == 4);
    CHECK(sv == "mine");
}

TEST_CASE("InlineString_Compare") {
    InlineString a{"apple"};
    InlineString b{"banana"};
    CHECK(a < b);
    CHECK(a == "apple");
    CHECK(a != b);
}

TEST_CASE("InlineString_Heap_Transition") {
    // 从内联过渡到堆，再退回内联（assign 短字符串时）
    InlineString s{"short"};
    CHECK(s.is_small());
    s = "this is a very long string that forces heap allocation for sure";
    CHECK(!s.is_small());
    s = "hi";
    CHECK(s.is_small());
    CHECK(s == "hi");
}

TEST_CASE("InlineString_Hash") {
    Hash<InlineString> hasher;
    InlineString a{"key"};
    InlineString b{"key"};
    InlineString c{"other"};
    CHECK(hasher(a) == hasher(b));
    CHECK(hasher(a) != hasher(c));
}

TEST_CASE("InlineString_In_HashMap") {
    HashMap<InlineString, int> m;
    m.insert(InlineString{"one"}, 1);
    m.insert(InlineString{"two"}, 2);
    CHECK(m.contains(InlineString{"one"}));
    CHECK(*m.find(InlineString{"two"}) == 2);
}

// ─────────────────────────────────────────────────────────────────────────────
// IntrusiveList<T>
// ─────────────────────────────────────────────────────────────────────────────

struct Widget : public IntrusiveListNode<Widget> {
    int id;
    explicit Widget(int i) : id{i} {}
};

TEST_CASE("IntrusiveList_Empty") {
    IntrusiveList<Widget> list;
    CHECK(list.empty());
    CHECK(list.size() == 0);
}

TEST_CASE("IntrusiveList_PushBack") {
    IntrusiveList<Widget> list;
    Widget w1{1}, w2{2}, w3{3};
    list.push_back(w1);
    list.push_back(w2);
    list.push_back(w3);
    CHECK(list.size() == 3);
    CHECK(list.front().id == 1);
    CHECK(list.back().id == 3);
}

TEST_CASE("IntrusiveList_PushFront") {
    IntrusiveList<Widget> list;
    Widget w1{1}, w2{2};
    list.push_front(w2);
    list.push_front(w1);
    CHECK(list.front().id == 1);
    CHECK(list.back().id == 2);
}

TEST_CASE("IntrusiveList_PopFront") {
    IntrusiveList<Widget> list;
    Widget w1{1}, w2{2};
    list.push_back(w1);
    list.push_back(w2);
    Widget& front = list.pop_front();
    CHECK(front.id == 1);
    CHECK(list.size() == 1);
    CHECK(!w1.is_linked());
}

TEST_CASE("IntrusiveList_PopBack") {
    IntrusiveList<Widget> list;
    Widget w1{1}, w2{2};
    list.push_back(w1);
    list.push_back(w2);
    Widget& back = list.pop_back();
    CHECK(back.id == 2);
    CHECK(list.size() == 1);
}

TEST_CASE("IntrusiveList_Erase") {
    IntrusiveList<Widget> list;
    Widget w1{1}, w2{2}, w3{3};
    list.push_back(w1);
    list.push_back(w2);
    list.push_back(w3);
    auto it = list.begin();
    ++it;  // 指向 w2
    auto next = list.erase(it);
    CHECK(list.size() == 2);
    CHECK(!w2.is_linked());
    CHECK(next->id == 3);
}

TEST_CASE("IntrusiveList_Insert") {
    IntrusiveList<Widget> list;
    Widget w1{1}, w3{3}, w2{2};
    list.push_back(w1);
    list.push_back(w3);
    auto it = list.begin();
    ++it;  // 指向 w3
    list.insert(it, w2);
    int ids[3];
    int i = 0;
    for (Widget& w : list) ids[i++] = w.id;
    CHECK(ids[0] == 1);
    CHECK(ids[1] == 2);
    CHECK(ids[2] == 3);
}

TEST_CASE("IntrusiveList_NodeAutoUnlink") {
    IntrusiveList<Widget> list;
    {
        Widget w{42};
        list.push_back(w);
        CHECK(list.size() == 1);
        // w 析构时应自动从 list 中移除
    }
    CHECK(list.empty());
}

TEST_CASE("IntrusiveList_Unlink_SelfRemoval") {
    IntrusiveList<Widget> list;
    Widget w1{1}, w2{2}, w3{3};
    list.push_back(w1);
    list.push_back(w2);
    list.push_back(w3);
    // w2 自己从链表中移除
    w2.unlink();
    CHECK(list.size() == 2);
    CHECK(!w2.is_linked());
    CHECK(list.front().id == 1);
    CHECK(list.back().id == 3);
}

TEST_CASE("IntrusiveList_Iterator") {
    IntrusiveList<Widget> list;
    Widget w1{10}, w2{20}, w3{30};
    list.push_back(w1);
    list.push_back(w2);
    list.push_back(w3);
    int sum = 0;
    for (Widget& w : list) sum += w.id;
    CHECK(sum == 60);
}

TEST_CASE("IntrusiveList_Clear") {
    IntrusiveList<Widget> list;
    Widget w1{1}, w2{2};
    list.push_back(w1);
    list.push_back(w2);
    list.clear();
    CHECK(list.empty());
    CHECK(!w1.is_linked());
    CHECK(!w2.is_linked());
}

TEST_CASE("IntrusiveList_Move") {
    IntrusiveList<Widget> a;
    Widget w1{1}, w2{2};
    a.push_back(w1);
    a.push_back(w2);
    IntrusiveList<Widget> b{std::move(a)};
    CHECK(a.empty());
    CHECK(b.size() == 2);
    CHECK(b.front().id == 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Hash<T> — 基础哈希验证
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Hash_Int") {
    Hash<int> h;
    CHECK(h(0) == h(0));
    CHECK(h(1) != h(2));
}

TEST_CASE("Hash_Int64") {
    Hash<int64_t> h;
    CHECK(h(100LL) == h(100LL));
    CHECK(h(0LL) != h(-1LL));
}

TEST_CASE("Hash_Pointer") {
    Hash<int*> h;
    int a = 0, b = 0;
    CHECK(h(&a) == h(&a));
    CHECK(h(&a) != h(&b));
}

TEST_CASE("Hash_StringView") {
    Hash<StringView> h;
    CHECK(h("abc") == h("abc"));
    CHECK(h("abc") != h("abcd"));
}

TEST_CASE("Hash_InlineString") {
    Hash<InlineString> h;
    InlineString a{"test"}, b{"test"}, c{"other"};
    CHECK(h(a) == h(b));
    CHECK(h(a) != h(c));
}
