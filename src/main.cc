#include "proxy_server.h"

#include <exception>
#include <iostream>
#include <spdlog/spdlog.h>

int main() {
    spdlog::info("MiniEdge starting...");
    spdlog::debug("Debug logging enabled");
    try {
        ProxyServer server(8001);
        server.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "fatal error: " << error.what() << '\n';
        return 1;
    }
}
