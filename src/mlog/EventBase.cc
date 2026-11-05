#include "EventBase.h"
#include "include/Appender.h"

#include <stdexcept>

namespace mlog {
bool FlushSignal::notify() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (stopping) {
            return false;
        }
        requested = true;
    }
    cv.notify_one();
    return true;
}

EventBase::EventBase(std::chrono::milliseconds interval)
    : m_interval(interval), m_signal(std::make_shared<FlushSignal>()) {
    if (interval.count() <= 0) {
        throw std::invalid_argument("EventBase: interval must be positive");
    }
    run();
}

EventBase::~EventBase() { stop(); }

void EventBase::run() {
    std::lock_guard<std::mutex> lifecycle_lock(m_lifecycle_mutex);
    std::lock_guard<std::mutex> lock(m_signal->mutex);
    if (m_signal->stopping) {
        throw std::logic_error("EventBase: cannot restart after stop");
    }
    if (!m_thread.joinable()) {
        m_thread = std::thread(&EventBase::loop, this);
    }
}

void EventBase::stop() {
    // 串行化并发 stop，只有一个调用者执行 join。
    std::lock_guard<std::mutex> lifecycle_lock(m_lifecycle_mutex);
    {
        std::lock_guard<std::mutex> lock(m_signal->mutex);
        m_signal->stopping = true;
    }
    m_signal->cv.notify_one();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void EventBase::addAppender(std::shared_ptr<FileAppender> fa) {
    if (!fa) {
        throw std::invalid_argument("EventBase: null appender");
    }
    std::lock_guard<std::mutex> lock(m_signal->mutex);
    if (m_signal->stopping) {
        throw std::logic_error("EventBase: already stopped");
    }
    std::lock_guard<std::mutex> appender_lock(fa->m_mutex);
    if (auto signal = fa->m_signal.lock()) {
        if (signal == m_signal) {
            return;
        }
        throw std::logic_error("EventBase: appender already registered elsewhere");
    }
    m_appenders.push_back(fa);
    fa->m_signal = m_signal;
}

void EventBase::loop() {
    std::unique_lock<std::mutex> lock(m_signal->mutex);
    for (;;) {
        // 谓词状态和 wait 使用同一把锁，避免通知发生在等待前而丢失。
        m_signal->cv.wait_for(lock, m_interval,
                              [this] { return m_signal->requested || m_signal->stopping; });
        const bool stopping = m_signal->stopping;
        m_signal->requested = false;
        std::vector<std::shared_ptr<FileAppender>> appenders;
        for (auto it = m_appenders.begin(); it != m_appenders.end();) {
            if (auto appender = it->lock()) {
                appenders.push_back(std::move(appender));
                ++it;
            } else {
                it = m_appenders.erase(it);
            }
        }
        lock.unlock();
        for (const auto& appender : appenders) {
            appender->flush(true);
        }
        // 强引用也在 EventBase 锁外释放，析构可能需要刷盘。
        appenders.clear();
        if (stopping) {
            return;
        }
        lock.lock();
    }
}
} // namespace mlog
