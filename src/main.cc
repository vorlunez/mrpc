#include "TcpServer.h"
#include "HttpServer.h"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

namespace {
volatile std::sig_atomic_t stopping = 0;
void stop(int) { stopping = 1; }
} // namespace

int main(int argc, char** argv) {
    long port = 8080;
    if (argc > 3 || (argc == 3 && std::string(argv[2]) != "--http")) {
        std::cerr << "Usage: " << argv[0] << " [port] [--http]\n";
        return 1;
    }
    if (argc >= 2) {
        char* end = nullptr;
        errno = 0;
        port = std::strtol(argv[1], &end, 10);
        if (errno || end == argv[1] || *end || port < 1 || port > 65535) {
            std::cerr << "Port must be an integer from 1 to 65535\n";
            return 1;
        }
    }
    struct sigaction action{};
    action.sa_handler = stop;
    ::sigemptyset(&action.sa_mask);
    if (::sigaction(SIGINT, &action, nullptr) < 0 ||
        ::sigaction(SIGTERM, &action, nullptr) < 0) {
        std::perror("sigaction");
        return 1;
    }
    try {
        if (argc == 3) {
            mrpc::HttpServer server;
            server.run(static_cast<unsigned short>(port), stopping);
        } else {
            mrpc::TcpServer server;
            server.run(static_cast<unsigned short>(port), stopping);
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
