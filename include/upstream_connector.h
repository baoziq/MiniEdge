#pragma once

#include "unique_fd.h"

#include <cstdint>
#include <string_view>

enum class ConnectStatus {
    KConnected,
    KInProgress,
    KError
};

struct ConnectResult {
    UniqueFd fd;
    ConnectStatus status{ConnectStatus::KError};
    int error_code{0};
};

struct ConnectCheckResult {
    ConnectStatus status{ConnectStatus::KError};
    int error_code{0};
};

ConnectResult connect_upstream(std::string_view ip, std::uint16_t port);
ConnectCheckResult check_connect_result(int fd) noexcept;
