#pragma once

#include <unistd.h>

class UniqueFd {
public:
    UniqueFd() noexcept;
    explicit UniqueFd(int fd) noexcept;
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
