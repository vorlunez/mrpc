#pragma once

#include "Appender.h"
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace mlog {
class EventBase;
class Logger {
public:
    static Logger& getInstance();
    explicit Logger(std::chrono::milliseconds interval = std::chrono::milliseconds(100));
    ~Logger();

public:
    std::shared_ptr<Appender> getFileAppender(const char* name);
    std::shared_ptr<Appender> getFileAppender(std::string& name);
    std::shared_ptr<Appender> getConsoleAppender();
    void setGlobalAppender(std::shared_ptr<Appender>);

private:
    std::unordered_map<std::string, std::shared_ptr<Appender>> m_file_appender_map;
    std::mutex m_mutex;
    // 最后声明，先停止刷新线程，再析构上面的 Appender 缓存。
    std::unique_ptr<EventBase> m_event_base;
};

} // namespace mlog
