#pragma once

#include "connection.h"
#include "epoller.h"
#include "tcp_listener.h"
#include "lru.h"
#include "http_parser.h"

#include <cstdint>
#include <string>
#include <unordered_map>

constexpr int KTimerTickMs = 1000;

class ProxyServer {
public:
    ProxyServer(std::uint16_t listen_port);
    void run();
    
private:
    using Connections = std::unordered_map<int, Connection>;
    
    void handle_event(int fd, std::uint32_t events);
    void accept_client();
    void handle_client_event(int fd, std::uint32_t events);
    void expire_idle_connections();

    void close_session(Connections::iterator client_it) noexcept;
    void remove_from_epoll(int fd) noexcept;

    Listener listener_;
    Epoller epoller_;
    Connections connections_;

    LRU<std::string, std::string> static_cache_{100};
    HttpRequestLine request_line_;
    HttpRequest request_;
    
};