# 12 — 数据层（SQLite + ORM）

## 12.1 模块拆分

| 模块 | 职责 |
|------|------|
| `mine.data.sqlite` | 直接封装 SQLite C API，提供安全的 RAII 句柄 |
| `mine.data.orm`    | 元数据驱动 ORM，查询构建器 |
| `mine.data.cache`  | 内存/磁盘缓存（LRU/ARC） |
| `mine.data.migrate`| Schema 迁移 |

## 12.2 SQLite 高层封装

```cpp
namespace mine::data::sqlite {

class Database {
public:
    static Result<Database, Error> open(StringView path, OpenFlags = OpenFlags::Default);
    Statement prepare(StringView sql);
    int       execute(StringView sql);                // 直接 exec，无返回值
    Transaction begin(IsolationLevel = Deferred);
    int64_t   last_insert_rowid() const;
    int       changes() const;
};

class Statement {
public:
    Statement& bind(int idx, int64_t v);
    Statement& bind(int idx, double v);
    Statement& bind(int idx, StringView v);
    Statement& bind(int idx, Span<const std::byte> blob);
    Statement& bind(StringView name, ...);            // 命名参数
    Result<bool, Error> step();                       // true: HasRow
    template<class T> T column(int idx) const;
    Iter<RowView> rows();
    template<class T> Iter<T> map_rows();             // 支持 lambda/反射
};

class Transaction {                                   // RAII，未 commit 自动 rollback
    Status commit();
    void   rollback();
};
}
```

便捷 API：

```cpp
auto db = sqlite::Database::open("app.db").value();
db.execute(R"(CREATE TABLE IF NOT EXISTS users(id INTEGER PRIMARY KEY, name TEXT))");

auto stmt = db.prepare("INSERT INTO users(name) VALUES (?)");
stmt.bind(1, "alice").step();

for (auto row : db.prepare("SELECT id,name FROM users").rows()) {
    int64_t id = row.get<int64_t>(0);
    auto    name = row.get<String>(1);
}
```

## 12.3 错误处理

* 所有可失败 API 返回 `Result<T, sqlite::Error>`，`Error` 含 SQLite errcode/msg。
* 不抛异常。
* 锁/Busy：默认 `busy_timeout(5s)`，可配置。

## 12.4 ORM

```cpp
struct User {
    int64_t id;
    String  name;
    String  email;
    Optional<int> age;

    MINE_TABLE("users",
        MINE_PK(id),
        MINE_COL(name, "name"),
        MINE_COL(email, "email"),
        MINE_COL(age,   "age"));
};

orm::Session s(db);
auto u = User{ .name="bob", .email="b@x.com" };
s.insert(u);                            // 设置 id

auto users = s.from<User>()
              .where(orm::col(&User::name).like("a%"))
              .order_by(orm::col(&User::id).desc())
              .limit(20)
              .to_vec();
```

* 强类型查询构建器，列引用为成员指针。
* 支持 join、子查询、表达式、聚合。
* 查询编译期生成 SQL，参数绑定安全。

## 12.5 异步

* `AsyncDatabase` 在专用线程执行：
  ```cpp
  Task<int64_t> insert_user_async(AsyncDatabase& db, User u) {
      co_return co_await db.execute_async("INSERT INTO ...", u.name, u.email);
  }
  ```
* 通过 `mine.async::Dispatcher` 自动跳回 UI 线程。

## 12.6 迁移

```cpp
migrate::Migrator m(db);
m.add(1, [](Database& d) {
    d.execute("CREATE TABLE users(...)");
});
m.add(2, [](Database& d) {
    d.execute("ALTER TABLE users ADD COLUMN email TEXT");
});
m.migrate_to_latest();    // 内部维护 schema_version
```

## 12.7 缓存

* `LruCache<K,V>`、`ArcCache<K,V>`：泛型，线程安全。
* 磁盘缓存：基于 SQLite 单表 `(key BLOB PK, value BLOB, meta BLOB, mtime INT)`。

## 12.8 加密 / 备份

* 可选启用 SQLCipher（构建选项 `MINE_DATA_SQLCIPHER`）。
* `Backup` API：基于 `sqlite3_backup_*`，支持热备份。

## 12.9 安全（OWASP）

* 永远使用绑定参数（防 SQL 注入）。
* 字符串拼接形式的 `execute(...)` 仅接受**编译期常量**（`StringLiteral`），动态字符串路径关闭。
* 文件路径检查：禁止跨越 app 数据目录。
