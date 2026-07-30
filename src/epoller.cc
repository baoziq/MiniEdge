#include "epoller.h"

#include <cerrno>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <sys/epoll.h>
#include <system_error>

Epoller::Epoller(std::size_t max_events) {
    if (max_events == 0 ||
        max_events > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("max_events must be between 1 and INT_MAX");
    }
    events_.resize(max_events);

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
    }
}

void Epoller::remove(int fd) {
    if (epoll_ctl(epoll_fd_.get(), EPOLL_CTL_DEL, fd, nullptr) < 0) {
        throw std::system_error {
            errno,
            std::generic_category(),
            "epoll_del"
        };
    }
}

int Epoller::wait(int timeout_ms) {
    int ready;
    do {
        ready = epoll_wait(
            epoll_fd_.get(),
            events_.data(),
            static_cast<int>(events_.size()),
            timeout_ms
        );
    } while (ready < 0 && errno == EINTR);

    if (ready < 0) {
        throw std::system_error {
            errno,
            std::generic_category(),
            "epoll_wait"
        };
    }
    return ready;
}

const epoll_event& Epoller::event(std::size_t index) const {
    return events_.at(index);
}
