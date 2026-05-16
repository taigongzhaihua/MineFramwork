# 09 — 依赖属性与数据绑定

## 9.1 依赖属性系统（mine.ui.property）

WPF/Avalonia 风格，但 **静态注册 + 编译期布局**：

```cpp
class Button : public ButtonBase {
public:
    static const ::mine::ui::DependencyProperty& ContentProperty;
    void  set_content(::mine::Variant v);
    auto  content() const -> ::mine::Variant const&;
};

// 静态注册（实现文件）
const DependencyProperty& Button::ContentProperty =
    register_property<Button, Variant>(
        "Content",
        PropertyOptions{ .default_value = Variant{},
                         .affects_measure = true,
                         .changed = &Button::on_content_changed });
```

特性：

* 每属性一个 token（编译期编号）。
* 值优先级（高 → 低）：动画值 > 本地值 > 模板绑定 > 样式状态 setter（当前视觉状态）> 样式 setter > 继承 > 默认。完整说明见 [20-style-template.md §20.5](20-style-template.md#205-依赖属性优先级链整合)。
* 附加属性：`Grid::ColumnProperty`、`Canvas::LeftProperty` 等。
* 值类型为 `mine::Variant`，常用类型有专门 `set_<typed>` 路径以避免装箱。
* 通知：`PropertyChanged` 信号 + 受影响的布局/绘制标志。

## 9.2 属性继承

* 类型继承：子类自动继承父类属性集合。
* 值继承：`inherits = true` 的属性沿可视化树向下继承（如 `FontSize`）。

## 9.3 反射桥

* `MINE_REFLECT_DECL()` 宏在头文件声明类型反射；
* `MINE_REFLECT_IMPL(Type, props..., methods..., events...)` 在 cpp 注册；
* MML 编译期通过反射生成强类型访问代码（非运行期反射）。

## 9.4 数据绑定（mine.ui.binding）

```cpp
class BindingExpression {
public:
    Function<Variant()>            getter;
    Function<void(const Variant&)> setter;     // TwoWay 时
    Vector<PropertyDependency>     deps;       // (源对象, 属性) 列表
    BindingMode                    mode;
    IConverter*                    converter = nullptr;
    Variant                        fallback;
};
```

* MML 的绑定表达式被 `mmlc` 翻译成上述结构 + lambda。
* 依赖追踪通过订阅 `PropertyChanged` 实现，自动取消订阅（弱引用 + scope guard）。
* 中间路径节点变更时重建后续订阅。
* 双向绑定使用 `BindingDirection` 防止环路（更新中的属性临时屏蔽反向回调）。

## 9.5 INotify 接口

* `INotifyPropertyChanged`：所有 `ViewModelBase` 默认实现。
* `INotifyCollectionChanged`：`ObservableCollection<T>` 实现，支持批处理 `BeginUpdate/EndUpdate`。

## 9.6 转换器

```cpp
struct IConverter {
    virtual Variant convert(Variant const& v, TypeInfo target, StringView param) = 0;
    virtual Variant convert_back(Variant const& v, TypeInfo target, StringView param) = 0;
};
```

MML 中：

```mml
text: bind(vm.Bytes, converter: BytesToHumanReadable, parameter: "MB");
```

## 9.7 资源与样式驱动

* 资源以**树形作用域**保存（Application → Window → Element）。
* MML 的 `{StaticResource Key}` 编译期解析；`{DynamicResource Key}` 编译为运行期查找 + 订阅资源变更。

## 9.8 性能策略

* 默认值不存储到实例字段（节省内存）。
* 通过 `SmallVector<Slot>` 存储有效值条目（多数控件 < 16 条）。
* 布局/绘制失效合并：`InvalidateMeasure/Arrange/Render` 入队，单次提交。
* 绑定回调延迟到下一帧（避免链式风暴）。
