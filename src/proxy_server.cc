#include "proxy_server.h"

#include "common.h"
#include "http_parser.h"
#include "upstream_connector.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <string_view>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <system_error>
#include <utility>

namespace {

constexpr auto KIdleTimeout = std::chrono::seconds(30);
constexpr std::size_t KUpstreamReadBufferSize = 16 * 1024;
constexpr std::uint32_t KUpstreamWriteEvents =
    EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
constexpr std::uint32_t KUpstreamReadEvents =
    EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP;

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

std::string make_upstream_request(std::string_view header) {
    const std::size_t request_line_end = header.find("\r\n");
    const std::size_t header_end = header.find("\r\n\r\n");

    std::string result;
    result.reserve(header.size() + 19);
    result.append(header.substr(0, request_line_end + 2));

    std::size_t line_start = request_line_end + 2;
    while (line_start < header_end) {
        const std::size_t line_end = header.find("\r\n", line_start);
        const std::string_view line =
            header.substr(line_start, line_end - line_start);
        const std::size_t colon = line.find(':');

        if (!iequals(line.substr(0, colon), "connection")) {
            result.append(line);
            result.append("\r\n");
        }
        line_start = line_end + 2;
    }

    result.append("Connection: close\r\n\r\n");
    return result;
}

}  // namespace

ProxyServer::ProxyServer(std::uint16_t listen_port,
                         std::string upstream_host,
                         std::uint16_t upstream_port)
    : listener_(Listener::create(listen_port)),
      epoller_(1024),
      upstream_host_(std::move(upstream_host)),
      upstream_port_(upstream_port) {
    if (set_nonblocking(listener_.fd()) < 0) {
        throw std::system_error(
            errno,
            std::generic_category(),
            "set_nonblocking listener"
        );
    }
}

void ProxyServer::run() {
    epoller_.add(listener_.fd(), EPOLLIN);

    while (true) {
        const int ready = epoller_.wait(KTimerTickMs);

        for (int i = 0; i < ready; ++i) {
            const auto event = epoller_.event(static_cast<std::size_t>(i));
            handle_event(event.data.fd, event.events);
        }

        expire_idle_connections();
    }
}

void ProxyServer::handle_event(int fd, std::uint32_t events) {
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

void ProxyServer::accept_client() {
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
        std::cerr << "set_nonblocking client failed: "
                  << std::strerror(errno) << '\n';
        return;
    }

    const int client_fd = client->get();
    Connection connection(std::move(*client));
    auto [it, inserted] =
        connections_.try_emplace(client_fd, std::move(connection));
    if (!inserted) {
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
        std::cerr << "epoll_add client fd failed: " << error.what() << '\n';
    }
}

void ProxyServer::handle_upstream_event(int upstream_fd,
                                        std::uint32_t events) {
    ProxySession* session = registry_.find_by_upstream(upstream_fd);
    if (session == nullptr) {
        return;
    }

    const int client_fd = session->client_fd;
    auto client_it = connections_.find(client_fd);
    if (client_it == connections_.end()) {
        erase_upstream(client_fd);
        registry_.erase_by_client(client_fd);
        return;
    }

    Connection& client = client_it->second;
    bool upstream_failed = false;

    if (session->state == ProxyState::KConnectingUpstream &&
        (events & (EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP))) {
        const auto result = check_connect_result(upstream_fd);
        if (result.status == ConnectStatus::KError) {
            upstream_failed = true;
        } else {
            session->state = ProxyState::KSendingRequest;
        }
    }

    if (!upstream_failed && (events & EPOLLERR)) {
        upstream_failed = true;
    }

    if (!upstream_failed &&
        session->state == ProxyState::KSendingRequest &&
        (events & EPOLLOUT)) {
        while (session->upstream_write_offset <
               session->upstream_output.size()) {
            const std::size_t remaining =
                session->upstream_output.size() -
                session->upstream_write_offset;
            const ssize_t sent = send(
                upstream_fd,
                session->upstream_output.data() +
                    session->upstream_write_offset,
                remaining,
                MSG_NOSIGNAL
            );

            if (sent > 0) {
                session->upstream_write_offset +=
                    static_cast<std::size_t>(sent);
                continue;
            }
            if (sent < 0 && errno == EINTR) {
                continue;
            }
            if (sent < 0 &&
                (errno == EAGAIN || errno == EWOULDBLOCK)) {
                break;
            }

            upstream_failed = true;
            break;
        }

        if (!upstream_failed &&
            session->upstream_write_offset ==
                session->upstream_output.size()) {
            session->upstream_output.clear();
            session->upstream_write_offset = 0;
            session->state = ProxyState::KReadingResponse;
            try {
                epoller_.modify(upstream_fd, KUpstreamReadEvents);
            } catch (const std::system_error& error) {
                std::cerr << "failed to monitor upstream response: "
                          << error.what() << '\n';
                upstream_failed = true;
            }
        }
    }

    if (!upstream_failed &&
        session->state == ProxyState::KSendingRequest &&
        (events & (EPOLLHUP | EPOLLRDHUP))) {
        upstream_failed = true;
    }

    if (!upstream_failed &&
        session->state == ProxyState::KReadingResponse &&
        (events & (EPOLLIN | EPOLLHUP | EPOLLRDHUP))) {
        char buffer[KUpstreamReadBufferSize];
        bool upstream_closed = false;

        while (true) {
            const std::size_t available =
                KMaxOutputSize - client.pending_output_size();
            if (available == 0) {
                try {
                    epoller_.modify(
                        upstream_fd,
                        EPOLLERR | EPOLLHUP | EPOLLRDHUP
                    );
                } catch (const std::system_error& error) {
                    std::cerr << "failed to pause upstream reads: "
                              << error.what() << '\n';
                    close_session(client_it);
                    return;
                }
                break;
            }

            const std::size_t read_size =
                std::min(available, sizeof(buffer));
            const ssize_t received =
                recv(upstream_fd, buffer, read_size, 0);
            if (received > 0) {
                session->upstream_response_started = true;
                if (!client.queue_output(std::string_view(
                        buffer, static_cast<std::size_t>(received)))) {
                    close_session(client_it);
                    return;
                }
                continue;
            }
            if (received == 0) {
                upstream_closed = true;
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }

            upstream_failed = true;
            break;
        }

        if (upstream_closed && !session->upstream_response_started) {
            upstream_failed = true;
        }

        if (!upstream_failed && client.has_pending_output()) {
            try {
                epoller_.modify(client_fd, EPOLLOUT);
            } catch (const std::system_error& error) {
                std::cerr << "failed to enable client writes: "
                          << error.what() << '\n';
                close_session(client_it);
                return;
            }
        }

        if (!upstream_failed && upstream_closed) {
            erase_upstream(client_fd);
            session = registry_.find_by_client(client_fd);
            if (session == nullptr) {
                close_session(client_it);
                return;
            }
            session->state = ProxyState::KSendingResponse;
            client.mark_close_after_write();

            if (!client.has_pending_output()) {
                close_session(client_it);
            }
            return;
        }
    }

    if (!upstream_failed) {
        return;
    }

    const bool response_started = session->upstream_response_started;
    erase_upstream(client_fd);
    session = registry_.find_by_client(client_fd);
    if (session == nullptr) {
        close_session(client_it);
        return;
    }
    session->state = ProxyState::KSendingResponse;

    if (response_started) {
        client.mark_close_after_write();
    } else if (!queue_error_response(client, BAD_GATEWAY_RESPONSE)) {
        close_session(client_it);
        return;
    }

    if (!client.has_pending_output()) {
        close_session(client_it);
        return;
    }

    try {
        epoller_.modify(client_fd, EPOLLOUT);
    } catch (const std::system_error& error) {
        std::cerr << "failed to enable client error response: "
                  << error.what() << '\n';
        close_session(client_it);
    }
}

void ProxyServer::handle_client_event(int client_fd,
                                      std::uint32_t events) {
    auto it = connections_.find(client_fd);
    if (it == connections_.end()) {
        return;
    }

    Connection& connection = it->second;
    ProxySession* session = registry_.find_by_client(client_fd);
    if (session == nullptr) {
        close_session(it);
        return;
    }

    const bool reading_request =
        session->state == ProxyState::KReadingRequest;
    bool close = (events & EPOLLERR) != 0;

    if (!close && reading_request && !connection.peer_closed() &&
        !connection.close_after_write() &&
        (events & (EPOLLIN | EPOLLRDHUP))) {
        if (connection.handle_read() == ReadResult::KError) {
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
            } else {
                connection.mark_close_after_write();
            }
            break;
        }

        HttpRequest request{};
        if (parse_header_fields(connection.input(), request) ==
            HeaderFieldsParseStatus::KBadRequest) {
            if (!connection.queue_output(BAD_RESPONSE)) {
                close = true;
            } else {
                connection.mark_close_after_write();
            }
            break;
        }

        session->upstream_output = make_upstream_request(
            connection.input().substr(0, result.length)
        );
        connection.consume(result.length);

        auto connect_result =
            connect_upstream(upstream_host_, upstream_port_);
        if (connect_result.status == ConnectStatus::KError) {
            session->state = ProxyState::KSendingResponse;
            if (!queue_error_response(connection, BAD_GATEWAY_RESPONSE)) {
                close = true;
            }
            break;
        }

        const ConnectStatus connect_status = connect_result.status;
        const int upstream_fd = connect_result.fd.get();
        if (!registry_.bind_upstream(
                client_fd, std::move(connect_result.fd))) {
            session->state = ProxyState::KSendingResponse;
            if (!queue_error_response(
                    connection, INTERNAL_SERVER_ERROR_RESPONSE)) {
                close = true;
            }
            break;
        }

        session = registry_.find_by_client(client_fd);
        session->state = connect_status == ConnectStatus::KInProgress
            ? ProxyState::KConnectingUpstream
            : ProxyState::KSendingRequest;

        try {
            epoller_.add(upstream_fd, KUpstreamWriteEvents);
        } catch (const std::system_error& error) {
            std::cerr << "failed to add upstream fd " << upstream_fd
                      << " to epoll: " << error.what() << '\n';
            registry_.unbind_upstream(client_fd);
            session = registry_.find_by_client(client_fd);
            session->state = ProxyState::KSendingResponse;
            if (!queue_error_response(
                    connection, INTERNAL_SERVER_ERROR_RESPONSE)) {
                close = true;
            }
        }
        break;
    }

    if (!close && (events & EPOLLOUT)) {
        if (connection.handle_write() == WriteResult::KError) {
            close = true;
        }
    }

    session = registry_.find_by_client(client_fd);
    if (!close && session != nullptr &&
        session->state == ProxyState::KReadingResponse &&
        session->upstream_fd.has_value()) {
        try {
            epoller_.modify(
                session->upstream_fd->get(), KUpstreamReadEvents);
        } catch (const std::system_error& error) {
            std::cerr << "failed to resume upstream reads: "
                      << error.what() << '\n';
            close = true;
        }
    }

    if (!close && (events & EPOLLHUP)) {
        close = true;
    }

    session = registry_.find_by_client(client_fd);
    if (!close && session != nullptr &&
        connection.peer_closed() &&
        session->state == ProxyState::KReadingRequest &&
        !connection.has_pending_output()) {
        close = true;
    }

    if (!close && connection.close_after_write() &&
        !connection.has_pending_output()) {
        close = true;
    }

    if (close) {
        close_session(it);
        return;
    }

    session = registry_.find_by_client(client_fd);
    const bool should_read_request =
        session != nullptr &&
        session->state == ProxyState::KReadingRequest &&
        !connection.peer_closed() &&
        !connection.close_after_write();
    std::uint32_t interests = should_read_request ? kReadEvents : 0U;
    if (connection.has_pending_output()) {
        interests |= EPOLLOUT;
    }

    try {
        epoller_.modify(client_fd, interests);
    } catch (const std::system_error& error) {
        std::cerr << "failed to update client events: "
                  << error.what() << '\n';
        close_session(it);
    }
}

void ProxyServer::expire_idle_connections() {
    const auto now = Connection::Clock::now();
    for (auto it = connections_.begin(); it != connections_.end();) {
        if (it->second.idle_expired(now, KIdleTimeout)) {
            auto expired_it = it++;
            close_session(expired_it);
        } else {
            ++it;
        }
    }
}

void ProxyServer::close_session(Connections::iterator client_it) noexcept {
    const int client_fd = client_it->first;
    ProxySession* session = registry_.find_by_client(client_fd);
    if (session != nullptr && session->upstream_fd.has_value()) {
        remove_from_epoll(session->upstream_fd->get());
    }

    remove_from_epoll(client_fd);
    registry_.erase_by_client(client_fd);
    connections_.erase(client_it);
}

void ProxyServer::erase_upstream(int client_fd) noexcept {
    ProxySession* session = registry_.find_by_client(client_fd);
    if (session == nullptr || !session->upstream_fd.has_value()) {
        return;
    }

    remove_from_epoll(session->upstream_fd->get());
    registry_.unbind_upstream(client_fd);
}

void ProxyServer::remove_from_epoll(int fd) noexcept {
    try {
        epoller_.remove(fd);
    } catch (const std::system_error& error) {
        std::cerr << "failed to remove fd " << fd
                  << " from epoll: " << error.what() << '\n';
    }
}
