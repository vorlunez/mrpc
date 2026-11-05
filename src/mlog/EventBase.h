#pragma once

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace mlog {
class FileAppender;

// 独立于 EventBase 对象的通知状态，Appender 只保存它的弱引用。
struct FlushSignal {
    bool notify();

    std::mutex mutex;
    std::condition_variable cv;
    bool requested = false;
    bool stopping = false;
};

class EventBase {
public:
    // 构造时启动唯一的刷新线程；run() 可以重复调用，但 stop() 后不可重启。
    explicit EventBase(std::chrono::milliseconds interval = std::chrono::milliseconds(100));
    ~EventBase();
    EventBase(const EventBase&) = delete;
    EventBase& operator=(const EventBase&) = delete;

    void run();
    void stop();
    void addAppender(std::shared_ptr<FileAppender> fa);

private:
    void loop();

    const std::chrono::milliseconds m_interval;
    std::shared_ptr<FlushSignal> m_signal;
    // 登记表由 m_signal->mutex 保护，刷新时临时提升为强引用。
    std::vector<std::weak_ptr<FileAppender>> m_appenders;
    std::mutex m_lifecycle_mutex;
    std::thread m_thread;
};
} // namespace mlog
