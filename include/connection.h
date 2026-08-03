#pragma once
#include "unique_fd.h"

#include <string_view>
#include <string>

static constexpr std::size_t KMaxInputSize = 1024 * 1024;

enum class ReadResult {
    KDataAvailable,
    KPeerClosed,
    KError
};

enum class WriteResult {
    KDrained,
    KWouldBlock,
    KError
};

class Connection {
public:
    explicit Connection(UniqueFd fd);
    int fd() const noexcept;
    ReadResult handle_read();
    std::string_view input() const noexcept;
    void consume(std::size_t length);

    bool queue_output(std::string_view data);
    WriteResult handle_write();

    bool has_pending_output() const noexcept;
    std::size_t pending_output_size() const noexcept;

private:
    UniqueFd fd_;
    std::string input_buffer_;
    std::string output_buffer_;
    std::size_t write_offset_{0};
};