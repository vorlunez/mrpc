#include "HttpRequest.h"
#include "HttpSyntax.h"

namespace mrpc {
namespace {
bool validExtensions(const std::string& value) {
    std::size_t pos = 0;
    auto whitespace = [&]() {
        while (pos < value.size() && (value[pos] == ' ' || value[pos] == '\t')) ++pos;
    };
    while (pos < value.size()) {
        whitespace();
        if (pos == value.size() || value[pos++] != ';') return false;
        whitespace();
        std::size_t begin = pos;
        while (pos < value.size() && http::token(value.substr(pos, 1))) ++pos;
        if (pos == begin) return false;
        whitespace();
        if (pos < value.size() && value[pos] == '=') {
            ++pos;
            whitespace();
            if (pos < value.size() && value[pos] == '"') {
                ++pos;
                bool closed = false;
                while (pos < value.size()) {
                    unsigned char c = value[pos++];
                    if (c == '"') { closed = true; break; }
                    if (c == '\\') {
                        if (pos == value.size()) return false;
                        c = value[pos++];
                    }
                    if ((c < 32 && c != '\t') || c == 127) return false;
                }
                if (!closed) return false;
            } else {
                begin = pos;
                while (pos < value.size() && http::token(value.substr(pos, 1))) ++pos;
                if (pos == begin) return false;
            }
            whitespace();
        }
        if (pos < value.size() && value[pos] != ';') return false;
    }
    return true;
}
} // namespace

std::string HttpRequest::header(const std::string& name) const {
    auto it = headers.find(http::lower(name));
    return it == headers.end() ? "" : it->second;
}

HttpRequest::ParseResult HttpRequest::parse(const std::string& input, std::size_t& consumed) {
    *this = HttpRequest{};
    consumed = 0;
    const auto incomplete = ParseResult::Incomplete;
    const auto error = ParseResult::Error;
    auto headerEnd = input.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        if (input.size() > maxHeaderBytes) { errorStatus = 431; return error; }
        return incomplete;
    }
    if (headerEnd + 4 > maxHeaderBytes) { errorStatus = 431; return error; }
    auto lineEnd = input.find("\r\n");
    std::string line = input.substr(0, lineEnd);
    auto first = line.find(' '), second = line.find(' ', first == std::string::npos ? 0 : first + 1);
    if (first == std::string::npos || second == std::string::npos || line.find(' ', second + 1) != std::string::npos)
        return error;
    method = line.substr(0, first);
    target = line.substr(first + 1, second - first - 1);
    if (!http::token(method) || target.empty()) return error;
    for (unsigned char c : target) if (c <= 32 || c == 127 || c == '#') return error;
    // This origin server accepts origin-form and OPTIONS *; no proxy/CONNECT mode.
    if (target[0] != '/' && !(method == "OPTIONS" && target == "*")) return error;
    if (line.substr(second + 1) != "HTTP/1.1") { errorStatus = 505; return error; }
    std::size_t pos = lineEnd + 2;
    while (pos < headerEnd) {
        auto end = input.find("\r\n", pos);
        std::string field = input.substr(pos, end - pos);
        auto colon = field.find(':');
        if (colon == std::string::npos || !http::token(field.substr(0, colon))) return error;
        std::string name = http::lower(field.substr(0, colon));
        std::string value = http::trim(field.substr(colon + 1));
        if (!http::fieldValue(value)) return error;
        auto existing = headers.find(name);
        if (existing != headers.end()) {
            // Reject ambiguous framing instead of guessing between duplicate values.
            if (name == "host" || name == "content-length" || name == "transfer-encoding") return error;
            existing->second += "," + value;
        } else headers.emplace(name, value);
        pos = end + 2;
    }
    if (header("host").empty()) return error;
    for (unsigned char c : header("host")) if (c <= 32 || c == ',' || c == '/' || c == '@') return error;
    keepAlive = !http::hasToken(header("connection"), "close");
    const bool transfer = headers.count("transfer-encoding") != 0;
    const bool length = headers.count("content-length") != 0;
    if (transfer && length) return error;
    // Explicitly reject expectations so clients do not wait forever for 100 Continue.
    if (headers.count("expect")) { errorStatus = 417; return error; }
    pos = headerEnd + 4;
    if (transfer) {
        if (http::lower(header("transfer-encoding")) != "chunked") { errorStatus = 501; return error; }
        const std::size_t bodyStart = pos;
        while (true) {
            if (pos - bodyStart > maxBodyBytes + maxHeaderBytes) { errorStatus = 413; return error; }
            auto end = input.find("\r\n", pos);
            if (end == std::string::npos) {
                if (input.size() - pos > maxHeaderBytes) { errorStatus = 431; return error; }
                return incomplete;
            }
            if (end - pos > maxHeaderBytes) { errorStatus = 431; return error; }
            auto semicolon = input.find(';', pos);
            std::size_t numberEnd = semicolon < end ? semicolon : end;
            auto digits = input.substr(pos, numberEnd - pos);
            if (numberEnd < end) {
                auto last = digits.find_last_not_of(" \t");
                if (last == std::string::npos) return error;
                digits.resize(last + 1);
            }
            if (digits.empty()) return error;
            std::size_t size = 0;
            for (unsigned char c : digits) {
                int digit = c >= '0' && c <= '9' ? c - '0' :
                            c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                            c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
                if (digit < 0) return error;
                if (size > (maxBodyBytes - static_cast<std::size_t>(digit)) / 16) { errorStatus = 413; return error; }
                size = size * 16 + digit;
            }
            if (!validExtensions(input.substr(numberEnd, end - numberEnd))) return error;
            pos = end + 2;
            if (size == 0) {
                const auto trailerStart = pos;
                while (true) {
                    end = input.find("\r\n", pos);
                    if (end == std::string::npos) {
                        if (input.size() - trailerStart > maxHeaderBytes) { errorStatus = 431; return error; }
                        return incomplete;
                    }
                    if (end + 2 - trailerStart > maxHeaderBytes) { errorStatus = 431; return error; }
                    if (end == pos) { consumed = end + 2; return ParseResult::Complete; }
                    std::string field = input.substr(pos, end - pos);
                    auto colon = field.find(':');
                    if (colon == std::string::npos || !http::token(field.substr(0, colon)) ||
                        !http::fieldValue(field.substr(colon + 1))) return error;
                    auto name = http::lower(field.substr(0, colon));
                    if (name == "content-length" || name == "transfer-encoding" || name == "host" || name == "connection") return error;
                    // Trailers are validated and discarded, never merged into headers.
                    pos = end + 2;
                }
            }
            if (size > maxBodyBytes - body.size()) { errorStatus = 413; return error; }
            if (input.size() - pos < size + 2) return incomplete;
            if (input.compare(pos + size, 2, "\r\n") != 0) return error;
            body.append(input, pos, size);
            pos += size + 2;
        }
    }
    std::size_t size = 0;
    if (length) {
        auto value = header("content-length");
        if (value.empty()) return error;
        for (unsigned char c : value) {
            if (c < '0' || c > '9') return error;
            if (size > (maxBodyBytes - (c - '0')) / 10) { errorStatus = 413; return error; }
            size = size * 10 + c - '0';
        }
    }
    if (input.size() - pos < size) return incomplete;
    body = input.substr(pos, size);
    consumed = pos + size;
    return ParseResult::Complete;
}
} // namespace mrpc
