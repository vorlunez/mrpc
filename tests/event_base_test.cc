#include "../src/mlog/EventBase.h"
#include "Appender.h"
#include "Logger.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
void require(bool ok, const char* message) {
    if (!ok) {
        throw std::runtime_error(message);
    }
}

void reset(const char* path) {
    require(!std::ofstream(path, std::ios::trunc).fail(), "cannot create test log");
}

std::string read(const char* path) {
    std::ifstream file(path);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

bool waitFor(const char* path, const std::string& message) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    do {
        if (read(path).find(message) != std::string::npos) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
}

void timerFlush() {
    const char* path = "tests/event_base_timer.log";
    const char* second_path = "tests/event_base_timer_second.log";
    reset(path);
    reset(second_path);
    mlog::EventBase events(std::chrono::milliseconds(20));
    auto appender = std::make_shared<mlog::FileAppender>(path);
    auto second = std::make_shared<mlog::FileAppender>(second_path);
    events.addAppender(appender);
    events.addAppender(second);
    events.addAppender(appender); // 重复注册不重复持有或刷新。
    appender->append("timer flush", mlog::INFO);
    second->append("second file", mlog::INFO);
    require(waitFor(path, "timer flush\n"), "periodic flush failed");
    require(waitFor(second_path, "second file\n"), "multiple appender flush failed");
}

void loggerShutdown() {
    const char* path = "tests/event_base_logger_lifetime.log";
    reset(path);
    std::shared_ptr<mlog::Appender> appender;
    {
        mlog::Logger logger(std::chrono::seconds(30));
        appender = logger.getFileAppender(path);
        appender->append("logger shutdown", mlog::INFO);
    }
    require(read(path).find("logger shutdown\n") != std::string::npos,
            "Logger destroyed appenders before draining");
    appender->append("logger gone", mlog::INFO);
    require(read(path).find("logger gone\n") != std::string::npos,
            "appender outliving Logger could not write");
}

void notificationFlush() {
    const char* path = "tests/event_base_notify.log";
    reset(path);
    // 观察窗口远短于周期，必须由达到缓冲阈值的通知唤醒。
    mlog::EventBase events(std::chrono::seconds(30));
    auto appender = std::make_shared<mlog::FileAppender>(path);
    events.addAppender(appender);
    std::string message(2 * 1024 * 1024, 'x');
    message += "notify flush";
    appender->append(message, mlog::INFO);
    require(waitFor(path, "notify flush\n"), "notification flush failed");
}

void appenderDiesFirst() {
    const char* path = "tests/event_base_appender_lifetime.log";
    reset(path);
    mlog::EventBase events(std::chrono::seconds(30));
    auto appender = std::make_shared<mlog::FileAppender>(path);
    std::weak_ptr<mlog::FileAppender> observer = appender;
    events.addAppender(appender);
    appender->append("last message", mlog::INFO);
    appender.reset();
    require(observer.expired(), "EventBase retained appender ownership");
    require(read(path).find("last message\n") != std::string::npos,
            "appender destructor did not drain");
}

void eventBaseDiesFirst() {
    const char* path = "tests/event_base_lifetime.log";
    reset(path);
    auto appender = std::make_shared<mlog::FileAppender>(path);
    {
        mlog::EventBase events(std::chrono::seconds(30));
        events.addAppender(appender);
        appender->append("before shutdown", mlog::INFO);
    }
    require(read(path).find("before shutdown\n") != std::string::npos,
            "EventBase destructor did not drain");
    appender->append("after shutdown", mlog::INFO);
    require(read(path).find("after shutdown\n") != std::string::npos,
            "surviving appender did not fall back to synchronous flush");
}

void concurrentStopAndAppend() {
    const char* path = "tests/event_base_stop.log";
    reset(path);
    mlog::EventBase events(std::chrono::seconds(30));
    auto appender = std::make_shared<mlog::FileAppender>(path);
    events.addAppender(appender);
    auto write = [appender](int worker) {
        for (int i = 0; i < 100; ++i) {
            auto message = std::to_string(worker) + ":" + std::to_string(i);
            appender->append(message, mlog::INFO);
        }
    };
    std::thread first(write, 1);
    std::thread second(write, 2);
    std::thread stop_first([&] { events.stop(); });
    std::thread stop_second([&] { events.stop(); });
    first.join();
    second.join();
    stop_first.join();
    stop_second.join();
    events.stop();

    std::set<std::string> expected;
    for (int worker = 1; worker <= 2; ++worker) {
        for (int i = 0; i < 100; ++i) {
            expected.insert(std::to_string(worker) + ":" + std::to_string(i));
        }
    }
    std::ifstream input(path);
    std::string line;
    while (std::getline(input, line)) {
        const auto start = line.find("] ");
        require(start != std::string::npos && expected.erase(line.substr(start + 2)) == 1,
                "duplicate or corrupt log during shutdown");
    }
    require(!input.bad() && expected.empty(), "shutdown lost queued messages");
}
} // namespace

int main() {
    try {
        timerFlush();
        notificationFlush();
        appenderDiesFirst();
        eventBaseDiesFirst();
        loggerShutdown();
        concurrentStopAndAppend();
        std::cout << "PASS: periodic/notification flush, lifetime and concurrent shutdown\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
