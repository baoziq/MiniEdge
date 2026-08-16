#include "upstream_connector.h"

#include "unique_fd.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <string_view>
#include <sys/socket.h>

namespace {

void expect(bool condition, std::string_view test_name) {
    if (!condition) {
        std::cerr << "[FAILED] " << test_name << '\n';
        std::exit(EXIT_FAILURE);
    }
    std::cout << "[PASSED] " << test_name << '\n';
}

UniqueFd create_loopback_listener(std::uint16_t& port) {
    UniqueFd listener(socket(AF_INET, SOCK_STREAM, 0));
    expect(listener.valid(), "create loopback listener");

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    expect(bind(
               listener.get(),
               reinterpret_cast<sockaddr*>(&address),
               sizeof(address)) == 0,
           "bind loopback listener");
    expect(listen(listener.get(), 1) == 0, "listen on loopback socket");

    socklen_t address_length = sizeof(address);
    expect(getsockname(
               listener.get(),
               reinterpret_cast<sockaddr*>(&address),
               &address_length) == 0,
           "read loopback listener port");
    port = ntohs(address.sin_port);
    return listener;
}

void test_nonblocking_connect_completes() {
    std::uint16_t port = 0;
    auto listener = create_loopback_listener(port);
    (void)listener;
    auto result = connect_upstream("127.0.0.1", port);

    expect(result.fd.valid(), "upstream fd is retained");
    expect(result.status != ConnectStatus::KError,
           "loopback connection starts successfully");

    const int flags = fcntl(result.fd.get(), F_GETFL, 0);
    expect(flags >= 0 && (flags & O_NONBLOCK) != 0,
           "upstream fd is nonblocking");

    if (result.status == ConnectStatus::KInProgress) {
        pollfd event{};
        event.fd = result.fd.get();
        event.events = POLLOUT;
        expect(poll(&event, 1, 1000) == 1,
               "nonblocking connection becomes writable");

        const auto check = check_connect_result(result.fd.get());
        expect(check.status == ConnectStatus::KConnected,
               "SO_ERROR confirms connection success");
        expect(check.error_code == 0,
               "successful completion has no error code");
    }
}

void test_invalid_ip_is_reported() {
    auto result = connect_upstream("not-an-ip", 9000);
    expect(result.status == ConnectStatus::KError,
           "invalid IPv4 address is rejected");
    expect(result.error_code == EINVAL,
           "invalid IPv4 address reports EINVAL");
}

void test_refused_connection_is_reported() {
    std::uint16_t port = 0;
    {
        auto listener = create_loopback_listener(port);
        (void)listener;
    }

    auto result = connect_upstream("127.0.0.1", port);
    if (result.status == ConnectStatus::KInProgress) {
        pollfd event{};
        event.fd = result.fd.get();
        event.events = POLLOUT;
        expect(poll(&event, 1, 1000) == 1,
               "refused connection finishes asynchronously");

        const auto check = check_connect_result(result.fd.get());
        expect(check.status == ConnectStatus::KError,
               "SO_ERROR reports refused connection");
        expect(check.error_code != 0,
               "refused completion preserves socket error");
        return;
    }

    expect(result.status == ConnectStatus::KError,
           "refused connection is not reported as connected");
    expect(result.error_code != 0,
           "immediate connection failure preserves errno");
}

void test_invalid_fd_is_reported() {
    const auto result = check_connect_result(-1);
    expect(result.status == ConnectStatus::KError,
           "invalid fd completion check fails");
    expect(result.error_code == EBADF,
           "invalid fd completion check reports EBADF");
}

}  // namespace

int main() {
    test_nonblocking_connect_completes();
    test_invalid_ip_is_reported();
    test_refused_connection_is_reported();
    test_invalid_fd_is_reported();
    std::cout << "All upstream connector tests passed.\n";
    return EXIT_SUCCESS;
}
