#include "http_parser.h"

HeaderParseResult parse_header(std::string_view input) {
    const auto index = input.find("\r\n\r\n");
    if (index == std::string::npos) {
        if (input.size() > kMaxHeaderSize) {
            return {HeaderParseStatus::KTooLarge, 0};
        }
        return {HeaderParseStatus::KIncomplete, 0};
    }
    const std::size_t length = index + 4;
    if (length > kMaxHeaderSize) {
        return {HeaderParseStatus::KTooLarge, 0};
    }
    return {HeaderParseStatus::KComplete, length};
}

// GET /index.html HTTP/1.1\r\n
// method target version
LineParseStatus parse_line(std::string_view input, HttpRequestLine& res) {
    const auto index = input.find("\r\n");
    if (index == std::string::npos) {
        return LineParseStatus::KBadRequest;
    }
    std::string_view line = input.substr(0, index);
    const auto method_end = line.find(" ");
    if (method_end == std::string::npos) {
        return LineParseStatus::KBadRequest;
    }
    const size_t target_start = method_end + 1; 
    const auto target_end = line.find(" ", target_start);
    if (target_end == std::string::npos || target_end == target_start) {
        return LineParseStatus::KBadRequest;
    }
    const size_t version_start = target_end + 1;
    if (version_start >= line.size()) {
        return LineParseStatus::KBadRequest;
    }
    res.method = line.substr(0, method_end);
    res.target = line.substr(target_start, target_end - target_start);
    res.version = line.substr(version_start, index - version_start);
    return LineParseStatus::KComplete;

}