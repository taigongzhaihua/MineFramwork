# 13 — 网络模块

## 13.1 模块拆分

```
mine.net.socket   ── 异步 TCP/UDP/Unix socket
mine.net.tls      ── TLS 适配（mbedTLS / OpenSSL 二选一）
mine.net.dns      ── 异步 DNS 解析
mine.net.http     ── HTTP/1.1 + HTTP/2 客户端，简易服务端
mine.net.ws       ── WebSocket
mine.net.rest     ── REST 客户端高层 + 模型映射（与 mine.meta 对接）
mine.net.rpc      ── 二进制 RPC（可选）
```

所有 IO 基于 `mine.async`：协程 + IOCP/epoll/kqueue。

## 13.2 Socket 抽象

```cpp
class TcpSocket {
public:
    static Task<Result<TcpSocket>> connect(Endpoint, Duration timeout);
    Task<Result<size_t>> read (Span<std::byte> buf);
    Task<Result<size_t>> write(Span<const std::byte> buf);
    void close();
};

class TcpListener {
public:
    static Result<TcpListener> bind(Endpoint);
    Task<Result<TcpSocket>> accept();
};
```

* 非阻塞 + 协程，零回调风格。
* `Endpoint` ABI 安全包装 v4/v6/Unix。

## 13.3 TLS

```cpp
class TlsClient {
    static Task<Result<TlsClient>> handshake(TcpSocket sock, TlsConfig const&);
    Task<Result<size_t>> read(Span<std::byte>);
    Task<Result<size_t>> write(Span<const std::byte>);
};
```

* `TlsConfig`：CA bundle、ALPN、SNI、客户端证书、会话票据。
* 默认禁用 TLS<1.2、不安全 cipher。

## 13.4 HTTP 客户端

```cpp
HttpClient client;
client.default_headers()["User-Agent"] = "MineApp/1.0";

auto resp = co_await client.send(
    HttpRequest::get("https://api.example.com/users/1")
        .header("Accept", "application/json"));
if (resp && resp->status == 200) {
    auto user = json::deserialize<User>(resp->body);
}
```

特性：

* 协程为先；`Future`/`Task<T>`。
* HTTP/2（基于 nghttp2 或自实现，可裁剪开关）。
* 自动 Cookie Jar、自动重定向、Gzip/Brotli 解压。
* 透明缓存（依据 `Cache-Control`），可关闭。
* 进度回调、上传/下载流。

## 13.5 WebSocket

```cpp
auto ws = co_await WebSocket::connect("wss://chat.example.com");
co_await ws.send_text("hello");
while (auto msg = co_await ws.receive()) {
    if (msg->is_text()) handle(msg->text());
}
```

* 支持 ping/pong、permessage-deflate。
* 自动重连策略可配（指数退避）。

## 13.6 REST 高层

```cpp
struct GetUser : Request<User> {
    static constexpr auto method = "GET";
    static constexpr auto path   = "/users/{id}";
    int id;
};
auto user = co_await rest.send(GetUser{ .id = 1 });
```

* 反射驱动序列化（`mine.meta` JSON），编译期校验路径变量。

## 13.7 服务端（轻量）

非主用例，但提供：

* `HttpServer`：基础路由 + 静态文件，便于调试/嵌入式管理界面。
* 不与 `nginx`/`envoy` 竞争；功能边界明确。

## 13.8 安全

* 默认证书校验开启；用户必须显式 `dangerous_skip_verify(true)` 才关闭。
* HSTS、SNI、CRL/OCSP 可选。
* 输入解析（HTTP header/body）严格限制大小，防 OOM 攻击（OWASP A04）。
* URL 解析使用白名单 scheme，避免 SSRF（应用层使用须自行限制）。

## 13.9 与 UI/MVVM 集成

* `mine::async::Task<T>` 与 UI 线程 `Dispatcher` 无缝衔接：
  ```cpp
  Task<void> LoginVM::login() {
      Busy = true;
      auto r = co_await api.login_async(UserName, Password);
      // 自动回到 UI 线程
      Busy = false;
      if (r) navigate.navigate_to("/main", r->token);
  }
  ```
* 网络错误 → `IErrorReporter` 服务（可对接 Toast/对话框/上报）。
