#pragma once
#include <string>
namespace mrpc {
namespace http {
inline std::string lower(std::string value) {
    for (char& c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return value;
}
inline std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string::npos) return "";
    return value.substr(first, value.find_last_not_of(" \t") - first + 1);
}
inline bool token(const std::string& value) {
    if (value.empty()) return false;
    for (unsigned char c : value)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || std::string("!#$%&'*+-.^_`|~").find(c) != std::string::npos))
            return false;
    return true;
}
inline bool fieldValue(const std::string& value) {
    for (unsigned char c : value) if ((c < 32 && c != '\t') || c == 127) return false;
    return true;
}
inline bool hasToken(const std::string& value, const std::string& wanted) {
    std::size_t pos = 0;
    while (pos <= value.size()) {
        auto end = value.find(',', pos);
        if (lower(trim(value.substr(pos, end - pos))) == wanted) return true;
        if (end == std::string::npos) break;
        pos = end + 1;
    }
    return false;
}
} // namespace http
} // namespace mrpc
