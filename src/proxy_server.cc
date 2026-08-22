#include "proxy_server.h"
#include "common.h"
#include "connection.h"
#include "epoller.h"
#include "http_parser.h"
#include "session_registry.h"
#include "tcp_listener.h"
#include "unique_fd.h"
#include "upstream_connector.h"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <sys/epoll.h>

#include <iostream>
#include <system_error>

constexpr auto KIdleTimeout = std::chrono::seconds(30);
constexpr std::string_view INTERNAL_SERVER_ERROR_RESPONSE =
    "HTTP/1.1 500 Internal Server Error\r\n"
    "Content-Length: 0\r\n"
    "Connection: close\r\n"
    "\r\n";
constexpr std::string_view BAD_GATEWAY_RESPONSE =
    "HTTP/1.1 502 Bad Gateway\r\n"
    "Content-Length: 0\r\n"
    "Connection: close\r\n"
    "\r\n";

bool queue_error_response(Connection& connection,
                          std::string_view response) {
    if (!connection.queue_output(response)) {
        return false;
    }
    connection.mark_close_after_write();
    return true;
}

void ProxyServer::run() {
    try {
        epoller_.add(listener_.fd(), EPOLLIN);
    } catch (const std::system_error& error) {
        std::cerr << "epoll add listen_fd failed: " << error.what() << '\n';
        return;
    }
    
    while (true) {
        const int ready = epoller_.wait(KTimerTickMs);

        for (int i = 0; i < ready; i++) {
            const auto& event = epoller_.event(i);
            handle_event(event.data.fd, event.events);
        }

        expire_idle_connections();
    }
}

// 分发事件
void ProxyServer::handle_event(int fd, uint32_t events) {
    if (fd == listener_.fd()) {
        accept_client();
        return;
    }

    if (registry_.find_by_upstream(fd) != nullptr) {
        handle_upstream_event(fd, events);
        return;
    }

    if (connections_.find(fd) != connections_.end()) {
        handle_client_event(fd, events);
    }
}

// 收到来自客户端的连接请求，从就绪队列中取出client_fd
// 加入connection
// 加入epoll
// 加入registry
void ProxyServer::accept_client() {
    // new connection comming
    std::optional<UniqueFd> client;

    try {
        client = listener_.accept_connection();
    } catch (const std::system_error& error) {
        std::cerr << "accept failed: " << error.what() << '\n';
        return;
    }

    if (!client.has_value()) {
        return;
    }

    if (set_nonblocking(client->get()) < 0) {
        std::cerr << "set_nonblocking client failed: " << std::strerror(errno) << '\n';
        return;
    }

    int client_fd = client->get();
    Connection connection(std::move(*client));
    auto [it, instered] = connections_.try_emplace(client_fd, std::move(connection));
    if (!instered) {
        return;
    }
    if (!registry_.create(client_fd)) {
        connections_.erase(it);
        return;
    }

    try {
        epoller_.add(client_fd, kReadEvents);
    } catch (const std::system_error& error) {
        registry_.erase_by_client(client_fd);
        connections_.erase(it);
        std::cerr << "epoll_add client_fd failed: " << error.what() << '\n';
        return;
    }
}

// 收到上游的fd
void ProxyServer::handle_upstream_event(int upstream_fd, uint32_t events) {
    auto upstram_session = registry_.find_by_upstream(upstream_fd);
    

    
}

// 收到客户端的fd
void ProxyServer::handle_client_event(int client_fd, uint32_t events) {
    auto it = connections_.find(client_fd);
    Connection& connection = it->second;
    ProxySession* client_session = registry_.find_by_client(client_fd);
    const bool reading_request = 
        client_session != nullptr &&
        client_session->state == ProxyState::KReadingRequest;
    bool close = (events & EPOLLERR) != 0;
    if (!close && reading_request && !connection.peer_closed() &&
        !connection.close_after_write() &&
        (events & (EPOLLIN | EPOLLRDHUP))) {
        const auto read_flag = connection.handle_read();
        if (read_flag == ReadResult::KError) {
            close = true;
        }
    }

    while (!close && reading_request &&
        !connection.close_after_write()) {
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
        client_session->upstream_output = connection.input().substr(0, result.length);
        connection.consume(result.length);

        auto connect_result =
            connect_upstream(upstream_host_, upstream_port_);
        if (connect_result.status == ConnectStatus::KError) {
            registry_.erase_by_client(client_fd);
            if (!queue_error_response(connection, BAD_GATEWAY_RESPONSE)) {
                close = true;
            }
            break;
        }
        const ConnectStatus connect_status =
            connect_result.status;
        const int upstream_fd = connect_result.fd.get();
        if (!registry_.bind_upstream(client_fd, std::move(connect_result.fd))) {
            registry_.erase_by_client(client_fd);
            if (!queue_error_response(connection, INTERNAL_SERVER_ERROR_RESPONSE)) {
                close = true;
            }
            break;
        }
        ProxySession* session = registry_.find_by_client(client_fd);
        if (connect_status == ConnectStatus::KInProgress) {
            session->state = ProxyState::KConnectingUpstream;
        } else {
            session->state = ProxyState::KSendingRequest;
        }
        const std::uint32_t upstream_interests =
                    EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
        try {
            epoller_.add(upstream_fd, upstream_interests);
        } catch (const std::system_error& error) {
            std::cerr << "failed to add upstream fd "
                        << upstream_fd << " to epoll: "
                        << error.what() << '\n';
            registry_.erase_by_client(client_fd);
            if (!queue_error_response(
                    connection,
                    INTERNAL_SERVER_ERROR_RESPONSE)) {
                close = true;
            }
        }
        break;

    }

}

void ProxyServer::expire_idle_connections() {
    const auto now = Connection::Clock::now();
    for (auto it = connections_.begin(); it != connections_.end();) {
        if (it->second.idle_expired(now, KIdleTimeout)) {
            auto expired_it = it++;
            close_session(expired_it);
        } else {
            it++;
        }
    }
}