#include "connection.h"
#include "unique_fd.h"
#include <cerrno>
#include <unistd.h>

Connection::Connection(UniqueFd fd) : fd_(std::move(fd)), input_buffer_() {}

int Connection::fd() const noexcept {
    return fd_.get();
}

ReadResult Connection::handle_read() {
    char buffer[1024];
    while (true) {
        int n = read(fd_.get(), &buffer, sizeof(buffer));
        if (n > 0) {
            input_buffer_.append(buffer);
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
        return;
    }
    input_buffer_.erase(input_buffer_.size() - length, length);
}

std::string_view Connection::input() const noexcept {
    return input_buffer_;
}