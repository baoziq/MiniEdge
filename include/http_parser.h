#include <unistd.h>
#include <cstddef>
#include <string_view>
#include <string>

constexpr std::size_t kMaxHeaderSize = 16 * 1024;

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