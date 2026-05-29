/**
 * @file MvvmTest.cpp
 * @brief mine.mvvm 模块单元测试。
 *
 * 覆盖：
 *   - ObservableObject：subscribe/unsubscribe，set<T>() 触发通知，
 *     同值不触发，raise() 手动触发
 *   - MINE_OBSERVABLE 宏：getter/setter 自动生成，变更通知正常触发
 *   - ViewModelBase：默认生命周期虚方法为空实现，子类重写正确调用
 *   - ObservableCollection<T>：add/insert/remove_at/remove/replace/move/clear
 *     各触发正确的 action 和 index；多订阅者；subscribe/unsubscribe
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <mine/mvvm/Mvvm.h>

using namespace mine::mvvm;
using namespace mine::ui;

// ─────────────────────────────────────────────────────────────────────────────
// ObservableObject 测试
// ─────────────────────────────────────────────────────────────────────────────

namespace {

/**
 * @brief 用于测试的简单 ObservableObject 子类
 */
class TestModel : public ObservableObject {
public:
    MINE_OBSERVABLE(int, count, 0)
    MINE_OBSERVABLE(bool, flag, false)

    /**
     * @brief 公开暴露 raise()，以便测试手动触发计算属性通知
     */
    void raise_display() noexcept {
        raise("display");
    }
};

} // namespace

TEST_SUITE("ObservableObject") {

    TEST_CASE("subscribe/unsubscribe 基础操作") {
        TestModel model;

        int call_count = 0;
        auto fn = [](INotifyPropertyChanged* /*sender*/,
                     mine::core::StringView  /*name*/,
                     void*                  user_data) noexcept {
            ++*static_cast<int*>(user_data);
        };

        [[maybe_unused]] uint32_t token = model.subscribe_property_changed(fn, &call_count);
        CHECK(token != 0);

        // 触发通知
        model.set_count(1);
        CHECK(call_count == 1);

        // 取消订阅后不再触发
        model.unsubscribe_property_changed(token);
        model.set_count(2);
        CHECK(call_count == 1);  // 不变
    }

    TEST_CASE("同值不触发通知（EqualityComparable 分支）") {
        TestModel model;

        int call_count = 0;
        auto fn = [](INotifyPropertyChanged*, mine::core::StringView, void* ud) noexcept {
            ++*static_cast<int*>(ud);
        };

        [[maybe_unused]] auto tok = model.subscribe_property_changed(fn, &call_count);

        model.set_count(5);
        CHECK(call_count == 1);

        // 相同值，不触发
        model.set_count(5);
        CHECK(call_count == 1);

        // 不同值，触发
        model.set_count(6);
        CHECK(call_count == 2);
    }

    TEST_CASE("raise() 手动触发计算属性通知") {
        TestModel model;

        mine::core::StringView last_name;
        auto fn = [](INotifyPropertyChanged*, mine::core::StringView name, void* ud) noexcept {
            *static_cast<mine::core::StringView*>(ud) = name;
        };

        [[maybe_unused]] auto tok = model.subscribe_property_changed(fn, &last_name);

        model.raise_display();
        CHECK(last_name == "display");
    }

    TEST_CASE("多订阅者均收到通知") {
        TestModel model;

        int cnt1 = 0, cnt2 = 0;
        auto fn1 = [](INotifyPropertyChanged*, mine::core::StringView, void* ud) noexcept {
            ++*static_cast<int*>(ud);
        };
        auto fn2 = [](INotifyPropertyChanged*, mine::core::StringView, void* ud) noexcept {
            ++*static_cast<int*>(ud);
        };

        [[maybe_unused]] auto tok1 = model.subscribe_property_changed(fn1, &cnt1);
        [[maybe_unused]] auto tok2 = model.subscribe_property_changed(fn2, &cnt2);

        model.set_flag(true);

        CHECK(cnt1 == 1);
        CHECK(cnt2 == 1);
    }

    TEST_CASE("MINE_OBSERVABLE getter 返回正确值") {
        [[maybe_unused]] TestModel model;

        CHECK(model.count() == 0);
        model.set_count(42);
        CHECK(model.count() == 42);

        CHECK(model.flag() == false);
        model.set_flag(true);
        CHECK(model.flag() == true);
    }

    TEST_CASE("取消无效令牌（不崩溃）") {
        TestModel model;
        // 取消一个从未注册过的令牌，应静默忽略
        model.unsubscribe_property_changed(9999);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// ViewModelBase 测试
// ─────────────────────────────────────────────────────────────────────────────

namespace {

/**
 * @brief 记录生命周期调用顺序的测试 ViewModel
 */
class TrackingViewModel : public ViewModelBase {
public:
    bool initialized    = false;
    bool navigated_to_  = false;
    bool navigated_from_= false;
    bool appeared       = false;
    bool disappeared    = false;
    bool disposed       = false;

    mine::core::Variant last_param;

    /// 供测试用：公开触发任意属性变更通知
    void raise_prop(mine::core::StringView name) noexcept {
        raise(name);
    }

protected:
    void on_initialize() noexcept override          { initialized     = true; }
    void on_navigated_to(const mine::core::Variant& p) noexcept override {
        navigated_to_ = true;
        last_param    = p;
    }
    void on_navigated_from() noexcept override      { navigated_from_ = true; }
    void on_appear()         noexcept override      { appeared        = true; }
    void on_disappear()      noexcept override      { disappeared     = true; }
    void on_dispose()        noexcept override      { disposed        = true; }
};

} // namespace

TEST_SUITE("ViewModelBase") {

    TEST_CASE("生命周期方法默认为空实现（不崩溃）") {
        ViewModelBase vm;
        vm.initialize();
        vm.navigated_to(mine::core::Variant{});
        vm.navigated_from();
        vm.appear();
        vm.disappear();
        vm.dispose();
        // 无断言，只验证不崩溃
    }

    TEST_CASE("子类重写生命周期虚方法") {
        TrackingViewModel vm;

        CHECK(!vm.initialized);
        vm.initialize();
        CHECK(vm.initialized);

        CHECK(!vm.appeared);
        vm.appear();
        CHECK(vm.appeared);

        CHECK(!vm.disappeared);
        vm.disappear();
        CHECK(vm.disappeared);

        CHECK(!vm.navigated_to_);
        mine::core::Variant p;
        vm.navigated_to(p);
        CHECK(vm.navigated_to_);

        CHECK(!vm.navigated_from_);
        vm.navigated_from();
        CHECK(vm.navigated_from_);

        CHECK(!vm.disposed);
        vm.dispose();
        CHECK(vm.disposed);
    }

    TEST_CASE("ViewModelBase 继承 ObservableObject，属性通知正常") {
        TrackingViewModel vm;

        int cnt = 0;
        auto fn = [](INotifyPropertyChanged*, mine::core::StringView, void* ud) noexcept {
            ++*static_cast<int*>(ud);
        };

        [[maybe_unused]] auto tok = vm.subscribe_property_changed(fn, &cnt);
        vm.raise_prop("some_prop");
        CHECK(cnt == 1);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// ObservableCollection<T> 测试
// ─────────────────────────────────────────────────────────────────────────────

namespace {

/**
 * @brief 用于记录最后一次集合变更通知的辅助结构
 */
struct LastEvent {
    CollectionChangedAction action{};
    int new_index{ -1 };
    int old_index{ -1 };
    int count{ 0 };
    int call_count{ 0 };
};

/**
 * @brief 标准测试回调，将变更参数记录到 LastEvent
 */
inline void on_changed(INotifyCollectionChanged* /*sender*/,
                       const CollectionChangedArgs& args,
                       void* user_data) noexcept
{
    auto* ev          = static_cast<LastEvent*>(user_data);
    ev->action        = args.action;
    ev->new_index     = args.new_index;
    ev->old_index     = args.old_index;
    ev->count         = args.count;
    ++ev->call_count;
}

} // namespace

TEST_SUITE("ObservableCollection") {

    TEST_CASE("add 触发 Add 通知") {
        ObservableCollection<int> coll;
        LastEvent ev;
        [[maybe_unused]] auto tok = coll.subscribe_collection_changed(on_changed, &ev);

        coll.add(10);
        CHECK(coll.size() == 1);
        CHECK(coll[0] == 10);
        CHECK(ev.action    == CollectionChangedAction::Add);
        CHECK(ev.new_index == 0);
        CHECK(ev.old_index == -1);
        CHECK(ev.count     == 1);
        CHECK(ev.call_count == 1);

        coll.add(20);
        CHECK(ev.new_index == 1);
    }

    TEST_CASE("insert 触发 Add 通知（头部插入）") {
        ObservableCollection<int> coll;
        LastEvent ev;
        [[maybe_unused]] auto tok = coll.subscribe_collection_changed(on_changed, &ev);

        coll.add(1);
        coll.add(2);
        ev.call_count = 0;

        coll.insert(0, 99);  // 插入到头部
        CHECK(coll.size() == 3);
        CHECK(coll[0] == 99);
        CHECK(ev.action    == CollectionChangedAction::Add);
        CHECK(ev.new_index == 0);
        CHECK(ev.call_count == 1);
    }

    TEST_CASE("remove_at 触发 Remove 通知") {
        ObservableCollection<int> coll;
        coll.add(10);
        coll.add(20);
        coll.add(30);

        LastEvent ev;
        [[maybe_unused]] auto tok = coll.subscribe_collection_changed(on_changed, &ev);

        coll.remove_at(1);  // 删除 20
        CHECK(coll.size() == 2);
        CHECK(coll[0] == 10);
        CHECK(coll[1] == 30);
        CHECK(ev.action    == CollectionChangedAction::Remove);
        CHECK(ev.old_index == 1);
        CHECK(ev.new_index == -1);
        CHECK(ev.call_count == 1);
    }

    TEST_CASE("remove(value) 触发 Remove 通知") {
        ObservableCollection<int> coll;
        coll.add(5);
        coll.add(10);
        coll.add(15);

        LastEvent ev;
        [[maybe_unused]] auto tok = coll.subscribe_collection_changed(on_changed, &ev);

        bool removed = coll.remove(10);
        CHECK(removed);
        CHECK(coll.size() == 2);
        CHECK(ev.action    == CollectionChangedAction::Remove);
        CHECK(ev.call_count == 1);

        // 移除不存在的元素
        removed = coll.remove(99);
        CHECK(!removed);
        CHECK(ev.call_count == 1);  // 不额外触发
    }

    TEST_CASE("replace 触发 Replace 通知") {
        ObservableCollection<int> coll;
        coll.add(1);
        coll.add(2);

        LastEvent ev;
        [[maybe_unused]] auto tok = coll.subscribe_collection_changed(on_changed, &ev);

        coll.replace(0, 100);
        CHECK(coll[0] == 100);
        CHECK(ev.action    == CollectionChangedAction::Replace);
        CHECK(ev.new_index == 0);
        CHECK(ev.old_index == 0);
        CHECK(ev.call_count == 1);
    }

    TEST_CASE("move 触发 Move 通知") {
        ObservableCollection<int> coll;
        coll.add(1);
        coll.add(2);
        coll.add(3);
        // [1, 2, 3]

        LastEvent ev;
        [[maybe_unused]] auto tok = coll.subscribe_collection_changed(on_changed, &ev);

        // 把索引 0 的元素（值 1）移到索引 2（即末尾）
        coll.move(0, 2);
        CHECK(ev.action    == CollectionChangedAction::Move);
        CHECK(ev.old_index == 0);
        CHECK(ev.call_count == 1);
    }

    TEST_CASE("move 同位置不触发通知") {
        ObservableCollection<int> coll;
        coll.add(1);
        coll.add(2);

        LastEvent ev;
        [[maybe_unused]] auto tok = coll.subscribe_collection_changed(on_changed, &ev);

        coll.move(1, 1);
        CHECK(ev.call_count == 0);
    }

    TEST_CASE("clear 触发 Reset 通知") {
        ObservableCollection<int> coll;
        coll.add(1);
        coll.add(2);
        coll.add(3);

        LastEvent ev;
        [[maybe_unused]] auto tok = coll.subscribe_collection_changed(on_changed, &ev);

        coll.clear();
        CHECK(coll.empty());
        CHECK(ev.action    == CollectionChangedAction::Reset);
        CHECK(ev.call_count == 1);
    }

    TEST_CASE("空集合 clear 不触发通知") {
        ObservableCollection<int> coll;

        LastEvent ev;
        [[maybe_unused]] auto tok = coll.subscribe_collection_changed(on_changed, &ev);

        coll.clear();
        CHECK(ev.call_count == 0);
    }

    TEST_CASE("多订阅者均收到通知") {
        ObservableCollection<int> coll;
        LastEvent ev1, ev2;
        [[maybe_unused]] auto tok1 = coll.subscribe_collection_changed(on_changed, &ev1);
        [[maybe_unused]] auto tok2 = coll.subscribe_collection_changed(on_changed, &ev2);

        coll.add(42);
        CHECK(ev1.call_count == 1);
        CHECK(ev2.call_count == 1);
    }

    TEST_CASE("unsubscribe 后不再收到通知") {
        ObservableCollection<int> coll;
        LastEvent ev;
        uint32_t token = coll.subscribe_collection_changed(on_changed, &ev);

        coll.add(1);
        CHECK(ev.call_count == 1);

        coll.unsubscribe_collection_changed(token);
        coll.add(2);
        CHECK(ev.call_count == 1);  // 不变
    }

    TEST_CASE("size/empty/at/operator[] 基础访问") {
        ObservableCollection<int> coll;
        CHECK(coll.empty());
        CHECK(coll.size() == 0);

        coll.add(10);
        CHECK(!coll.empty());
        CHECK(coll.size() == 1);
        CHECK(coll.at(0) == 10);
        CHECK(coll[0]    == 10);
    }

    TEST_CASE("迭代器可用") {
        ObservableCollection<int> coll;
        coll.add(1);
        coll.add(2);
        coll.add(3);

        int sum = 0;
        for (int v : coll) sum += v;
        CHECK(sum == 6);
    }
}
