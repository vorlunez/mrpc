#include "include/Logger.h"
#include "EventBase.h"
#include <cerrno>
#include <cstring>
#include <unistd.h>
namespace mlog {
namespace {
struct GlobalAppender {
    std::mutex mutex;
    std::shared_ptr<Appender> appender = Logger::getConsoleAppender();
};
GlobalAppender& globalAppender() {
    static GlobalAppender state;
    return state;
}
} // namespace

Logger::Logger(std::chrono::milliseconds interval) : m_event_base(new EventBase(interval)) {}

Logger::~Logger() = default;

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
    m_event_base->addAppender(file_appender_sp);
    m_file_appender_map[name] = file_appender_sp;

    return file_appender_sp;
}

std::shared_ptr<Appender> Logger::getFileAppender(const char* name) {
    auto a = std::string(name);
    return getFileAppender(a);
}

std::shared_ptr<Appender> Logger::getConsoleAppender() {
    // Owning shared_ptr: an in-flight LogMessage can outlive a sink switch.
    return ConsoleAppender::sharedInstance();
}

void Logger::setGlobalAppender(std::shared_ptr<Appender> appender) {
    if (!appender) appender = getConsoleAppender();
    auto& state = globalAppender();
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.appender.swap(appender);
    }
    // Destroy the old sink outside the global lock (it may flush or log).
}

std::shared_ptr<Appender> Logger::getGlobalAppender() {
    auto& state = globalAppender();
    std::lock_guard<std::mutex> lock(state.mutex);
    return state.appender;
}

LogMessage::LogMessage(LogLevel level, const char* file, int line)
    : m_errno(errno), m_level(level), m_appender(Logger::getGlobalAppender()) {
    const char* base = std::strrchr(file, '/');
    m_stream << (base ? base + 1 : file) << ':' << line << " ";
    errno = m_errno;
}

LogMessage::~LogMessage() noexcept {
    try {
        auto message = m_stream.str();
        m_appender->append(message, m_level);
    } catch (...) {
        // Logging failure must not throw from a destructor or log recursively.
        static const char fallback[] = "mlog: failed to write log message\n";
        const auto ignored = ::write(STDERR_FILENO, fallback, sizeof(fallback) - 1);
        (void)ignored;
    }
    errno = m_errno;
}

} // namespace mlog
