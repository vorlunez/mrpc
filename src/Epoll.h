#pragma once

#include <sys/epoll.h>

namespace mrpc {

// Owns only the epoll descriptor; watched descriptors belong to the caller.
class Epoll {
public:
    Epoll();
    ~Epoll();
    Epoll(const Epoll&) = delete;
    Epoll& operator=(const Epoll&) = delete;

    void add(int fd, unsigned int events);
    void modify(int fd, unsigned int events);
    void remove(int fd);

    // Returns ready event count, or 0 on timeout/signal interruption.
    // Other syscall failures throw std::system_error.
    int wait(epoll_event* events, int capacity, int timeoutMs = -1);

private:
    void control(int operation, int fd, unsigned int events);
    int fd_;
};

} // namespace mrpc
