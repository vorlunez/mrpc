#pragma once
#include "LogLevel.h"
#include <fstream>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>

namespace mlog {
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
    std::list<std::string> m_pending_list;
    std::list<std::string> m_working_list;
    std::mutex m_mutex;
    std::ofstream m_file;

private:
    static const int max_list_size = 2 * 1024 * 1024;
};

class ConsoleAppender : public Appender {
public:
    static ConsoleAppender& getInstance();
    void append(std::string& msg, LogLevel level) override;
    void append(const char* msg, LogLevel level) override;

private:
    std::mutex m_mutex;
};
} // namespace mlog
