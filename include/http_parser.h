#pragma once
#include <unistd.h>
#include <cstddef>
#include <string_view>
#include <string>

constexpr std::size_t kMaxHeaderSize = 16 * 1024;
constexpr std::string_view response =
                                "HTTP/1.1 200 OK\r\n"
                                "Content-Length: 2\r\n"
                                "Connection: keep-alive\r\n"
                                "\r\n"
                                "OK";
                                
enum class HeaderParseStatus {
    KIncomplete,
    KComplete,
    KTooLarge
};

struct HeaderParseResult {
    HeaderParseStatus status;
    std::size_t length;
};

HeaderParseResult parse_header(std::string_view input);