#include "HttpServer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include <nlohmann/json.hpp>

namespace mrpc {
HttpServer::HttpServer() : tcp_(handle) {}

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
        response.body = nlohmann::json({{"error", "invalid or unsupported HTTP request"},
                                        {"status", response.status}}).dump();
        response.close = true;
        input.clear();
    } else {
        input.erase(0, consumed);
        response.close = !request.keepAlive || (eof && input.empty());
        if (request.method != "GET" && request.method != "POST" && request.method != "HEAD" && request.method != "OPTIONS") {
            response.status = 405;
            response.headers["Allow"] = "GET, HEAD, POST, OPTIONS";
            response.body = "{\"error\":\"method not allowed\"}";
        } else {
            // Demo application: report the parsed request, without a router yet.
            // Replacement handles arbitrary request-body bytes safely in JSON.
            response.body = nlohmann::json({{"method", request.method},
                {"target", request.target}, {"body", request.body}})
                .dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        }
    }
    reply.data = response.serialize(request.method == "HEAD");
    reply.close = response.close;
    return reply;
}
} // namespace mrpc
