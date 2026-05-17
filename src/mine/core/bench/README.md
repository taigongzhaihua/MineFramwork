# bench

此目录用于放置 mine.core 的性能基准。

建议优先补充以下基准：

- String 的构造、追加、扩容与拷贝成本。
- Variant 在内联存储和堆存储两种路径下的构造与复制成本。
- 自定义 Allocator 与 SystemAllocator 在高频小对象分配上的差异。