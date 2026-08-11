#include "http_parser.h"
#include <cctype>
#include <cstddef>
#include <unistd.h>

HeaderParseResult parse_header_boundary(std::string_view input) {
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

HeaderFieldsParseStatus parse_header_fields(std::string_view input, HttpRequest& request) {
    auto line_flag = parse_line(input, request.request_line);
    if (line_flag == LineParseStatus::KBadRequest) {
        return HeaderFieldsParseStatus::KBadRequest;
    }
    auto header_start = input.find("\r\n");
    header_start += 2;
    auto header_end = input.find("\r\n\r\n");
    while (header_start < header_end) {
        auto line_end = input.find("\r\n", header_start);
        auto colon_index = input.find(":", header_start);
        auto line = input.substr(colon_index + 2, line_end - colon_index);
        auto header_key = input.substr(header_start, colon_index - header_start);
        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front()))) {
            line.remove_prefix(1);
        }
        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) {
            line.remove_suffix(1);
        }
        auto header_value = line;
        if (iequals(header_key, "host")) {
            request.host = header_value;
        } else if (iequals(header_key, "connection")) {
            request.connection = header_value;
        }
        header_start = line_end + 2;
    }
    if (request.request_line.version == "HTTP/1.1" && request.host.empty()) {
        return HeaderFieldsParseStatus::KBadRequest;
    }
    if (request.connection == "close") {
        return HeaderFieldsParseStatus::KClose;
    }
    return HeaderFieldsParseStatus::KComplete;

}

bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); i++) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
                return false;
            }
    }
    return true;
}