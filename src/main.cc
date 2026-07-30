#include "unique_fd.h"
#include "tcp_listener.h"
#include "common.h"
#include "epoller.h"

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <cstddef>
#include <sys/epoll.h>
#include <unistd.h>
#include <unordered_map>

int main() {
    std::unordered_map<int, UniqueFd> connections;

    Listener listener = Listener::create(8001);
    if (set_nonblocking(listener.fd()) < 0) {
        std::cerr << "set_nonblocking failed: " << std::strerror(errno) << std::endl;
        return 1;
    }
    Epoller epoller(1024);
    epoller.add(listener.fd(), EPOLLIN);
    while (true) {
        int ready = epoller.wait(-1);
        for (int i = 0; i < ready; i++) {
            int fd = epoller.event(i).data.fd;
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
                epoller.add(client_fd, EPOLLIN);
                connections.emplace(client_fd, std::move(*client));
            } else {
                if (connections.find(fd) == connections.end()) {
                    continue;
                }
                char buffer[1024];
                ssize_t n = read(fd, buffer, sizeof(buffer));
                if (n > 0 ){
                    write(STDOUT_FILENO, buffer, n);
                } else if (n == 0) {
                    std::cout << "connection has closed\n";
                    epoller.remove(fd);
                    connections.erase(fd);
                } else {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        continue;
                    } else if (errno == EINTR) {
                        continue;
                    } else {
                        std::cerr << "read failed for fd " << fd << ": "
                                  << std::strerror(errno) << '\n';
                        epoller.remove(fd);
                        connections.erase(fd);
                    }
                }
            }
        }
    }
    
    return 0;

}
