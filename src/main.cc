#include "proxy_server.h"

#include <exception>
#include <iostream>
#include <spdlog/spdlog.h>

int main() {
    // spdlog::set_level(spdlog::level::debug);
    // spdlog::set_pattern(
    //     "[%Y-%m-%d %H:%M:%S.%e] [%l] [thread %t] %v"
    // );
    spdlog::info("MiniEdge starting...");
    spdlog::debug("Debug logging enabled");
    try {
        ProxyServer server(8001, "127.0.0.1", 9000);
        server.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "fatal error: " << error.what() << '\n';
        return 1;
    }
}
