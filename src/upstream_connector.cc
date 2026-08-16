#include "upstream_connector.h"
#include "common.h"

#include <arpa/inet.h>
#include <cerrno>
#include <string>
#include <netinet/in.h>
#include <sys/socket.h>
#include <utility>

ConnectResult connect_upstream(std::string_view ip, std::uint16_t port) {
    ConnectResult connect_result{};
    const int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        connect_result.error_code = errno;
        return connect_result;
    }

    UniqueFd fd(socket_fd);
    if (set_nonblocking(fd.get()) < 0) {
        connect_result.fd = std::move(fd);
        connect_result.error_code = errno;
        return connect_result;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    const std::string ip_string(ip);
    if (inet_pton(AF_INET, ip_string.c_str(), &address.sin_addr) != 1) {
        connect_result.fd = std::move(fd);
        connect_result.error_code = EINVAL;
        return connect_result;
    }

    const int ret = connect(
        fd.get(),
        reinterpret_cast<sockaddr*>(&address),
        sizeof(address)
    );
    const int connect_error = errno;
    connect_result.fd = std::move(fd);

    if (ret == 0) {
        connect_result.status = ConnectStatus::KConnected;
    } else if (ret == -1 && connect_error == EINPROGRESS) {
        connect_result.status = ConnectStatus::KInProgress;
    } else {
        connect_result.status = ConnectStatus::KError;
        connect_result.error_code = connect_error;
    }
    return connect_result;
}

ConnectCheckResult check_connect_result(int fd) noexcept {
    int socket_error = 0;
    ConnectCheckResult check_result{};
    socklen_t length = sizeof(socket_error);
    const int ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &length);
    if (ret < 0) {
        check_result.error_code = errno;
        return check_result;
    }

    if (socket_error != 0) {
        check_result.status = ConnectStatus::KError;
        check_result.error_code = socket_error;
    } else {
        check_result.status = ConnectStatus::KConnected;
    }
    return check_result;
}
