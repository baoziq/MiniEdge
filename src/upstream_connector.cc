#include "upstream_connector.h"
#include "common.h"

#include "unique_fd.h"
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

ConnectResult connect_upstream(std::string_view ip, std::uint64_t port) {
    ConnectResult connect_result{};
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        throw std::system_error {
            errno,
            std::generic_category(),
            "upstream socket"
        };
    }
    UniqueFd fd(listen_fd);
    set_nonblocking(fd.get());
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = port;
    std::string ip_str(ip);
    inet_pton(AF_INET, ip_str.c_str(), &address.sin_addr);
    connect_result.fd = std::move(fd);
    int ret = connect(fd.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address));
    if (ret == 0) {
        connect_result.status = ConnectStatus::KConnected;
    } else if (ret == -1 && errno == EINPROGRESS) {
        connect_result.status = ConnectStatus::KInProgress;
    } else {
        connect_result.status = ConnectStatus::KError;
    }
    return connect_result;

}

ConnectCheckResult check_connect_result(int fd) {
    int socket_error = 0;
    ConnectCheckResult check_result{};
    socklen_t length = sizeof(socket_error);
    int ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &length);
    if (ret < 0) {
        throw std::system_error {
            errno,
            std::generic_category(),
            "getsockopt"
        };
    }

    if (socket_error != 0) {
        check_result.status = ConnectStatus::KError;
    } else {
        check_result.status = ConnectStatus::KConnected;
    }
    return check_result;

}