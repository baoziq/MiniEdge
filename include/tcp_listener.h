#pragma once

#include "unique_fd.h"

class Listener {
    Listener();
    ~Listener();

private:
    UniqueFd fd;
};