#include "Logger.h"

#include <chrono>
#include <fstream>
#include <set>
#include <thread>

int main() {
    const char* path = "tests/mlog_test.log";
    // 每次运行重新生成测试日志，Appender 本身仍使用追加模式。
    if (std::ofstream(path, std::ios::trunc).fail()) {
        LOG(ERROR) << "Cannot create " << path << '\n';
        return 1;
    }

    const int count = 100;
    auto write = [path, count](int worker) {
        auto appender = mlog::Logger::getInstance().getFileAppender(path);
        for (int i = 0; i < count; ++i) {
            auto message = "worker=" + std::to_string(worker) + " message=" + std::to_string(i);
            appender->append(message, mlog::INFO);
        }
    };

    std::thread first(write, 1);
    std::thread second(write, 2);
    first.join();
    second.join();

    // 不手动 flush，等待 Logger 内部的 EventBase 定时刷新。
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
        std::ifstream pending(path);
        int lines = 0;
        std::string line;
        while (std::getline(pending, line)) {
            ++lines;
        }
        if (lines == count * 2) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::ifstream input(path);
    std::set<std::string> expected;
    for (int worker = 1; worker <= 2; ++worker) {
        for (int i = 0; i < count; ++i) {
            expected.insert("worker=" + std::to_string(worker) + " message=" + std::to_string(i));
        }
    }
    int lines = 0;
    std::string line;
    while (std::getline(input, line)) {
        const auto start = line.find("] ");
        if (start == std::string::npos || expected.erase(line.substr(start + 2)) != 1) {
            LOG(ERROR) << "Invalid or duplicate log line: " << line << '\n';
            return 1;
        }
        ++lines;
    }
    if (input.bad() || lines != count * 2 || !expected.empty()) {
        LOG(ERROR) << "Incomplete log: " << lines << " lines\n";
        return 1;
    }
    LOG(INFO) << "PASS: two threads wrote " << lines << " complete log lines to " << path << '\n';
}
