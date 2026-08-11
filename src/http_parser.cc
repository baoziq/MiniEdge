#include "http_parser.h"

#include <cstddef>

namespace {

char ascii_lower(char value) noexcept {
    if (value >= 'A' && value <= 'Z') {
        return static_cast<char>(value + ('a' - 'A'));
    }
    return value;
}

std::string_view trim_optional_whitespace(std::string_view value) noexcept {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }
    return value;
}

}  // namespace

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
    const auto line_end = input.find("\r\n");
    if (line_end == std::string_view::npos) {
        return LineParseStatus::KBadRequest;
    }

    const std::string_view line = input.substr(0, line_end);
    const auto method_end = line.find(' ');
    if (method_end == std::string_view::npos || method_end == 0) {
        return LineParseStatus::KBadRequest;
    }

    const std::size_t target_start = method_end + 1;
    const auto target_end = line.find(' ', target_start);
    if (target_end == std::string_view::npos || target_end == target_start) {
        return LineParseStatus::KBadRequest;
    }

    const std::size_t version_start = target_end + 1;
    if (version_start >= line.size()) {
        return LineParseStatus::KBadRequest;
    }

    const std::string_view version = line.substr(version_start);
    if (version != "HTTP/1.0" && version != "HTTP/1.1") {
        return LineParseStatus::KBadRequest;
    }

    res.method = line.substr(0, method_end);
    res.target = line.substr(target_start, target_end - target_start);
    res.version = version;
    return LineParseStatus::KComplete;
}

HeaderFieldsParseStatus parse_header_fields(std::string_view input, HttpRequest& request) {
    request = HttpRequest{};
    if (parse_line(input, request.request_line) == LineParseStatus::KBadRequest) {
        return HeaderFieldsParseStatus::KBadRequest;
    }

    const auto request_line_end = input.find("\r\n");
    const auto header_end = input.find("\r\n\r\n");
    if (request_line_end == std::string_view::npos ||
        header_end == std::string_view::npos ||
        request_line_end > header_end) {
        return HeaderFieldsParseStatus::KBadRequest;
    }

    request.keep_alive = request.request_line.version == "HTTP/1.1";

    std::size_t header_start = request_line_end + 2;
    while (header_start < header_end) {
        const auto line_end = input.find("\r\n", header_start);
        if (line_end == std::string_view::npos || line_end > header_end) {
            return HeaderFieldsParseStatus::KBadRequest;
        }

        const std::string_view current_line =
            input.substr(header_start, line_end - header_start);
        const auto colon_index = current_line.find(':');
        if (colon_index == std::string_view::npos || colon_index == 0) {
            return HeaderFieldsParseStatus::KBadRequest;
        }

        const std::string_view header_key = current_line.substr(0, colon_index);
        const std::string_view header_value =
            trim_optional_whitespace(current_line.substr(colon_index + 1));

        if (iequals(header_key, "host")) {
            request.host = header_value;
        } else if (iequals(header_key, "connection")) {
            request.connection = header_value;
            if (iequals(header_value, "close")) {
                request.keep_alive = false;
            }
        }

        header_start = line_end + 2;
    }

    if (request.request_line.version == "HTTP/1.1" && request.host.empty()) {
        return HeaderFieldsParseStatus::KBadRequest;
    }
    return HeaderFieldsParseStatus::KComplete;
}

bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (ascii_lower(a[i]) != ascii_lower(b[i])) {
            return false;
        }
    }
    return true;
}
