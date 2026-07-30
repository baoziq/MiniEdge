#include "epoller.h"

#include <cerrno>
#include <cstdint>
#include <sys/epoll.h>
#include <system_error>

Epoller::Epoller(std::size_t max_events) : events(max_events) {
    int fd = epoll_create1(0);
    if (fd < 0) {
        throw std::system_error {
            errno,
            std::generic_category(),
            "epoll_create1"
        };
    }
    epoll_fd_.reset(fd);
}

void Epoller::add(int fd, std::uint32_t events) {
    epoll_event event{};
    event.events = events;
    event.data.fd = fd;
    if (epoll_ctl(epoll_fd_.get(), EPOLL_CTL_ADD, fd, &event) < 0) {
        throw std::system_error {
            errno,
            std::generic_category(),
            "epoll_add"
        };
        return ;
    }
}

void Epoller::remove(int fd) {
    if (epoll_ctl(epoll_fd_.get(), EPOLL_CTL_DEL, fd, nullptr) < 0) {
        throw std::system_error {
            errno,
            std::generic_category(),
            "epoll_del"
        };
        return ;
    }
}

int Epoller::wait(int timeout_ms) {
    int ready = epoll_wait(epoll_fd_.get(), &events[0], events.size(), timeout_ms);
    if (ready < 0) {
        throw std::system_error {
            errno,
            std::generic_category(),
            "epoll_wait"
        };
        return -1;
    }
    return ready;
}

const epoll_event& Epoller::event(std::size_t index) const {
    if (index >= events.size()) {
        throw std::system_error {
            errno,
            std::generic_category(),
            "index out of events.size()"
        };
    }
    return events[index];
}