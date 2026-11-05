#include "include/Logger.h"
namespace mlog {

Logger& Logger::getInstance() {
    static Logger log;
    return log;
}

std::shared_ptr<Appender> Logger::getFileAppender(std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_file_appender_map.find(name);
    if (it != m_file_appender_map.end()) {
        return it->second;
    }
    auto file_appender_sp = std::make_shared<FileAppender>(name);
    m_file_appender_map[name] = file_appender_sp;

    return file_appender_sp;
}

std::shared_ptr<Appender> Logger::getFileAppender(const char* name) {
    auto a = std::string(name);
    return getFileAppender(a);
}

std::shared_ptr<Appender> Logger::getConsoleAppender() {
    static std::shared_ptr<Appender> appender(&ConsoleAppender::getInstance(), [](Appender*) {});
    return appender;
}
void Logger::setGlobalAppender(std::shared_ptr<Appender>) {}

} // namespace mlog
