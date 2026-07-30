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
    Epoller epoller(1024);
    epoller.add(listener.fd(), EPOLLIN);
    while (true) {
        int ready = epoller.wait(-1);
        for (int i = 0; i < ready; i++) {
            int fd = epoller.event(i).data.fd;
            if (fd == listener.fd()) {
                // new connection coming
                std::cout << "new connection comming\n";
                std::optional<UniqueFd> client = listener.accept_connection();
                if (!client.has_value()) {
                    std::cout << "No pending connection now\n";
                    return 0;
                }
                set_nonblocking(client->get());
                epoller.add(client->get(), EPOLLIN);
                connection[client->get()] = std::move(client);
            } else {
                if (connection.find(fd) == connection.end()) {
                    continue;
                }
                char buffer[1024];
                ssize_t n = read(fd, buffer, sizeof(buffer));
                if (n > 0 ){
                    write(STDOUT_FILENO, buffer, n);
                } else if (n == 0) {
                    std::cout << "connection has closed\n";
                    epoller.remove(fd);
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