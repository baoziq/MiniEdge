#include "unique_fd.h"

#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <cerrno>

int main() {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }
    int optval = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
        perror("setsocketopt");
        fd = -1;
        return -1;
    }
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8001);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        fd = -1;
        return 1;
    }

    int backlog = 128;
    if (listen(fd, backlog) < 0) {
        perror("listen");
        fd = -1;
        return -1;
    }

    int client_fd = accept(fd, nullptr, nullptr);
    

}