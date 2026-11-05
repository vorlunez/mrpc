#pragma once

#include "Epoll.h"

#include <csignal>
#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

namespace mrpc {

// Owns TCP sockets/buffers; the optional handler consumes application bytes.
class TcpServer {
public:
    struct Reply {
        std::string data;
        bool close = false;
    };
    // Consume complete input by erasing its prefix. Empty reply means wait for
    // more input. Called only when the previous reply has drained.
    using Handler = std::function<Reply(std::string& input, bool eof)>;
    TcpServer() = default;
    explicit TcpServer(Handler handler) : handler_(std::move(handler)) {}
    ~TcpServer();
    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    // Call once. The caller installs signal handlers and owns the stop flag.
    void run(unsigned short port, const volatile std::sig_atomic_t& stopping);

private:
    struct Connection {
        std::string input;
        std::string output;
        std::size_t sent = 0;
        bool eof = false;
        bool closing = false;
    };

    void acceptClients(const volatile std::sig_atomic_t& stopping);
    void closeClient(int fd);
    void handle(int fd, unsigned int flags);
    void processInput(Connection& client);

    static constexpr std::size_t bufferLimit = 64 * 1024;
    Epoll epoll_;
    int listener_ = -1;
    std::unordered_map<int, Connection> connections_;
    Handler handler_;
};

} // namespace mrpc
