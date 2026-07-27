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
    std::optional<UniqueFd> client = listener.accept_connection();
    if (!client.has_value()) {
        std::cout << "No pending connection now\n";
        return 0;
    }   
    char buffer[1024];
    ssize_t n = read(client->get(), buffer, sizeof(buffer));
    if (n > 0 ) {
        write(STDOUT_FILENO, buffer, n);
    }

}