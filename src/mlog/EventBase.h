#pragma once
#include "include/Appender.h"

#include <iostream>
#include <memory>
#include <vector>

namespace mlog {
class EventBase {
public:
    EventBase();
    void run();
    void stop();
    void addAppender(std::shared_ptr<FileAppender> fa);

private:
    std::vector<std::weak_ptr<FileAppender>> vec;
};
} // namespace mlog