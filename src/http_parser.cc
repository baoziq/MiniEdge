#include "http_parser.h"
#include <cctype>
#include <cstddef>

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
    HttpRequestLine request_line;
    auto line_flag = parse_line(input, request_line);
    if (line_flag != LineParseStatus::KComplete) {
        return HeaderFieldsParseStatus::KBadRequest;
    }
    const auto line_start = input.find("\r\n");
    const size_t header_start = line_start + 2;
    const auto header_end = input.find("\r\n\r\n");
    size_t length = header_end - header_start;
    std::string header{input.substr(header_start, length)};
    for (size_t i = 0; i < header.size(); i++) {
        header[i] = tolower(header[i]);
    }
    auto host_start = header.find("host");
    if (host_start == std::string::npos) {
        if (request_line.version == "HTTP/1.1") {
            return HeaderFieldsParseStatus::KBadRequest;
        }
        request.host = "";
    } else {
        host_start += 6;
        const auto host_end = header.find("\r\n");
        request.host = header.substr(host_start, host_end - host_start);
    }
    auto connection_start = header.find("connection");
    if (connection_start == std::string::npos) {
        request.connection = "";
    } else {
        connection_start += 12;
        const auto connection_end = header.find("\r\n", connection_start);
        request.connection = header.substr(connection_start, connection_end - connection_start);
    }
    if (request.connection == "close") {
        return HeaderFieldsParseStatus::KClose;
    }
    return HeaderFieldsParseStatus::KComplete;

}