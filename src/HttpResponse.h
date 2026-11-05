#pragma once
#include <map>
#include <string>
namespace mrpc {
class HttpResponse {
public:
    int status = 200;
    std::string body;
    std::map<std::string, std::string> headers;
    bool close = false;
    // HEAD emits the same Content-Length as GET, but no body.
    std::string serialize(bool head = false) const;
};
} // namespace mrpc
