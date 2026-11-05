#pragma once
#include <cstddef>
#include <map>
#include <string>

namespace mrpc {
class HttpRequest {
public:
    enum class ParseResult { Incomplete, Complete, Error };
    // On success consumes exactly one request. On incomplete/error consumes none.
    ParseResult parse(const std::string& input, std::size_t& consumed);
    std::string header(const std::string& name) const;
    std::string method, target, body;
    std::map<std::string, std::string> headers; // lowercase names
    bool keepAlive = true;
    int errorStatus = 400;
    static constexpr std::size_t maxHeaderBytes = 16 * 1024;
    static constexpr std::size_t maxBodyBytes = 1024 * 1024;
};
} // namespace mrpc
