#include "include/Appender.h"
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

FileAppender::~FileAppender() = default;

void FileAppender::append(std::string& msg, LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto formatter = Formatter::format(msg, level);
    // m_pending_list.push_back(formatter);
    m_file << formatter;
}

void FileAppender::append(const char* msg, LogLevel level) {
    auto a = std::string(msg);
    append(a, level);
}

void FileAppender::flush(bool /*force*/) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_file.flush();
}

ConsoleAppender& ConsoleAppender::getInstance() {
    static ConsoleAppender app;
    return app;
}

void ConsoleAppender::append(std::string& msg, LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::cout << Formatter::format(msg, level);
}

void ConsoleAppender::append(const char* msg, LogLevel level) {
    auto text = std::string(msg);
    append(text, level);
}

}; // namespace mlog
