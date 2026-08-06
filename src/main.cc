#include "unique_fd.h"
#include "tcp_listener.h"
#include "common.h"
#include "epoller.h"
#include "connection.h"
#include "http_parser.h"

#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <cstddef>
#include <sys/epoll.h>
#include <system_error>
#include <unistd.h>
#include <unordered_map>

using Connections = std::unordered_map<int, Connection>;

void close_connection(Epoller& epoller, Connections& connections,
                      Connections::iterator it) noexcept {
    const int fd = it->first;
    try {
        epoller.remove(fd);
    } catch (const std::system_error& error) {
        std::cerr << "failed to remove fd " << fd
                  << " from epoll: " << error.what() << '\n';
    }
    connections.erase(it);
}

void run_server() {
    std::unordered_map<int, Connection> connections;

    Listener listener = Listener::create(8001);
    if (set_nonblocking(listener.fd()) < 0) {
        throw std::system_error(
            errno,
            std::generic_category(),
            "set_nonblocking listener"
        );
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
                bool closed = false;
                Connection& connection = it->second;
                auto read_flag = connection.handle_read();
                if (read_flag == ReadResult::KError) {
                    closed = true;
                } else if (read_flag == ReadResult::KPeerClosed) {
                    closed = true;
                }
                auto header_flag = parse_header(connection.input());
                if (header_flag.status == HeaderParseStatus::KTooLarge) {
                    closed = true;
                }
                if (header_flag.status == HeaderParseStatus::KComplete) {
                    connection.tmp_send();
                    connection.consume(connection.input().size());
                }
                if (closed) {
                    close_connection(epoller, connections, it);
                }
                
            }
        }
    }
}

int main() {
    try {
        run_server();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "fatal error: " << error.what() << '\n';
        return 1;
    }
}
