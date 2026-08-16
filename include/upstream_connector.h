#pragma once

#include "unique_fd.h"

#include <cstdint>
#include <string_view>
#include <sys/socket.h>

enum class ConnectStatus {
    KConnected,
    KInProgress,
    KError
};

struct ConnectResult {
    UniqueFd fd;
    ConnectStatus status;
    int error_code;
};

struct ConnectCheckResult {
    ConnectStatus status;
    int error_code;
};

ConnectResult connect_upstream(std::string_view ip, std::uint64_t port);
ConnectCheckResult check_connect_result(UniqueFd fd);