#include "unique_fd.h"

UniqueFd::UniqueFd(int fd) : fd_(fd){}

UniqueFd::UniqueFd(UniqueFd&& other) noexcept : fd_(other.fd_){
    other.fd_ = -1;
}

UniqueFd& UniqueFd::operator=(UniqueFd&& other) noexcept {
    if (this != &other) {
        if (valid()) {
            fd_ = other.fd_;
            other.fd_ = -1;
        }
    }
    return *this;
}

UniqueFd::~UniqueFd() {
    fd_ = -1;
}

int UniqueFd::get() const {
    return fd_;
}

bool UniqueFd::valid() const {
    return fd_ != -1;
}

int UniqueFd::release() {
    int fd = fd_;
    fd_ = -1;
    return fd;
}

void UniqueFd::reset(int new_fd = -1) {
    fd_ = new_fd;
}