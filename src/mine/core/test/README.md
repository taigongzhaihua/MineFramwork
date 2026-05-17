# test

此目录用于放置 mine.core 的 doctest 单元测试。

当前测试覆盖重点：

- StringView 与 Span 的视图语义。
- Status / Result / MINE_TRY 的错误传播路径。
- Pimpl 的深拷贝行为。
- Allocator、String、Optional、Variant 的基础生命周期语义。

新增测试时应优先覆盖：

- 边界输入，例如空字符串、零长度 Span、空 Optional、空 Variant。
- 自引用或重叠缓冲区场景，例如 String::assign / append 的自切片追加。
- 不依赖异常与 RTTI 的路径是否仍然保持正确语义。