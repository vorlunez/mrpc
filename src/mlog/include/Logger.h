#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <mutex>
#include <unordered_map>
#include "Appender.h"

namespace mlog {
class Logger {
public:
    static std::shared_ptr<Appender> getFileAppender(const char* name);
    static std::shared_ptr<Appender> getFileAppender(std::string& name);
    static std::shared_ptr<Appender> getConsoleAppender();
    static void setGlobalAppender(std::shared_ptr<Appender>);
private:
    std::unordered_map<std::string, std::shared_ptr<Appender>> m_appender_map;
    std::mutex m_mutex;
};

}