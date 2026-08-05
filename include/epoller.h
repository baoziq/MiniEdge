#pragma once

#include "unique_fd.h"

#include <cstddef>
#include <cstdint>
#include <sys/epoll.h>
#include <vector>

constexpr std::uint32_t kReadEvents = EPOLLIN | EPOLLRDHUP;
class Epoller {
public:
    explicit Epoller(std::size_t max_events);
    void add(int fd, std::uint32_t events);
    void remove(int fd);
    int wait(int timeout_ms = -1);
    void modify(int fd, std::uint32_t events);
    const epoll_event& event(std::size_t index) const;
private:
    UniqueFd epoll_fd_;
    std::vector<epoll_event> events_;
};
