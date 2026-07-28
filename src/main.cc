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
#include <regex>
#include <sys/epoll.h>
#include <system_error>
#include <unistd.h>

int main() {
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
            if (!client.has_value()) {
                std::cout << "No pending connection now\n";
                return 0;
            }
        } else {
            char buffer[1024];
            ssize_t n = read(fd, buffer, sizeof(buffer));
            if (n > 0 ){
                write(STDOUT_FILENO, buffer, n);
            }
        }
    }

    return 0;

}