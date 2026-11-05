#pragma once
#include "TcpServer.h"
#include "Router.h"
namespace mrpc {
// Bridges HTTP codec and TCP transport; routes hold application callbacks.
class HttpServer {
public:
    HttpServer();
    void addRoute(const std::string& method, const std::string& path, Router::Callback callback);
    void run(unsigned short port, const volatile std::sig_atomic_t& stopping) {
        tcp_.run(port, stopping);
    }
private:
    TcpServer::Reply handle(std::string& input, bool eof);
    Router router_;
    TcpServer tcp_;
};
} // namespace mrpc
