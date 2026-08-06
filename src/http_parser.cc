#include "http_parser.h"

HeaderParseResult parse_header(std::string_view input) {
    HeaderParseResult res{HeaderParseStatus::KIncomplete, 0};
    auto it = input.find("\r\n\r\n");
    if (it == std::string::npos) {
        return res;
    }
    if (it > kMaxHeaderSize) {
        res.status = HeaderParseStatus::KTooLarge;
        return res;
    }
    res.status = HeaderParseStatus::KComplete;
    res.length = it;
    return res;
}