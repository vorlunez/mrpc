#pragma once
#include <string>
#include <list>
#include <unordered_map>
#include <mutex>
enum LogLevel{
    INFO,
    WARNING,
    ERROR
};

class Appender {
public:
    virtual void append(std::string& msg, LogLevel level);
    virtual void append(const char* msg, LogLevel level);
};

class FileAppender : public Appender {
public:
    FileAppender(std::string& file_path);
    FileAppender(const char* file_path);
    ~FileAppender();
public:
    void append(std::string& msg, LogLevel level) override;
    void append(const char* msg, LogLevel level) override;
private:
    std::list<std::string> m_pending_list;
    std::list<std::string> m_working_list;
    std::mutex m_mutex;
private:
    static const int max_list_size = 2 * 1024 * 1024;
};

class ConsoleAppender : public Appender {

};