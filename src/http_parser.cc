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