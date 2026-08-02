#include "unique_fd.h"

#include <string_view>
#include <string>

enum class ReadResult {
    KDataAvailable,
    KPeerClosed,
    KError
};

class Connection {
public:
    explicit Connection(UniqueFd fd);
    int fd() const noexcept;
    ReadResult handle_read();
    std::string_view input() const noexcept;
    void consume(std::size_t length);

private:
    UniqueFd fd_;
    std::string input_buffer_;
};