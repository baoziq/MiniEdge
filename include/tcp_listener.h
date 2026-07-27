#pragma once

#include "unique_fd.h"

#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <cerrno>
#include <system_error>

class Listener {
public:
    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;

    Listener(Listener&& other) noexcept = default;
    Listener& operator=(Listener&&) noexcept = default;

    static Listener create(uint16_t port);
    int fd() const noexcept {
        return fd_.get();
    }
    UniqueFd accept_connection() const;

private:
    explicit Listener(UniqueFd fd) : fd_(std::move(fd)) {}
    UniqueFd fd_;
};
