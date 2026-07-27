#include "unique_fd.h"
#include "tcp_listener.h"
#include "common.h"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <cstddef>

int main() {
    Listener listener = Listener::create(8001);
    if (set_nonblocking(listener.fd()) < 0) {
        std::cerr << "set_nonblocking failed: " << std::strerror(errno) << std::endl;
        return 1;
    }
    int client_fd = accept(listener.fd(), nullptr, nullptr);
    if (client_fd == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            std::cout << "No int client_fd = ::accept pending connection now\n";
            return 0;
        }
        std::cerr << "accept failed\n" << std::strerror(errno) << std::endl;
        return 1;
    }
    UniqueFd client = listener.accept_connection();
    char buffer[1024];
    ssize_t n = read(client.get(), buffer, sizeof(buffer));
    if (n > 0 ) {
        write(STDOUT_FILENO, buffer, n);
    }

}