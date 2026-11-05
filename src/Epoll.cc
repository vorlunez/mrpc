#include "Epoll.h"

#include <unistd.h>

#include <cerrno>
#include <system_error>

namespace mrpc {

Epoll::Epoll() : fd_(::epoll_create1(EPOLL_CLOEXEC)) {
    if (fd_ < 0)
        throw std::system_error(errno, std::generic_category(), "epoll_create1");
}

Epoll::~Epoll() { ::close(fd_); }

void Epoll::control(int operation, int fd, unsigned int events) {
    epoll_event event{};
    event.events = events;
    event.data.fd = fd;
    if (::epoll_ctl(fd_, operation, fd, operation == EPOLL_CTL_DEL ? nullptr : &event) < 0)
        throw std::system_error(errno, std::generic_category(), "epoll_ctl");
}

void Epoll::add(int fd, unsigned int events) { control(EPOLL_CTL_ADD, fd, events); }
void Epoll::modify(int fd, unsigned int events) { control(EPOLL_CTL_MOD, fd, events); }
void Epoll::remove(int fd) { control(EPOLL_CTL_DEL, fd, 0); }

int Epoll::wait(epoll_event* events, int capacity, int timeoutMs) {
    int count = ::epoll_wait(fd_, events, capacity, timeoutMs);
    if (count < 0) {
        if (errno == EINTR) return 0;
        throw std::system_error(errno, std::generic_category(), "epoll_wait");
    }
    return count;
}

} // namespace mrpc
