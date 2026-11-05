#include "include/Appender.h"
#include "EventBase.h"
#include "Formatter.h"
#include <iostream>
#include <stdexcept>
namespace mlog {

FileAppender::FileAppender(const char* file_path)
    : FileAppender(file_path ? std::string(file_path) : std::string()) {}
FileAppender::FileAppender(const std::string& file_path) {
    if (file_path.empty()) {
        throw std::invalid_argument("FileAppender: file path is empty");
    }
    m_file.open(file_path, std::ios::out | std::ios::app);
    if (!m_file.is_open() || !m_file.good()) {
        throw std::runtime_error("FileAppender: failed to open file: " + file_path);
    }
}

FileAppender::~FileAppender() { flush(true); }

void FileAppender::append(std::string& msg, LogLevel level) {
    auto formatter = Formatter::format(msg, level);
    std::shared_ptr<FlushSignal> signal;
    bool need_notify;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pending_list.push_back(std::move(formatter));
        m_pending_bytes += m_pending_list.back().size();
        need_notify = m_pending_bytes >= max_pending_bytes;
        signal = m_signal.lock();
    }
    // 不在队列锁内获取通知锁，避免与 addAppender 的锁顺序形成环。
    if (!signal) {
        flush(true);
    } else {
        // 即使未达到阈值也检查停止状态；停止后再写入时同步落盘。
        bool stopped;
        {
            std::lock_guard<std::mutex> lock(signal->mutex);
            stopped = signal->stopping;
        }
        if (stopped || (need_notify && !signal->notify())) {
            flush(true);
        }
    }
}

void FileAppender::append(const char* msg, LogLevel level) {
    auto a = std::string(msg);
    append(a, level);
}

void FileAppender::flush(bool /*force*/) {
    std::lock_guard<std::mutex> flush_lock(m_flush_mutex);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pending_list.swap(m_working_list);
        m_pending_bytes = 0;
    }
    for (const auto& message : m_working_list) {
        m_file << message;
    }
    m_file.flush();
    m_working_list.clear();
}

ConsoleAppender& ConsoleAppender::getInstance() {
    return *sharedInstance();
}

std::shared_ptr<ConsoleAppender> ConsoleAppender::sharedInstance() {
    static auto appender = std::make_shared<ConsoleAppender>();
    return appender;
}

void ConsoleAppender::append(std::string& msg, LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // glog-style console diagnostics use stderr; stdout remains available for data.
    std::cerr << Formatter::format(msg, level) << std::flush;
}

void ConsoleAppender::append(const char* msg, LogLevel level) {
    auto text = std::string(msg);
    append(text, level);
}

}; // namespace mlog
