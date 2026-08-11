#pragma once
#include <cstddef>
#include <string>
#include <string_view>

constexpr std::size_t kMaxHeaderSize = 16 * 1024;
constexpr std::string_view OK_KEEP_ALIVE_RESPONSE =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 2\r\n"
    "Connection: keep-alive\r\n"
    "\r\n"
    "OK";
constexpr std::string_view OK_CLOSE_RESPONSE =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 2\r\n"
    "Connection: close\r\n"
    "\r\n"
    "OK";
constexpr std::string_view BAD_RESPONSE =
    "HTTP/1.1 400 Bad Request\r\n"
    "Content-Length: 0\r\n"
    "Connection: close\r\n"
    "\r\n";
enum class HeaderParseStatus {
    KIncomplete,
    KComplete,
    KTooLarge
};

enum class LineParseStatus {
    KComplete,
    KBadRequest
};

enum class HeaderFieldsParseStatus {
    KComplete,
    KBadRequest
};
struct HeaderParseResult {
    HeaderParseStatus status;
    std::size_t length;
};

HeaderParseResult parse_header_boundary(std::string_view input);

struct HttpRequestLine {
    std::string method;
    std::string target;
    std::string version;
};

LineParseStatus parse_line(std::string_view input, HttpRequestLine& res);

struct HttpRequest {
    HttpRequestLine request_line;
    std::string_view host;
    std::string_view connection;
    bool keep_alive{false};
};

HeaderFieldsParseStatus parse_header_fields(std::string_view input, HttpRequest& request);
bool iequals(std::string_view a, std::string_view b);
