#include "HttpResponse.h"
#include "HttpSyntax.h"
#include <stdexcept>
#include <ctime>
#include <cstdio>

namespace mrpc {
namespace {
const char* reason(int status) {
    switch (status) {
    case 200: return "OK";
    case 201: return "Created";
    case 204: return "No Content";
    case 205: return "Reset Content";
    case 304: return "Not Modified";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 413: return "Content Too Large";
    case 417: return "Expectation Failed";
    case 431: return "Request Header Fields Too Large";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 503: return "Service Unavailable";
    case 505: return "HTTP Version Not Supported";
    default: return "Unknown";
    }
}
}
std::string HttpResponse::serialize(bool head) const {
    // This minimal serializer emits final responses only, without upgrades/tunnels.
    if (status < 200 || status > 599) throw std::invalid_argument("invalid final HTTP status");
    bool noBody = status == 204 || status == 205 || status == 304;
    std::string result = "HTTP/1.1 " + std::to_string(status) + " " + reason(status) + "\r\n";
    bool hasDate = false;
    for (const auto& field : headers) {
        if (!http::token(field.first) || !http::fieldValue(field.second))
            throw std::invalid_argument("invalid HTTP response header");
        auto name = http::lower(field.first);
        if (name == "date") hasDate = true;
        // Framing is owned exclusively by this serializer.
        if (name == "content-length" || name == "connection" || name == "transfer-encoding") continue;
        result += field.first + ": " + field.second + "\r\n";
    }
    if (!hasDate) {
        auto now = std::time(nullptr);
        std::tm utc{};
        if (::gmtime_r(&now, &utc)) {
            const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
            const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
            char date[64];
            std::snprintf(date, sizeof(date), "%s, %02d %s %04d %02d:%02d:%02d GMT",
                days[utc.tm_wday], utc.tm_mday, months[utc.tm_mon], utc.tm_year + 1900,
                utc.tm_hour, utc.tm_min, utc.tm_sec);
            result += std::string("Date: ") + date + "\r\n";
        }
    }
    if (!noBody || status == 205)
        result += "Content-Length: " + std::to_string(noBody ? 0 : body.size()) + "\r\n";
    result += std::string("Connection: ") + (close ? "close" : "keep-alive") + "\r\n\r\n";
    if (!head && !noBody) result += body;
    return result;
}
} // namespace mrpc
