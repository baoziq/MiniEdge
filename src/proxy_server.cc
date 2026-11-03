#include "proxy_server.h"

#include "common.h"
#include "connection.h"
#include "http_parser.h"
#include "static_file_handler.h"

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <spdlog/spdlog.h>
#include <string_view>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <system_error>
#include <utility>

namespace {

constexpr auto KIdleTimeout = std::chrono::seconds(30);


}  // namespace

ProxyServer::ProxyServer(std::uint16_t listen_port)
    : listener_(Listener::create(listen_port)),
      epoller_(1024) {
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
        spdlog::info("client connect");
        accept_client();
        return;
    }

    if (connections_.find(fd) != connections_.end()) {
        spdlog::info("client send");
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
    spdlog::info("add client in connections");

    try {
        epoller_.add(client_fd, kReadEvents);
    } catch (const std::system_error& error) {
        connections_.erase(it);
        std::cerr << "epoll_add client fd failed: " << error.what() << '\n';
    }
}

void ProxyServer::handle_client_event(int client_fd,
                                      std::uint32_t events) {
    auto it = connections_.find(client_fd);
    if (it == connections_.end()) {
        spdlog::error("unknown client_fd");
        return;
    }

    Connection& connection = it->second;
    if (events & EPOLLERR) {
        spdlog::error("EPOLLER");
        close_session(it);
        return;
    }
    if (events & EPOLLIN) {
        // request
        auto read_status = connection.handle_read();
        if (read_status == ReadResult::KError) {
            spdlog::error("read error");
            return;
        }
        auto header_status = parse_header_boundary(connection.input());
        switch (header_status.status) {
        case HeaderParseStatus::KComplete:
            connection.queue_output(OK);
            break;
        case HeaderParseStatus::KTooLarge:
            connection.queue_output(HEADER_TOO_LARGE);
            return;
        case HeaderParseStatus::KIncomplete:
            // todo
            break;
        }
        
        auto line_status = parse_line(connection.input(), res_);
        if (line_status == LineParseStatus::KBadRequest) {
            connection.queue_output(BAD_RESPONSE);
            return;
        }
        auto field_status = parse_header_fields(connection.input(), res_);
    }

    if (events & EPOLLOUT) {
        // response
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
    remove_from_epoll(client_fd);
    connections_.erase(client_it);
}

void ProxyServer::remove_from_epoll(int fd) noexcept {
    try {
        epoller_.remove(fd);
    } catch (const std::system_error& error) {
        std::cerr << "failed to remove fd " << fd
                  << " from epoll: " << error.what() << '\n';
    }
}
