#include "Logger.h"

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <iomanip>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>
#include <unistd.h>

namespace {
class RecordingAppender : public mlog::Appender {
public:
    void append(std::string& message, mlog::LogLevel level) override {
        std::lock_guard<std::mutex> lock(mutex);
        messages.emplace_back(message, level);
    }
    void append(const char* message, mlog::LogLevel level) override {
        std::string copy(message);
        append(copy, level);
    }
    std::mutex mutex;
    std::vector<std::pair<std::string, mlog::LogLevel>> messages;
};

class ThrowingAppender : public mlog::Appender {
public:
    void append(std::string&, mlog::LogLevel) override { throw std::runtime_error("sink failure"); }
    void append(const char*, mlog::LogLevel) override { throw std::runtime_error("sink failure"); }
};

// Capture the actual console descriptor, including the no-throw fallback path.
class ConsoleCapture {
public:
    ConsoleCapture() : file(std::tmpfile()), saved(::dup(STDERR_FILENO)) {
        assert(file && saved >= 0);
        assert(::dup2(::fileno(file), STDERR_FILENO) >= 0);
    }
    ~ConsoleCapture() {
        assert(::dup2(saved, STDERR_FILENO) >= 0);
        ::close(saved);
        std::fclose(file);
    }
    std::string read() {
        std::fflush(stderr);
        std::rewind(file);
        std::string output;
        char buffer[1024];
        std::size_t size;
        while ((size = std::fread(buffer, 1, sizeof(buffer), file)) != 0)
            output.append(buffer, size);
        return output;
    }
    FILE* file;
    int saved;
};
} // namespace

int main() {
    auto console = mlog::Logger::getGlobalAppender();
    assert(console == mlog::Logger::getConsoleAppender());
    assert(console.get() == &mlog::ConsoleAppender::getInstance());
    {
        ConsoleCapture capture;
        LOG(INFO) << "123";
        LOG(WARNING) << "warning";
        LOG(ERROR) << "error";
        auto output = capture.read();
        assert(output[0] == 'I');
        assert(output.find("\nW") != std::string::npos);
        assert(output.find("\nE") != std::string::npos);
        assert(output.find("log_stream_test.cc:") != std::string::npos);
        assert(output.find(" 123\n") != std::string::npos);
    }

    auto first = std::make_shared<RecordingAppender>();
    auto second = std::make_shared<RecordingAppender>();
    mlog::Logger::setGlobalAppender(first);
    errno = ENOENT;
    LOG(INFO) << "number=" << 123 << ' ' << std::hex << 255 << std::endl;
    assert(errno == ENOENT);
    assert(first->messages.size() == 1);
    assert(first->messages[0].first.find("number=123 ff\n") != std::string::npos);
    {
        mlog::LogMessage pending(mlog::WARNING, __FILE__, __LINE__);
        pending.stream() << "old sink";
        mlog::Logger::setGlobalAppender(second);
        assert(first->messages.size() == 1); // Not committed until destruction.
    }
    assert(first->messages.size() == 2);
    assert(first->messages.back().second == mlog::WARNING);
    LOG(ERROR) << "new sink";
    assert(second->messages.size() == 1 && second->messages[0].second == mlog::ERROR);
    try {
        mlog::LogMessage unwinding(mlog::ERROR, __FILE__, __LINE__);
        unwinding.stream() << "during stack unwinding";
        throw std::runtime_error("business failure");
    } catch (const std::runtime_error&) {}
    assert(second->messages.size() == 2);

    // Race global sink switches against writers; every record must arrive once.
    const std::size_t baseline = first->messages.size() + second->messages.size();
    auto writer = [](int id) {
        for (int i = 0; i < 200; ++i) LOG(INFO) << "record=" << id << ':' << i;
    };
    std::thread a(writer, 1), b(writer, 2);
    std::thread switcher([&] {
        for (int i = 0; i < 200; ++i)
            mlog::Logger::setGlobalAppender(i % 2 ? first : second);
    });
    a.join();
    b.join();
    switcher.join();
    assert(first->messages.size() + second->messages.size() == baseline + 400);
    std::set<std::string> records;
    for (auto sink : {first, second}) {
        for (const auto& message : sink->messages) {
            auto start = message.first.find("record=");
            if (start != std::string::npos) assert(records.insert(message.first.substr(start)).second);
        }
    }
    assert(records.size() == 400);
    {
        ConsoleCapture capture;
        mlog::Logger::setGlobalAppender(std::make_shared<ThrowingAppender>());
        errno = EACCES;
        LOG(ERROR) << "must not throw";
        assert(errno == EACCES);
        assert(capture.read().find("mlog: failed to write log message") != std::string::npos);
    }
    mlog::Logger::setGlobalAppender(nullptr);
    assert(mlog::Logger::getGlobalAppender() == console);
    LOG(INFO) << "PASS: LOG syntax, console, levels, errno, lifetime, concurrency and no-throw fallback";
}
