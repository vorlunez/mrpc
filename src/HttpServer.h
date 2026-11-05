#pragma once
#include "TcpServer.h"
namespace mrpc {
// Bridges HTTP codec and TCP transport. Business routing can be added here.
class HttpServer {
public:
    HttpServer();
    void run(unsigned short port, const volatile std::sig_atomic_t& stopping) {
        tcp_.run(port, stopping);
    }
private:
    static TcpServer::Reply handle(std::string& input, bool eof);
    TcpServer tcp_;
};
} // namespace mrpc
