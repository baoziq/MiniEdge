#include "unique_fd.h"
#include "tcp_listener.h"
#include "common.h"
#include "epoller.h"
#include "connection.h"

#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <cstddef>
#include <stdexcept>
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
            std::uint32_t event = epoller.event(i).events;
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
                Connection connection(std::move(*client));
                auto [it, inserted] = connections.try_emplace(client_fd, std::move(connection));
                if (!inserted) {
                    continue;
                }
                epoller.add(client_fd, kReadEvents);
            } else {
                auto it = connections.find(fd);
                if (it == connections.end()) {
                    continue;
                }
                Connection& connection = it->second;
                if (event & EPOLLIN) {
                    auto read_flag = connection.handle_read();
                    if (read_flag == ReadResult::KError || read_flag == ReadResult::KPeerClosed) {
                        epoller.remove(fd);
                        connections.erase(fd);
                        continue;
                    }
                    // 读完
                    std::string_view str = connection.input();
                    if (!connection.queue_output(str)) {
                        throw std::out_of_range("queue_output");
                    }
                    connection.consume(str.size());

                    if (connection.has_pending_output()) {
                        auto write_flag = connection.handle_write();
                        if (write_flag == WriteResult::KDrained) {
                            epoller.modify(fd, kReadEvents);
                            continue;
                        } else if (write_flag == WriteResult::KWouldBlock) {
                            epoller.modify(fd, kReadEvents | EPOLLOUT);
                            continue;
                        } else {
                            epoller.remove(fd);
                            connections.erase(fd);
                            continue;
                        }
                    }
                    if (connection.has_pending_output()) {
                        epoller.modify(fd, EPOLLOUT);
                    }
                    
                }
                
                if (event & EPOLLOUT) {
                    auto write_flag = connection.handle_write();
                    if (write_flag == WriteResult::KDrained) {
                        epoller.modify(fd, kReadEvents);
                        continue;
                    } else if (write_flag == WriteResult::KWouldBlock) {
                        epoller.modify(fd, kReadEvents | EPOLLOUT);
                        continue;
                    } else {
                        epoller.remove(fd);
                        connections.erase(fd);
                        continue;
                    }

                }

            }
        }
    }
    
    return 0;

}
