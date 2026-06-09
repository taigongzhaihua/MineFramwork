# mine.core —— 基础类型与工具模块 详细接口文档

## 模块导航

\mine.core\ 是 MineFramework 最底层的模块，提供所有其他模块共同依赖的基础类型、内存管理和工具设施。本目录包含详细的**逐类、逐方法**的接口参考文档。

### 📚 文档清单

| 文件 | 类型 | 用途 | 重要性 |
|------|------|------|--------|
| [01-TypeId.md](01-TypeId.md) | \TypeId\ | 无 RTTI 的编译期类型标识符 | ⭐⭐⭐ |
| [02-StringView.md](02-StringView.md) | \StringView\ | 非拥有的 UTF-8 字符串视图 | ⭐⭐⭐ |
| [03-Span.md](03-Span.md) | \Span<T>\ | 非拥有的连续内存视图 | ⭐⭐⭐ |
| [04-Status-Result.md](04-Status-Result.md) | \Status\, \Result<T,E>\ | 错误传递机制 | ⭐⭐⭐ |
| [05-Memory.md](05-Memory.md) | \IAllocator\, \OwnedPtr<T>\ | 内存管理接口与智能指针 | ⭐⭐⭐ |
| [06-Function.md](06-Function.md) | \Function<R(Args...)>\ | 类型擦除的函数包装器 | ⭐⭐ |
| [07-Variant.md](07-Variant.md) | \Variant\ | 类型擦除容器 | ⭐⭐ |
| [08-Utilities.md](08-Utilities.md) | \Pimpl<>\, \Assert\, 其他辅助 | 工具类和宏 | ⭐ |

### 🎯 核心设计原则

1. **无 RTTI** (\/GR-\)
2. **无异常**
3. **ABI 安全**
4. **零开销抽象**
5. **constexpr 友好**

详见各文档的"设计特点"章节。

---

**文档版本**：1.0  
**最后更新**：2026-06-09
