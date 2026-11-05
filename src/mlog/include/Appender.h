#pragma once
#include "LogLevel.h"
#include <fstream>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace mlog {
struct FlushSignal;
class EventBase;
class Logger;
class Appender {
public:
    virtual ~Appender() = default;
    virtual void append(std::string& msg, LogLevel level) = 0;
    virtual void append(const char* msg, LogLevel level) = 0;
};

class FileAppender : public Appender {
public:
    FileAppender(const std::string& file_path);
    FileAppender(const char* file_path);
    ~FileAppender();

public:
    void append(std::string& msg, LogLevel level) override;
    void append(const char* msg, LogLevel level) override;
    void flush(bool force);

private:
    friend class EventBase;
    std::list<std::string> m_pending_list;
    std::list<std::string> m_working_list;
    std::mutex m_mutex;
    // 所有 flush（后台、手动、析构）共用此锁，保证输出顺序和文件流安全。
    std::mutex m_flush_mutex;
    std::ofstream m_file;
    std::weak_ptr<FlushSignal> m_signal;
    std::size_t m_pending_bytes = 0;

private:
    static const std::size_t max_pending_bytes = 2 * 1024 * 1024;
};

class ConsoleAppender : public Appender {
public:
    static ConsoleAppender& getInstance();
    void append(std::string& msg, LogLevel level) override;
    void append(const char* msg, LogLevel level) override;

private:
    friend class Logger;
    static std::shared_ptr<ConsoleAppender> sharedInstance();
    std::mutex m_mutex;
};
} // namespace mlog
