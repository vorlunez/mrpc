#include "Logger.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {
void fail(const char* operation) {
    throw std::system_error(errno, std::generic_category(), operation);
}

// Own the client socket so all exit paths release it.
class Socket {
public:
    Socket() : fd_(::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0)) {
        if (fd_ < 0) fail("socket");
    }
    ~Socket() { ::close(fd_); }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    int fd() const { return fd_; }

private:
    int fd_;
};

void sendAll(int fd, const char* data, std::size_t size) {
    std::size_t sent = 0;
    while (sent < size) {
        ssize_t count = ::send(fd, data + sent, size - sent, MSG_NOSIGNAL);
        if (count > 0) sent += static_cast<std::size_t>(count);
        else if (count < 0 && errno == EINTR) continue;
        else if (count < 0) fail("send");
        else throw std::runtime_error("send made no progress");
    }
}

void receiveEcho(int fd, std::size_t expected) {
    char buffer[8192];
    while (expected != 0) {
        ssize_t count = ::recv(fd, buffer, std::min(expected, sizeof(buffer)), 0);
        if (count > 0) {
            LOG(INFO) << "[recv] " << std::string(buffer, static_cast<std::size_t>(count));
            expected -= static_cast<std::size_t>(count);
        } else if (count == 0) {
            throw std::runtime_error("server closed before the complete echo arrived");
        } else if (errno != EINTR) {
            fail("recv");
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    if (argc > 3) {
        LOG(ERROR) << "Usage: " << argv[0] << " [IPv4 address] [port]";
        return 1;
    }
    long port = 8080;
    if (argc == 3) {
        char* end = nullptr;
        errno = 0;
        port = std::strtol(argv[2], &end, 10);
        if (errno || end == argv[2] || *end || port < 1 || port > 65535) {
            LOG(ERROR) << "Port must be an integer from 1 to 65535";
            return 1;
        }
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<unsigned short>(port));
    const char* host = argc >= 2 ? argv[1] : "127.0.0.1";
    if (::inet_pton(AF_INET, host, &address.sin_addr) != 1) {
        LOG(ERROR) << "Invalid IPv4 address: " << host;
        return 1;
    }

    try {
        Socket socket;
        if (::connect(socket.fd(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
            fail("connect");
        const bool interactive = ::isatty(STDIN_FILENO);
        if (interactive)
            LOG(INFO) << "Connected to " << host << ':' << port
                      << ". Enter a line to send; Ctrl+D to exit.";
        std::string line;
        while (true) {
            if (interactive) LOG(INFO) << "Enter a message:";
            if (!std::getline(std::cin, line)) break;
            // Preserve a final input line without a newline when used in a pipe.
            if (!std::cin.eof()) line.push_back('\n');
            // Alternate bounded sends/receives so a large input line cannot
            // deadlock against the server's bounded output buffer.
            for (std::size_t offset = 0; offset < line.size();) {
                std::size_t size = std::min(std::size_t(8192), line.size() - offset);
                sendAll(socket.fd(), line.data() + offset, size);
                // This count is known only because the server echoes every byte.
                receiveEcho(socket.fd(), size);
                offset += size;
            }
        }
        if (std::cin.bad()) throw std::runtime_error("failed to read standard input");
        if (::shutdown(socket.fd(), SHUT_WR) < 0) fail("shutdown");
        // All echoes have been read; wait for the server to acknowledge EOF.
        char byte;
        ssize_t count;
        do {
            count = ::recv(socket.fd(), &byte, 1, 0);
        } while (count < 0 && errno == EINTR);
        if (count < 0) fail("recv after shutdown");
        if (count > 0) throw std::runtime_error("server sent unexpected extra data");
    } catch (const std::exception& error) {
        LOG(ERROR) << error.what();
        return 1;
    }
}
