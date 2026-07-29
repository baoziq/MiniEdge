#include "unique_fd.h"
#include "tcp_listener.h"
#include "common.h"
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <cstddef>
#include <optional>
#include <sys/epoll.h>
#include <system_error>
#include <unistd.h>
#include <unordered_map>

int main() {
    std::unordered_map<int, std::optional<UniqueFd>> connection;

    Listener listener = Listener::create(8001);
    if (set_nonblocking(listener.fd()) < 0) {
        std::cerr << "set_nonblocking failed: " << std::strerror(errno) << std::endl;
        return 1;
    }

    UniqueFd epoller(epoll_create1(0));
    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = listener.fd();
    
    if (epoll_ctl(epoller.get(), EPOLL_CTL_ADD, listener.fd(), &event) < 0) {
        throw std::system_error {
            errno,
            std::generic_category(),
            "epoll_ctl"
        } ;
        return 1;
    }
    epoll_event events[16];
    while (true) {
        int ready = epoll_wait(epoller.get(), &events[0], 16, -1);
        if (ready < 0) {
            throw std::system_error {
                errno,
                std::generic_category(),
                "epoll_wait"
            };
            return 1;
        }
        for (int i = 0; i < ready; i++) {
            int fd = events[i].data.fd;
            if (fd == listener.fd()) {
                // new connection coming
                std::cout << "new connection comming\n";
                std::optional<UniqueFd> client = listener.accept_connection();
                std::cout << "after accept\n";
                if (!client.has_value()) {
                    std::cout << "No pending connection now\n";
                    return 0;
                }
                set_nonblocking(client->get());
                epoll_event client_event{};
                client_event.events = EPOLLIN;
                client_event.data.fd = client->get();
                if (epoll_ctl(epoller.get(), EPOLL_CTL_ADD, client->get(), &client_event) < 0) {
                    throw std::system_error {
                        errno,
                        std::generic_category(),
                        "epoll_ctl_add client_fd"
                    };
                }
                connection[client->get()] = std::move(client);
            } else {
                if (connection.find(fd) == connection.end()) {
                    continue;
                }
                std::cout << "starting reading\n";
                char buffer[1024];
                ssize_t n = read(fd, buffer, sizeof(buffer));
                if (n > 0 ){
                    write(STDOUT_FILENO, buffer, n);
                } else if (n == 0) {
                    std::cout << "connection has closed\n";
                    if (epoll_ctl(epoller.get(), EPOLL_CTL_DEL, fd, nullptr) < 0) {
                        throw std::system_error {
                            errno,
                            std::generic_category(),
                            "epoll_ctl_del"
                        };
                        return 1;
                    }
                    connection.erase(fd);
                } else {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        continue;
                    } else {
                        throw std::system_error {
                            errno,
                            std::generic_category(),
                            "read"
                        };
                        return 1;
                    }
                }
            }
        }
    }
    
    return 0;

}