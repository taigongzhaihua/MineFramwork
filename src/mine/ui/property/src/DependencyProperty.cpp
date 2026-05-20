/**
 * @file DependencyProperty.cpp
 * @brief DependencyProperty 全局注册表与描述符实现。
 *
 * 注册表（DependencyPropertyRegistry）是一个 Meyer's 单例，
 * 在首次调用时构造，在程序退出时析构（调用所有属性描述符的析构函数
 * 并释放内存）。
 *
 * 线程安全说明：
 *   register_property() 应在静态初始化阶段（全局/局部静态变量初始化时）
 *   调用，此阶段单线程执行，无须加锁。运行期不应再注册新属性。
 */

#include <mine/ui/property/DependencyProperty.h>

#include <memory>  // std::construct_at / std::destroy_at（Variant.h 依赖）
#include <mine/core/Assert.h>
#include <mine/core/Allocator.h>
#include <mine/containers/Vector.h>

namespace mine::ui {

// ────────────────────────────────────────────────────────────────────────────
// 内部：全局注册表
// ────────────────────────────────────────────────────────────────────────────

/**
 * @brief 依赖属性全局注册表。
 *
 * 所有通过 register_property / register_attached_property 创建的
 * DependencyProperty 实例均由此注册表拥有（使用 mine::core::default_allocator()
 * 分配），程序退出时统一析构释放。
 *
 * 实例身份：DependencyProperty* 指针即为属性身份（地址比较）。
 */
class DependencyPropertyRegistry {
public:
    /// Meyer's 单例：首次调用时构造，程序退出时析构
    static DependencyPropertyRegistry& instance() noexcept {
        static DependencyPropertyRegistry s_instance;
        return s_instance;
    }

    /**
     * @brief 注册一个新的依赖属性描述符。
     *
     * 若 (owner_type, name) 已存在则触发断言（禁止重复注册）。
     * 在内部分配 DependencyProperty 对象并加入注册列表。
     *
     * @return 对新属性描述符的常量引用（地址在程序运行期稳定）
     */
    const DependencyProperty& add(core::StringView  name,
                                   core::TypeId      owner_type,
                                   core::TypeId      value_type,
                                   core::Variant     default_value,
                                   PropertyMetadata  metadata,
                                   bool              is_attached) {
        // 检查是否已注册相同 (所有者类型, 名称) 的属性
        for (DependencyProperty* p : props_) {
            if (p->owner_type() == owner_type && p->name() == name) {
                MINE_ASSERT_MSG(false, "DependencyProperty 重复注册：同一所有者类型下已存在同名属性");
                return *p;
            }
        }

        // 分配 DependencyProperty 对象（使用默认分配器，跨 DLL 安全）
        auto* alloc = core::default_allocator();
        void* mem   = alloc->alloc(sizeof(DependencyProperty), alignof(DependencyProperty));
        MINE_CHECK(mem != nullptr);

        // placement new 构造
        DependencyProperty* prop = new (mem) DependencyProperty(name,
                                                                 owner_type,
                                                                 value_type,
                                                                 std::move(default_value),
                                                                 metadata,
                                                                 is_attached);
        props_.push_back(prop);
        return *prop;
    }

    ~DependencyPropertyRegistry() {
        // 析构并释放所有已注册的属性描述符
        auto* alloc = core::default_allocator();
        for (DependencyProperty* p : props_) {
            p->~DependencyProperty();
            alloc->dealloc(p, sizeof(DependencyProperty), alignof(DependencyProperty));
        }
    }

private:
    DependencyPropertyRegistry() = default;

    /// 所有已注册属性描述符的原始指针（注册表拥有生命周期）
    containers::Vector<DependencyProperty*> props_;
};

// ────────────────────────────────────────────────────────────────────────────
// DependencyProperty 实现
// ────────────────────────────────────────────────────────────────────────────

DependencyProperty::DependencyProperty(core::StringView  name,
                                       core::TypeId      owner_type,
                                       core::TypeId      value_type,
                                       core::Variant     default_value,
                                       PropertyMetadata  metadata,
                                       bool              is_attached) noexcept
    : name_{name}
    , owner_type_{owner_type}
    , value_type_{value_type}
    , default_value_{std::move(default_value)}
    , metadata_{metadata}
    , is_attached_{is_attached}
{}

DependencyProperty::~DependencyProperty() noexcept = default;

core::StringView DependencyProperty::name() const noexcept {
    return name_;
}

core::TypeId DependencyProperty::owner_type() const noexcept {
    return owner_type_;
}

core::TypeId DependencyProperty::value_type() const noexcept {
    return value_type_;
}

const core::Variant& DependencyProperty::default_value() const noexcept {
    return default_value_;
}

const PropertyMetadata& DependencyProperty::metadata() const noexcept {
    return metadata_;
}

bool DependencyProperty::is_attached() const noexcept {
    return is_attached_;
}

// ────────────────────────────────────────────────────────────────────────────
// 公开注册函数
// ────────────────────────────────────────────────────────────────────────────

const DependencyProperty& register_property(core::StringView  name,
                                            core::TypeId      owner_type,
                                            core::TypeId      value_type,
                                            core::Variant     default_value,
                                            PropertyMetadata  metadata) {
    return DependencyPropertyRegistry::instance().add(name,
                                                       owner_type,
                                                       value_type,
                                                       std::move(default_value),
                                                       metadata,
                                                       /*is_attached=*/false);
}

const DependencyProperty& register_attached_property(core::StringView  name,
                                                     core::TypeId      owner_type,
                                                     core::TypeId      value_type,
                                                     core::Variant     default_value,
                                                     PropertyMetadata  metadata) {
    return DependencyPropertyRegistry::instance().add(name,
                                                       owner_type,
                                                       value_type,
                                                       std::move(default_value),
                                                       metadata,
                                                       /*is_attached=*/true);
}

} // namespace mine::ui
