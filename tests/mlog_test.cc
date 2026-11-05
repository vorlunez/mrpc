#include "Logger.h"

#include <fstream>
#include <iostream>
#include <set>
#include <thread>

int main() {
    const char* path = "tests/mlog_test.log";
    // 每次运行重新生成测试日志，Appender 本身仍使用追加模式。
    if (std::ofstream(path, std::ios::trunc).fail()) {
        std::cerr << "Cannot create " << path << '\n';
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

    auto appender = std::dynamic_pointer_cast<mlog::FileAppender>(
        mlog::Logger::getInstance().getFileAppender(path));
    appender->flush(true);

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
            std::cerr << "Invalid or duplicate log line: " << line << '\n';
            return 1;
        }
        ++lines;
    }
    if (input.bad() || lines != count * 2 || !expected.empty()) {
        std::cerr << "Incomplete log: " << lines << " lines\n";
        return 1;
    }
    std::cout << "PASS: two threads wrote " << lines << " complete log lines to " << path << '\n';
}
