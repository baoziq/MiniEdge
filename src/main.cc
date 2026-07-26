#include "unique_fd.h"
#include "tcp_listener.h"

int main() {
    Listener listener = Listener::create(8001);
    UniqueFd client = listener.accept_connection();
    char buffer[1024];
    ssize_t n = read(client.get(), buffer, sizeof(buffer));
    if (n > 0 ) {
        write(STDOUT_FILENO, buffer, n);
    }

}