#pragma once

#include <unistd.h>

class UniqueFd {
public:
    UniqueFd(int fd = -1);
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    UniqueFd(UniqueFd&& other) noexcept;
    UniqueFd& operator=(UniqueFd&& other) noexcept;

    ~UniqueFd();

    int get() const;
    bool valid() const;
    int release();
    void reset(int new_fd = -1);
private:
    int fd_;
};