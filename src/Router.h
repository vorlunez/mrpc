#pragma once

#include "HttpRequest.h"
#include "HttpResponse.h"

#include <functional>
#include <map>
#include <string>

namespace mrpc {
class Router {
public:
    using Callback = std::function<HttpResponse(const HttpRequest&)>;

    // Register before the server starts; method names are case sensitive.
    // Paths match exactly, excluding the request's query string.
    void add(const std::string& method, const std::string& path, Callback callback);
    HttpResponse dispatch(const HttpRequest& request) const;

private:
    std::map<std::string, std::map<std::string, Callback>> routes_;
};
} // namespace mrpc
