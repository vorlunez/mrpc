#include "HttpServer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Logger.h"
#include <exception>
#include <nlohmann/json.hpp>

namespace mrpc {
HttpServer::HttpServer() : tcp_([this](std::string& input, bool eof) {
    return handle(input, eof);
}) {}

void HttpServer::addRoute(const std::string& method, const std::string& path, Router::Callback callback) {
    router_.add(method, path, std::move(callback));
}

TcpServer::Reply HttpServer::handle(std::string& input, bool eof) {
    TcpServer::Reply reply;
    if (input.empty()) return reply;
    HttpRequest request;
    std::size_t consumed = 0;
    auto result = request.parse(input, consumed);
    if (result == HttpRequest::ParseResult::Incomplete && !eof) return reply;
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=utf-8";
    if (result != HttpRequest::ParseResult::Complete) {
        response.status = result == HttpRequest::ParseResult::Error ? request.errorStatus : 400;
        LOG(WARNING) << "HTTP request rejected: status=" << response.status;
        response.body = nlohmann::json({{"error", "invalid or unsupported HTTP request"},
                                        {"status", response.status}}).dump();
        response.close = true;
        input.clear();
    } else {
        input.erase(0, consumed);
        try {
            response = router_.dispatch(request);
        } catch (const std::exception& error) {
            LOG(ERROR) << "HTTP callback failed: " << error.what();
            response = HttpResponse{};
            response.status = 500;
            response.headers["Content-Type"] = "application/json; charset=utf-8";
            response.body = "{\"error\":\"internal server error\"}";
            response.close = true;
        } catch (...) {
            LOG(ERROR) << "HTTP callback failed: unknown exception";
            response = HttpResponse{};
            response.status = 500;
            response.headers["Content-Type"] = "application/json; charset=utf-8";
            response.body = "{\"error\":\"internal server error\"}";
            response.close = true;
        }
        response.close = response.close || !request.keepAlive || (eof && input.empty());
    }
    reply.data = response.serialize(request.method == "HEAD");
    reply.close = response.close;
    return reply;
}
} // namespace mrpc
