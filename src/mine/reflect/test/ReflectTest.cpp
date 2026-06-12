/**
 * @file ReflectTest.cpp
 * @brief mine.reflect 模块单元测试。
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <mine/reflect/Reflect.h>
#include <mine/core/Variant.h>
#include <mine/core/StringView.h>

using namespace mine::reflect;
using namespace mine::core;

// ══════════════════════════════════════════════════════════════════════════════
// 测试用类型
// ══════════════════════════════════════════════════════════════════════════════

struct Animal {
    MINE_REFLECT_DECL();
    virtual ~Animal() = default;

    const char* name() const { return name_; }
    void set_name(const char* n) { name_ = n; }

    int age() const { return age_; }
    void set_age(int a) { age_ = a; }

private:
    const char* name_{"未命名"};
    int age_{0};
};

struct Dog : Animal {
    MINE_REFLECT_DECL();

    const char* breed() const { return breed_; }
    void set_breed(const char* b) { breed_ = b; }

private:
    const char* breed_{"混种"};
};

struct EmptyComponent {
    MINE_REFLECT_DECL();
};

// ══════════════════════════════════════════════════════════════════════════════
// 反射注册
// ══════════════════════════════════════════════════════════════════════════════

MINE_REFLECT_IMPL(Animal, void,
    MINE_PROP_ENTRY(Animal, name, const char*, name, set_name),
    MINE_PROP_ENTRY(Animal, age,  int,        age,  set_age)
);

MINE_REFLECT_IMPL(Dog, Animal,
    MINE_PROP_ENTRY(Dog, breed, const char*, breed, set_breed)
);

MINE_REFLECT_IMPL(EmptyComponent, void);

// ══════════════════════════════════════════════════════════════════════════════
// 测试
// ══════════════════════════════════════════════════════════════════════════════

TEST_CASE("reflect_TypeInfo_name_and_type_id")
{
    const TypeInfo* info = Animal::static_type_info();
    REQUIRE(info != nullptr);
    CHECK(info->name == "Animal");
    CHECK(info->type_id == TypeId::of<Animal>());
}

TEST_CASE("reflect_TypeInfo_no_base_is_null")
{
    CHECK(Animal::static_type_info()->base == nullptr);
}

TEST_CASE("reflect_TypeInfo_derived_base_points_to_parent")
{
    CHECK(Dog::static_type_info()->base == Animal::static_type_info());
}

TEST_CASE("reflect_TypeInfo_property_count")
{
    CHECK(Animal::static_type_info()->properties.size() == 2);
    CHECK(Dog::static_type_info()->properties.size() == 1);
}

TEST_CASE("reflect_TypeInfo_find_property")
{
    auto* info = Animal::static_type_info();
    CHECK(info->find_property("name") != nullptr);
    CHECK(info->find_property("age") != nullptr);
    CHECK(info->find_property("不存在") == nullptr);
}

TEST_CASE("reflect_TypeInfo_is_a")
{
    auto* dog = Dog::static_type_info();
    auto* animal = Animal::static_type_info();
    CHECK(dog->is_a(animal));
    CHECK(dog->is_a(dog));
    CHECK(!animal->is_a(dog));
}

TEST_CASE("reflect_PropertyInfo_getter")
{
    Animal obj;
    obj.set_name("test");
    obj.set_age(42);

    auto* info = Animal::static_type_info();

    auto* np = info->find_property("name");
    REQUIRE(np != nullptr);
    Variant nv = np->getter(&obj);
    CHECK(StringView{nv.get<const char*>()} == "test");

    auto* ap = info->find_property("age");
    REQUIRE(ap != nullptr);
    CHECK(ap->getter(&obj).get<int>() == 42);
}

TEST_CASE("reflect_PropertyInfo_setter")
{
    Animal obj;
    auto* info = Animal::static_type_info();
    auto* np = info->find_property("name");
    REQUIRE(np != nullptr);

    np->setter(&obj, Variant{"新名称"});
    CHECK(StringView{obj.name()} == "新名称");

    auto* ap = info->find_property("age");
    REQUIRE(ap != nullptr);
    ap->setter(&obj, Variant{99});
    CHECK(obj.age() == 99);
}

TEST_CASE("reflect_derived_property_accessible")
{
    Dog dog;
    dog.set_breed("金毛");

    auto* bp = Dog::static_type_info()->find_property("breed");
    REQUIRE(bp != nullptr);
    Variant v = bp->getter(&dog);
    CHECK(StringView{v.get<const char*>()} == "金毛");
}

TEST_CASE("reflect_base_property_on_derived_instance")
{
    Dog dog;
    dog.set_name("旺财");

    auto* np = Animal::static_type_info()->find_property("name");
    REQUIRE(np != nullptr);
    Variant v = np->getter(&dog);
    CHECK(StringView{v.get<const char*>()} == "旺财");
}

TEST_CASE("reflect_registry_find_by_name")
{
    auto* info = TypeRegistry::instance().find_by_name("Animal");
    REQUIRE(info != nullptr);
    CHECK(info->type_id == TypeId::of<Animal>());
}

TEST_CASE("reflect_registry_find_by_id")
{
    auto* info = TypeRegistry::instance().find_by_id(TypeId::of<Dog>());
    REQUIRE(info != nullptr);
    CHECK(info->name == "Dog");
}

TEST_CASE("reflect_registry_not_found")
{
    CHECK(TypeRegistry::instance().find_by_name("不存在") == nullptr);
    CHECK(TypeRegistry::instance().find_by_id(TypeId{}) == nullptr);
}

TEST_CASE("reflect_registry_type_count")
{
    CHECK(TypeRegistry::instance().type_count() >= 3);
}

TEST_CASE("reflect_registry_all_types")
{
    bool has_animal = false, has_dog = false;
    for (auto* info : TypeRegistry::instance().all_types()) {
        if (info->name == "Animal") has_animal = true;
        if (info->name == "Dog") has_dog = true;
    }
    CHECK(has_animal);
    CHECK(has_dog);
}

TEST_CASE("reflect_empty_component")
{
    auto* info = EmptyComponent::static_type_info();
    REQUIRE(info != nullptr);
    CHECK(info->name == "EmptyComponent");
    CHECK(info->properties.size() == 0);
}

TEST_CASE("reflect_helpers")
{
    CHECK(type_name_by_id(TypeId::of<Animal>()) == "Animal");
    CHECK(type_id_by_name("Dog") == TypeId::of<Dog>());
    CHECK(type_name_by_id(TypeId{}).size() == 0);
    CHECK(!type_id_by_name("不存在").valid());
}

TEST_CASE("reflect_virtual_get_reflect_type_info")
{
    // 虚函数调用：通过基类指针调用 _get_reflect_type_info()
    // 由于 _get_reflect_type_info 是虚函数，实际调用的是 Dog 的版本，
    // 返回 Dog::static_type_info()
    Dog dog;
    Animal* ptr = &dog;
    auto* info = ptr->_get_reflect_type_info();
    REQUIRE(info != nullptr);
    // 虚函数分派到 Dog 的实现，返回 Dog 的 TypeInfo
    CHECK(info->type_id == TypeId::of<Dog>());
}
