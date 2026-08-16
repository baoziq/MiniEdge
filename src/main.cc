#include "unique_fd.h"
#include "tcp_listener.h"
#include "common.h"
#include "epoller.h"
#include "connection.h"
#include "http_parser.h"
#include "upstream_connector.h"

#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <cstddef>
#include <sys/epoll.h>
#include <system_error>
#include <unistd.h>
#include <unordered_map>

using Connections = std::unordered_map<int, Connection>;
constexpr auto KIdleTimeout = std::chrono::seconds(30);
constexpr int KTimerTickMs = 1000;
Epoller epoller(1024);
void close_connection(Epoller& epoller, Connections& connections,
                      Connections::iterator it) noexcept {
    const int fd = it->first;
    try {
        epoller.remove(fd);
    } catch (const std::system_error& error) {
        std::cerr << "failed to remove fd " << fd
                  << " from epoll: " << error.what() << '\n';
    }
    connections.erase(it);
}

void run_server() {
    std::unordered_map<int, Connection> connections;

    Listener listener = Listener::create(8001);
    if (set_nonblocking(listener.fd()) < 0) {
        throw std::system_error(
            errno,
            std::generic_category(),
            "set_nonblocking listener"
        );
    }
    
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
                epoller.add(client_fd, kReadEvents);
            } else {
                auto it = connections.find(fd);
                if (it == connections.end()) {
                    continue;
                }
                Connection& connection = it->second;
                bool close = (event & EPOLLERR) != 0;
                if (!close && !connection.peer_closed() &&
                    !connection.close_after_write() &&
                    (event & (EPOLLIN | EPOLLRDHUP))) {
                    auto read_flag = connection.handle_read();
                    if (read_flag == ReadResult::KError) {
                        close = true;
                    }
                }
                while (!close && !connection.close_after_write()) {
                    const auto result = parse_header_boundary(connection.input());

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

                    const std::string_view response = request.keep_alive
                        ? OK_KEEP_ALIVE_RESPONSE
                        : OK_CLOSE_RESPONSE;
                    if (!connection.queue_output(response)) {
                        close = true;
                        break;
                    }
                    connection.consume(result.length);
                    if (!request.keep_alive) {
                        connection.mark_close_after_write();
                        break;
                    }
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
                    close_connection(epoller, connections, it);
                    continue;
                }

                std::uint32_t interests =
                    (connection.peer_closed() || connection.close_after_write())
                    ? 0U
                    : kReadEvents;
                if (connection.has_pending_output()) {
                    interests |= EPOLLOUT;
                }
                epoller.modify(fd, interests);
            }
        }
        const auto now = Connection::Clock::now();
        for (auto it = connections.begin(); it != connections.end();) {
            if (it->second.idle_expired(now, KIdleTimeout)) {
                auto expired_it = it++;
                close_connection(epoller, connections, expired_it);
            } else {
                it++;
            }
        }
    }
}


void proxy_server() {
    auto res = connect_upstream("127.0.0.1", 9000);
    if (res.status == ConnectStatus::KConnected) {
        std::cout << "connect success\n";
        return;
    }
    if (res.status == ConnectStatus::KInProgress) {
        const int fd = res.fd.get();
        epoller.add(fd, EPOLLOUT);
        
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
