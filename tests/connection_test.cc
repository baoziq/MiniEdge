#include "unique_fd.h"
#include "tcp_listener.h"
#include "common.h"
#include "epoller.h"
#include "connection.h"

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
    std::unordered_map<int, Connection> connections;

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
                // Connection connection(std::move(*client));
                // connections.emplace(client_fd, std::move(connection));
                // epoller.add(client_fd, EPOLLIN);
                Connection connection(std::move(*client));
                connections.emplace(client_fd, connection);
            } else {
                auto it = connections.find(fd);
                if (it == connections.end()) {
                    continue;
                }
                Connection& connection = it->second;
                auto flag = connection.handle_read();
                if (flag == ReadResult::KError || flag == ReadResult::KPeerClosed) {
                    epoller.remove(fd);
                    connections.erase(fd);
                    continue;
                }
                std::cout << connection.input() << std::endl;
            }
        }
    }
    
    return 0;

}
