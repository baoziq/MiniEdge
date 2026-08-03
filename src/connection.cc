#include "connection.h"
#include "unique_fd.h"
#include <cerrno>
#include <cstddef>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

Connection::Connection(UniqueFd fd) : fd_(std::move(fd)), input_buffer_(), output_buffer_() {}

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
    if (length > input_buffer_.size()) {
        throw std::out_of_range("consume length exceeds input size");
    }
    input_buffer_.erase(0, length);
}

std::string_view Connection::input() const noexcept {
    return input_buffer_;
}

bool Connection::queue_output(std::string_view data) {
    if (data.size() > KMaxInputSize - output_buffer_.size()) {
        return false;
    }
    output_buffer_.append(data);
    return true;
}

WriteResult Connection::handle_write() {
    if (output_buffer_.empty()) {
        write_offset_ = 0;
        return WriteResult::KDrained;
    }

    while (write_offset_ < output_buffer_.size()) {
        const std::size_t remaining = output_buffer_.size() - write_offset_;
        const ssize_t n = send(
            fd_.get(),
            output_buffer_.data() + write_offset_,
            remaining,
            MSG_NOSIGNAL
        );

        if (n > 0) {
            write_offset_ += static_cast<std::size_t>(n);
            continue;
        }

        if (n < 0 && errno == EINTR) {
            continue;
        }

        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return WriteResult::KWouldBlock;
        }

        return WriteResult::KError;
    }

    output_buffer_.clear();
    write_offset_ = 0;
    return WriteResult::KDrained;
}

bool Connection::has_pending_output() const noexcept {
    return !output_buffer_.empty();
}

std::size_t Connection::pending_output_size() const noexcept {
    return output_buffer_.size() - write_offset_;
}
