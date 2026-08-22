#include "proxy_server.h"

#include <exception>
#include <iostream>

int main() {
    try {
        ProxyServer server(8001, "127.0.0.1", 9000);
        server.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "fatal error: " << error.what() << '\n';
        return 1;
    }
}
