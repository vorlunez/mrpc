#pragma once

#include "Appender.h"
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <sstream>
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
    static std::shared_ptr<Appender> getConsoleAppender();
    // Thread-safe process-wide sink. nullptr restores the default console sink.
    static void setGlobalAppender(std::shared_ptr<Appender> appender);
    static std::shared_ptr<Appender> getGlobalAppender();

private:
    std::unordered_map<std::string, std::shared_ptr<Appender>> m_file_appender_map;
    std::mutex m_mutex;
    // 最后声明，先停止刷新线程，再析构上面的 Appender 缓存。
    std::unique_ptr<EventBase> m_event_base;
};

// One temporary per LOG expression; submits exactly one message at the semicolon.
class LogMessage {
public:
    LogMessage(LogLevel level, const char* file, int line);
    ~LogMessage() noexcept;
    LogMessage(const LogMessage&) = delete;
    LogMessage& operator=(const LogMessage&) = delete;
    std::ostringstream& stream() { return m_stream; }

private:
    int m_errno;
    LogLevel m_level;
    std::shared_ptr<Appender> m_appender;
    std::ostringstream m_stream;
};

} // namespace mlog

#define LOG(level) ::mlog::LogMessage(::mlog::level, __FILE__, __LINE__).stream()
