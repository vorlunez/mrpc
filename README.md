# This is a mini rpc architure
just for study
by using AI to build a mini rpc arch quickly
I will refer to grpc,brpc,thirft

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
