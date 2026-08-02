#include "connection.h"
#include "unique_fd.h"
#include <cerrno>
#include <stdexcept>
#include <unistd.h>

Connection::Connection(UniqueFd fd) : fd_(std::move(fd)), input_buffer_() {}

int Connection::fd() const noexcept {
    return fd_.get();
}

ReadResult Connection::handle_read() {
    char buffer[1024];
    while (true) {
        ssize_t n = read(fd_.get(), &buffer, sizeof(buffer));
        if (n > 0) {
            input_buffer_.append(buffer, static_cast<std::size_t>(n));
            if (input_buffer_.size() >= KMaxInputSize) {
                return ReadResult::KError;
            }
        } else if (n == 0) {
            return ReadResult::KPeerClosed;
        } else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return ReadResult::KDataAvailable;
        } else if (n == -1 && errno == EINTR) {
            continue;
        } else {
            return ReadResult::KError;
        }
    }
}

void Connection::consume(std::size_t length) {
    if (length > input_buffer_.size() || input_buffer_.empty()) {
        throw std::out_of_range("consume length exceeds input size");
    }
    input_buffer_.erase(0, length);
}

std::string_view Connection::input() const noexcept {
    return input_buffer_;
}