# This is a mini rpc architure
just for study
by using AI to build a mini rpc arch quickly
I will refer to grpc,brpc,thirft

## epoll TCP server（Linux）

```sh
make build/epoll_server
./build/epoll_server 8080
# 另一个终端连接，输入内容即可收到原样回复
nc 127.0.0.1 8080
```

默认端口为 8080，仅监听本机。使用 Ctrl+C 或 SIGTERM 退出。
`src/TcpServer.cc` 是单线程 TCP echo 示例：非阻塞 socket + epoll LT，
一个事件循环处理多个连接，默认采用 TCP echo 模式。
TCP 是字节流，单次 recv 不代表一条完整请求。
服务端终端会打印每次收到的字节块，例如 `[recv fd=5, bytes=6] hello`；
这里展示的是一次 recv 的结果，长消息可能分成多次打印。

每个连接有待发送回复时暂停读，回复发送完后恢复读，避免慢客户端积压更多请求。
处理部分写入、EAGAIN、对端断开；客户端关闭写方向后，服务端发送完已接收数据再关闭连接。
连接数量暂无上限；耗尽文件描述符等监听错误会让程序报错退出。
运行 `make test-server` 验证网络收发，需要 Python 3。

职责划分：

- `src/Epoll.h` / `src/Epoll.cc`：封装 epoll 实例的创建、销毁、添加、修改、移除和等待事件；不管理 socket 或处理 TCP 数据。
- `src/TcpServer.h` / `src/TcpServer.cc`：管理监听和客户端 socket、连接缓冲和 TCP 收发，通过 `Epoll` 操作事件。
- `src/main.cc`：处理命令行参数和退出信号，启动服务。

## HTTP/1.1

```sh
make build/epoll_server
./build/epoll_server 8080 --http
```

在另一个终端发送请求：

```sh
curl -i 'http://127.0.0.1:8080/ping'
curl -i http://127.0.0.1:8080/print -H 'Content-Type: text/plain' --data-binary '你好，mrpc'
curl -I http://127.0.0.1:8080/ping
```

入口文件注册两个业务回调：

| 方法 | 路径 | 行为 |
| --- | --- | --- |
| GET（也支持 HEAD） | `/ping` | 返回 `pong` 和换行，HEAD 仅返回响应头 |
| POST | `/print` | 在服务端输出 `[print] 正文`，返回 JSON 确认并回传正文 |

`/print` 的响应示例：

```json
{"body":"你好，mrpc","printed":true}
```

路由按方法和路径精确匹配，忽略查询字符串：`/ping?q=1` 匹配 `/ping`，
`/ping/extra` 不匹配。未知路径返回 404，已知路径的方法不匹配返回 405 和 Allow 响应头。
HEAD 在未单独注册时复用 GET 回调，序列化时省略正文；GET 回调应避免副作用。
注册接口为 `HttpServer::addRoute(method, path, callback)`，回调接收 `const HttpRequest&`，返回 `HttpResponse`。
例如在 `run()` 前注册：

```cpp
mrpc::HttpServer server;
server.addRoute("GET", "/ping", [](const mrpc::HttpRequest&) {
    mrpc::HttpResponse response;
    response.headers["Content-Type"] = "text/plain; charset=utf-8";
    response.body = "pong\n";
    return response;
});
```

同一方法和路径不允许重复注册，启动后不修改路由表。
回调在事件循环线程同步执行，应保持简短；回调抛出异常时返回 500 并关闭该连接。

- `HttpRequest.h/.cc`：从缓冲区解析一个请求，返回 `Incomplete`、`Complete` 或 `Error`；成功时通过 `consumed` 返回本条请求的字节数，保留后续请求的边界。字段名统一转小写，支持大小写不敏感的查询。
- `HttpResponse.h/.cc`：生成状态行、响应头和响应体，自动计算字节长度、生成 Date 与 Connection；HEAD 省略正文，204/304 不发送正文及 Content-Length，205 发送零长度响应。拒绝包含非法控制字符的响应头。
- `HttpServer.h/.cc`：维护 HTTP 与 TCP 的边界，通过 TCP 的 Handler 回调解析请求，交由 Router 分发并编码回复。TcpServer 与 Epoll 不依赖 HTTP 类型。
- `Router.h/.cc`：保存方法、路径与业务回调的映射，负责匹配和 404/405；具体 `/ping`、`/print` 行为注册在 `main.cc`。

支持 CRLF 格式的 HTTP/1.1 请求、Host 检查、Content-Length、chunked 解码（含扩展及 trailer 语法检查）、默认长连接、Connection: close 和按顺序处理流水线请求。
请求头上限 16 KiB、解码后正文上限 1 MiB；chunked 额外限制编码开销，trailer 单独限制为 16 KiB，校验后丢弃。
拒绝重复 Host/Content-Length/Transfer-Encoding 和 Content-Length 与 Transfer-Encoding 同时出现的请求。
格式错误或未完成请求遇到 EOF 返回 400；过大正文返回 413、过大头返回 431，然后关闭连接。

这是供学习的 HTTP/1.1 子集：只接受 origin-form 路径和 `OPTIONS *`，不提供代理、CONNECT、Upgrade、TLS 或 HTTP/1.0。
方法是否支持由路由表决定；其他传输编码返回 501，Expect 请求返回 417，版本不支持返回 505。
body 保留原始字节；`/print` 把 body 放入 JSON 字符串，不自动解析其中的 JSON，无效 UTF-8 在响应中替换。
当前解析器会重解析已积累的缓冲，尚未实现连接超时或大正文的流式处理。

```sh
make test-http   # 编解码单测 + 实际 HTTP 网络测试
make test-server # 原有 TCP echo 网络回归
make test        # mlog 测试 + HTTP 编解码单测
```

HTTP 模式使用 curl 或 HTTP 客户端测试；`build/client` 按原始字节数等待 echo，用于默认的 TCP echo 模式。

## 单客户端通信

```sh
make build/epoll_server build/client
# 终端一
./build/epoll_server 8080
# 终端二
./build/client 127.0.0.1 8080
```

`client` 默认连接 `127.0.0.1:8080`。输入一行并回车发送，显示服务端原样回复，
同一个连接可以连续通信；在空行提示处按 Ctrl+D 结束输入并关闭写方向，
收到服务端关闭通知后退出。也支持管道输入：

```sh
printf 'hello\n你好，mrpc\n' | ./build/client
```

`src/client.cc` 使用阻塞 socket，处理部分发送、部分接收和 EINTR。
大行分块交替发送、接收；按发送字节数收齐 echo 回复，这依赖当前 echo 行为，
后续接入 HTTP/RPC 时需要改成对应的消息解析。目前只支持 IPv4 数字地址，未设置网络超时。

## JSON 库

项目内置 `nlohmann/json v3.12.0`，采用纯头文件方式，无需系统安装或额外链接。
Makefile 已配置 include 路径，在项目源码中可以直接使用：

```cpp
#include <nlohmann/json.hpp>

nlohmann::json request = {{"method", "echo"}, {"message", "hello"}};
std::string payload = request.dump();
auto decoded = nlohmann::json::parse(payload);
```

第三方文件位于 `third_party/nlohmann_json/`，附带 MIT 许可证、来源和 SHA-256。
HTTP 演示使用该库生成 JSON 响应，默认 TCP 模式保持 echo 行为。

## mlog

在项目根目录运行 `make test`。测试程序和静态库生成在 `build/`，测试日志生成在 `tests/`。

```cpp
#include "Logger.h"

mlog::Logger logger(std::chrono::milliseconds(100));
auto appender = logger.getFileAppender("tests/example.log");
appender->append("hello", mlog::INFO);
```

每个 Logger 拥有一个 EventBase 刷新线程，默认周期为 100 ms。写日志时先入队；
单个 FileAppender 的待写缓冲达到 2 MiB 时，通过条件变量通知刷新线程提前写出。
该阈值用于唤醒，不是队列容量上限。使用 `Logger::getInstance()` 时共享默认的 Logger。

EventBase 构造时启动，`run()` 重复调用不会增加线程。`stop()` 可重复或并发调用，
会唤醒线程、完成最后一轮刷新并 join；停止后不可重新启动或登记新 Appender。
FileAppender 的 `flush(true)` 可手动同步刷新，与后台刷新通过独立的文件锁串行执行。
这里的刷新是写入文件并刷新 C++ 流缓冲，不提供 `fsync` 的断电持久性保证。

EventBase 用弱引用登记 Appender，刷新期间临时持有强引用；Appender 用弱引用访问通知状态，
避免循环引用和访问已析构的 EventBase。Appender 析构时刷出剩余日志；未登记或所属 EventBase
已停止、销毁时，写入退回同步刷新。正常退出应先结束并 join 业务写入线程，再销毁 Logger；
不得在对象析构期间继续通过该对象调用其方法。单独持有的 Appender 可以比 Logger 存活更久。
