#pragma once

#include "connection.h"
#include "epoller.h"
#include "session_registry.h"
#include "tcp_listener.h"
#include <cstdint>
#include <string>
#include <unordered_map>

constexpr int KTimerTickMs = 1000;

class ProxyServer {
public:
    ProxyServer(std::uint16_t listen_port,
                std::string upstream_host,
                std::uint16_t upstream_port);
    void run();
private:
    using Connections = std::unordered_map<int, Connection>;
    
    void handle_event(int fd, std::uint32_t events);
    void accept_client();
    void handle_client_event(int fd, std::uint32_t events);
    void handle_upstream_event(int fd, std::uint32_t events);
    void expire_idle_connections();

    void close_session(Connections::iterator client_it) noexcept;
    void erase_upstream(int client_fd) noexcept;
    void remove_from_epoll(int fd) noexcept;

    Listener listener_;
    Epoller epoller_;
    SessionRegistry registry_;
    Connections connections_;

    std::string upstream_host_;
    std::uint16_t upstream_port_;
};