#include "Router.h"
#include "HttpSyntax.h"

#include <stdexcept>
#include <utility>

namespace mrpc {
void Router::add(const std::string& method, const std::string& path, Callback callback) {
    if (!http::token(method) || path.empty() || path[0] != '/' ||
        path.find_first_of("?# \t\r\n") != std::string::npos || !callback)
        throw std::invalid_argument("invalid HTTP route");
    auto& methods = routes_[path];
    if (methods.count(method)) throw std::invalid_argument("duplicate HTTP route");
    methods.emplace(method, std::move(callback));
}

HttpResponse Router::dispatch(const HttpRequest& request) const {
    const auto path = request.target.substr(0, request.target.find('?'));
    const auto route = routes_.find(path);
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=utf-8";
    if (route == routes_.end()) {
        response.status = 404;
        response.body = "{\"error\":\"not found\"}";
        return response;
    }
    auto callback = route->second.find(request.method);
    // HTTP HEAD shares GET's headers and omits the body in HttpResponse.
    if (callback == route->second.end() && request.method == "HEAD")
        callback = route->second.find("GET");
    if (callback != route->second.end()) return callback->second(request);

    response.status = 405;
    response.body = "{\"error\":\"method not allowed\"}";
    std::string allowed;
    for (const auto& method : route->second) {
        if (!allowed.empty()) allowed += ", ";
        allowed += method.first;
        if (method.first == "GET" && !route->second.count("HEAD")) allowed += ", HEAD";
    }
    response.headers["Allow"] = allowed;
    return response;
}
} // namespace mrpc
