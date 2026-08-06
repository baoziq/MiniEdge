#pragma once
#include "unique_fd.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>

inline constexpr std::size_t KMaxInputSize = 1024 * 1024;
inline constexpr std::size_t KMaxOutputSize = 1024 * 1024;

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
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    Connection(Connection&&) = default;
    Connection& operator=(Connection&&) = default;

    int fd() const noexcept;
    ReadResult handle_read();
    std::string_view input() const noexcept;
    void consume(std::size_t length);

    bool queue_output(std::string_view data);
    WriteResult handle_write();

    bool has_pending_output() const noexcept;
    std::size_t pending_output_size() const noexcept;
    bool peer_closed() const noexcept;
private:
    UniqueFd fd_;
    std::string input_buffer_;
    std::string output_buffer_;
    std::size_t write_offset_{0};
    bool peer_closed_{false};
};

static_assert(!std::is_copy_constructible_v<Connection>);
static_assert(!std::is_copy_assignable_v<Connection>);
static_assert(std::is_move_constructible_v<Connection>);
static_assert(std::is_move_assignable_v<Connection>);
