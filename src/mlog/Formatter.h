#pragma once
#include "include/LogLevel.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <sys/syscall.h>
#include <unistd.h>

namespace mlog {
class Formatter {
public:
    static std::string format(std::string& msg, LogLevel level) {
        // glog 风格的最小格式：I1105 12:34:56.123456 12345] message\n
        const auto now = std::chrono::system_clock::now();
        const auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
        const auto micros =
            std::chrono::duration_cast<std::chrono::microseconds>(now - seconds).count();
        const auto time = std::chrono::system_clock::to_time_t(seconds);
        std::tm local{};
        localtime_r(&time, &local);

        char severity = 'I';
        switch (level) {
        case INFO:
            severity = 'I';
            break;
        case WARNING:
            severity = 'W';
            break;
        case ERROR:
            severity = 'E';
            break;
        }

        std::ostringstream out;
        out << severity << std::put_time(&local, "%m%d %H:%M:%S") << '.' << std::setfill('0')
            << std::setw(6) << micros << ' ' << syscall(SYS_gettid) << "] " << msg;
        if (msg.empty() || msg.back() != '\n') {
            out << '\n';
        }
        return out.str();
    }
};

} // namespace mlog
