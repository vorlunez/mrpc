#include "TcpServer.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace mrpc {
namespace {
void fail(const char* operation) {
    throw std::runtime_error(std::string(operation) + ": " + std::strerror(errno));
}
} // namespace

TcpServer::~TcpServer() {
    for (const auto& entry : connections_) ::close(entry.first);
    if (listener_ >= 0) ::close(listener_);
}

void TcpServer::run(unsigned short port, const volatile std::sig_atomic_t& stopping) {
    listener_ = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listener_ < 0) fail("socket");
    int reuse = 1;
    if (::setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
        fail("setsockopt");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (::bind(listener_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
        fail("bind");
    if (::listen(listener_, SOMAXCONN) < 0) fail("listen");
    epoll_.add(listener_, EPOLLIN);
    std::cout << "TCP server listening on 127.0.0.1:" << port << std::endl;

    epoll_event events[64];
    while (!stopping) {
        // A bounded timeout lets SIGINT/SIGTERM end the loop even if the
        // signal arrives immediately before epoll_wait.
        int count = epoll_.wait(events, 64, 500);
        for (int i = 0; i < count; ++i) {
            int fd = events[i].data.fd;
            if (fd == listener_) {
                if (events[i].events & (EPOLLERR | EPOLLHUP))
                    throw std::runtime_error("listener failed");
                acceptClients(stopping);
            } else {
                handle(fd, events[i].events);
            }
        }
    }
}


void TcpServer::closeClient(int fd) {
    epoll_.remove(fd);
    ::close(fd);
    connections_.erase(fd);
}

void TcpServer::acceptClients(const volatile std::sig_atomic_t& stopping) {
    // Bound work per event so a busy listener cannot starve existing clients.
    for (int i = 0; i < 64 && !stopping; ++i) {
        int fd = ::accept4(listener_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd < 0) {
            if (errno == EINTR || errno == ECONNABORTED) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            fail("accept4");
        }
        try {
            connections_.emplace(fd, Connection{});
        } catch (...) {
            ::close(fd);
            throw;
        }
        epoll_.add(fd, EPOLLIN);
    }
}

void TcpServer::processInput(Connection& client) {
    if (client.sent != client.output.size() || client.closing) return;
    client.output.clear();
    client.sent = 0;
    if (client.input.empty() && !client.eof) return;
    if (handler_) {
        Reply reply = handler_(client.input, client.eof);
        client.output = std::move(reply.data);
        client.closing = reply.close;
    } else {
        client.output.swap(client.input); // Default TCP echo behavior.
    }
}

void TcpServer::handle(int fd, unsigned int flags) {
    auto it = connections_.find(fd);
    if (it == connections_.end()) return;
    if (flags & EPOLLERR) {
        closeClient(fd);
        return;
    }
    Connection& client = it->second;
    if (!client.eof && !client.closing && (flags & (EPOLLIN | EPOLLHUP))) {
        std::size_t pending = client.output.size() - client.sent;
        // Drain the current reply before reading more requests. This bounds
        // buffered pipelined input even when a client stops reading replies.
        std::size_t room = pending == 0 ? bufferLimit : 0;
        if (room != 0) {
            char buffer[8192];
            ssize_t size = ::recv(fd, buffer, std::min(room, sizeof(buffer)), 0);
            if (size > 0) {
                client.input.append(buffer, static_cast<std::size_t>(size));
                std::cout << "[recv fd=" << fd << ", bytes=" << size << "] ";
                // recv returns a byte count, not a null-terminated C string.
                std::cout.write(buffer, size);
                if (buffer[size - 1] != '\n') std::cout << '\n';
                std::cout.flush();
            }
            else if (size == 0) client.eof = true;
            else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                closeClient(fd);
                return;
            }
        }
    }
    processInput(client);
    if (client.sent < client.output.size()) {
        // MSG_NOSIGNAL prevents a disconnected peer from killing the server.
        ssize_t size = ::send(fd, client.output.data() + client.sent,
                              client.output.size() - client.sent, MSG_NOSIGNAL);
        if (size > 0) client.sent += static_cast<std::size_t>(size);
        else if (size < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            closeClient(fd);
            return;
        }
    }
    // A pipelined request may already be buffered even if the socket has no
    // unread bytes. Queue its reply now so EPOLLOUT drives further progress.
    processInput(client);
    std::size_t pending = client.output.size() - client.sent;
    if ((client.closing || (client.eof && client.input.empty())) && pending == 0) {
        closeClient(fd);
        return;
    }
    // Resume reads after the queued reply has drained.
    unsigned int interests = 0;
    if (!client.eof && !client.closing && pending == 0) interests |= EPOLLIN;
    if (pending != 0) interests |= EPOLLOUT;
    epoll_.modify(fd, interests);
}

} // namespace mrpc
