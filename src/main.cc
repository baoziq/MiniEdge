#include "unique_fd.h"
#include "tcp_listener.h"
#include "common.h"
#include "epoller.h"
#include "connection.h"
#include "http_parser.h"
#include "upstream_connector.h"
#include "session_registry.h"

#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <cstddef>
#include <string_view>
#include <sys/epoll.h>
#include <system_error>
#include <unistd.h>
#include <unordered_map>

using Connections = std::unordered_map<int, Connection>;
constexpr auto KIdleTimeout = std::chrono::seconds(30);
constexpr int KTimerTickMs = 1000;
constexpr std::string_view BAD_GATEWAY_RESPONSE =
    "HTTP/1.1 502 Bad Gateway\r\n"
    "Content-Length: 0\r\n"
    "Connection: close\r\n"
    "\r\n";
constexpr std::string_view INTERNAL_SERVER_ERROR_RESPONSE =
    "HTTP/1.1 500 Internal Server Error\r\n"
    "Content-Length: 0\r\n"
    "Connection: close\r\n"
    "\r\n";

void remove_from_epoll(Epoller& epoller, int fd) noexcept {
    try {
        epoller.remove(fd);
    } catch (const std::system_error& error) {
        std::cerr << "failed to remove fd " << fd
                  << " from epoll: " << error.what() << '\n';
    }
}

void close_session(Epoller& epoller, Connections& connections,
                   SessionRegistry& registry,
                   Connections::iterator client_it) noexcept {
    const int client_fd = client_it->first;
    ProxySession* session = registry.find_by_client(client_fd);
    if (session != nullptr && session->upstream_fd.has_value()) {
        remove_from_epoll(epoller, session->upstream_fd->get());
    }

    remove_from_epoll(epoller, client_fd);
    registry.erase_by_client(client_fd);
    connections.erase(client_it);
}

bool queue_error_response(Connection& connection,
                          std::string_view response) {
    if (!connection.queue_output(response)) {
        return false;
    }
    connection.mark_close_after_write();
    return true;
}

void erase_registered_upstream(Epoller& epoller,
                               SessionRegistry& registry,
                               int client_fd) noexcept {
    ProxySession* session = registry.find_by_client(client_fd);
    if (session != nullptr && session->upstream_fd.has_value()) {
        remove_from_epoll(epoller, session->upstream_fd->get());
    }
    registry.erase_by_client(client_fd);
}

void run_server() {
    Connections connections;

    Listener listener = Listener::create(8001);
    if (set_nonblocking(listener.fd()) < 0) {
        throw std::system_error(
            errno,
            std::generic_category(),
            "set_nonblocking listener"
        );
    }
    Epoller epoller(1024);
    SessionRegistry registry{};
    epoller.add(listener.fd(), EPOLLIN);
    while (true) {
        int ready = epoller.wait(KTimerTickMs);
        for (int i = 0; i < ready; i++) {
            int fd = epoller.event(i).data.fd;
            std::uint32_t event = epoller.event(i).events;
            if (fd == listener.fd()) {
                // new connection coming
                std::cout << "new connection comming\n";
                auto client = listener.accept_connection();
                if (!client.has_value()) {
                    std::cout << "No pending connection now\n";
                    continue;
                }
                if (set_nonblocking(client->get()) < 0) {
                    std::cerr << "set_nonblocking client failed: "
                              << std::strerror(errno) << '\n';
                    continue;
                }
                int client_fd = client->get();
                Connection connection(std::move(*client));
                auto [it, inserted] = connections.try_emplace(client_fd, std::move(connection));
                if (!inserted) {
                    continue;
                }
                if (!registry.create(client_fd)) {
                    connections.erase(it);
                    continue;
                }
                try {
                    epoller.add(client_fd, kReadEvents);
                } catch (...) {
                    registry.erase_by_client(client_fd);
                    connections.erase(it);
                    throw;
                }
                continue;
            }

            ProxySession* upstream_session = registry.find_by_upstream(fd);
            if (upstream_session != nullptr) {
                const int client_fd = upstream_session->client_fd;
                auto client_it = connections.find(client_fd);
                if (client_it == connections.end()) {
                    erase_registered_upstream(epoller, registry, client_fd);
                    continue;
                }

                bool upstream_failed = false;
                if (upstream_session->state ==
                        ProxyState::KConnectingUpstream &&
                    (event & (EPOLLOUT | EPOLLERR | EPOLLHUP |
                              EPOLLRDHUP))) {
                    const auto result = check_connect_result(fd);
                    if (result.status == ConnectStatus::KError ||
                        (event & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))) {
                        upstream_failed = true;
                    } else {
                        upstream_session->state =
                            ProxyState::KSendingRequest;
                        epoller.modify(fd, EPOLLRDHUP);
                    }
                } else if (event & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                    upstream_failed = true;
                }

                if (upstream_failed) {
                    erase_registered_upstream(epoller, registry, client_fd);
                    Connection& client = client_it->second;
                    if (!queue_error_response(client,
                                              BAD_GATEWAY_RESPONSE)) {
                        close_session(epoller, connections, registry,
                                      client_it);
                        continue;
                    }
                    epoller.modify(client_fd, EPOLLOUT);
                }
                continue;
            }

            auto it = connections.find(fd);
            if (it == connections.end()) {
                continue;
            }

            Connection& connection = it->second;
            ProxySession* client_session = registry.find_by_client(fd);
            const bool reading_request =
                client_session != nullptr &&
                client_session->state == ProxyState::KReadingRequest;
            bool close = (event & EPOLLERR) != 0;
            if (!close && reading_request && !connection.peer_closed() &&
                !connection.close_after_write() &&
                (event & (EPOLLIN | EPOLLRDHUP))) {
                const auto read_flag = connection.handle_read();
                if (read_flag == ReadResult::KError) {
                    close = true;
                }
            }
            while (!close && reading_request &&
                   !connection.close_after_write()) {
                const auto result =
                    parse_header_boundary(connection.input());

                if (result.status == HeaderParseStatus::KIncomplete) {
                    break;
                }
                if (result.status == HeaderParseStatus::KTooLarge) {
                    if (!connection.queue_output(BAD_RESPONSE)) {
                        close = true;
                        break;
                    }
                    connection.mark_close_after_write();
                    break;
                }

                HttpRequest request{};
                const auto header_flag =
                    parse_header_fields(connection.input(), request);
                if (header_flag == HeaderFieldsParseStatus::KBadRequest) {
                    if (!connection.queue_output(BAD_RESPONSE)) {
                        close = true;
                        break;
                    }
                    connection.mark_close_after_write();
                    break;
                }
                // 请求头解析成功
                auto connect_result =
                    connect_upstream("127.0.0.1", 9000);
                if (connect_result.status == ConnectStatus::KError) {
                    registry.erase_by_client(fd);
                    if (!queue_error_response(connection,
                                              BAD_GATEWAY_RESPONSE)) {
                        close = true;
                    }
                    break;
                }

                const ConnectStatus connect_status =
                    connect_result.status;
                const int upstream_fd = connect_result.fd.get();
                if (!registry.bind_upstream(
                        fd, std::move(connect_result.fd))) {
                    registry.erase_by_client(fd);
                    if (!queue_error_response(
                            connection,
                            INTERNAL_SERVER_ERROR_RESPONSE)) {
                        close = true;
                    }
                    break;
                }

                ProxySession* session = registry.find_by_client(fd);
                if (connect_status == ConnectStatus::KInProgress) {
                    session->state = ProxyState::KConnectingUpstream;
                } else {
                    session->state = ProxyState::KSendingRequest;
                }

                const std::uint32_t upstream_interests =
                    connect_status == ConnectStatus::KInProgress
                    ? EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP
                    : EPOLLERR | EPOLLHUP | EPOLLRDHUP;
                try {
                    epoller.add(upstream_fd, upstream_interests);
                } catch (const std::system_error& error) {
                    std::cerr << "failed to add upstream fd "
                              << upstream_fd << " to epoll: "
                              << error.what() << '\n';
                    registry.erase_by_client(fd);
                    if (!queue_error_response(
                            connection,
                            INTERNAL_SERVER_ERROR_RESPONSE)) {
                        close = true;
                    }
                }
                // 今天停在连接成功后的 KSendingRequest，不发送请求。
                break;
            }

            if (!close && event & EPOLLOUT) {
                auto write_flag = connection.handle_write();
                if (write_flag == WriteResult::KError) {
                    close = true;
                }
            }
            if (!close && (event & EPOLLHUP)) {
                close = true;
            }

            if (!close &&
                (connection.peer_closed() || connection.close_after_write()) &&
                !connection.has_pending_output()) {
                close = true;
            }

            if (close) {
                close_session(epoller, connections, registry, it);
                continue;
            }

            client_session = registry.find_by_client(fd);
            const bool should_read_request =
                client_session != nullptr &&
                client_session->state == ProxyState::KReadingRequest &&
                !connection.peer_closed() &&
                !connection.close_after_write();
            std::uint32_t interests =
                should_read_request ? kReadEvents : 0U;
            if (connection.has_pending_output()) {
                interests |= EPOLLOUT;
            }
            epoller.modify(fd, interests);
        }
        const auto now = Connection::Clock::now();
        for (auto it = connections.begin(); it != connections.end();) {
            if (it->second.idle_expired(now, KIdleTimeout)) {
                auto expired_it = it++;
                close_session(epoller, connections, registry, expired_it);
            } else {
                it++;
            }
        }
    }
}


int main() {
    try {
        run_server();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "fatal error: " << error.what() << '\n';
        return 1;
    }
}
