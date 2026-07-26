#include "tcp_listener.h"

Listener Listener::create(uint16_t port) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        throw std::system_error {
            errno,
            std::generic_category(),
            "socket"
        };
    }
    UniqueFd fd(listen_fd);
    
    int optval = 1;
    if (setsockopt(fd.get(), SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        throw std::system_error {
            errno,
            std::generic_category(),
            "setsocket"
        };
    }

    sockaddr_in address{};
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if (bind(fd.get(), (sockaddr*)&address, sizeof(address)) < 0) {
        throw std::system_error {
            errno,
            std::generic_category(),
            "bind"
        };
    }

    if (listen(fd.get(), 128) < 0) {
        throw std::system_error {
            errno,
            std::generic_category(),
            "listen"
        };
    }

    return Listener(std::move(fd));
}

UniqueFd Listener::accept_connection() const {
    int client_fd = accept(fd_.get(), nullptr, nullptr);
    if (client_fd < 0) {
        throw std::system_error {
            errno,
            std::generic_category(),
            "accept"
        };
    }
    return UniqueFd(client_fd);
}