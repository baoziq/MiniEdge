add_library(
    common.cc
    connection.cc
    epoller.cc
    http_parser.cc
    proxy_server.cc
    session_registry.cc
    tcp_listener.cc
    static_file_handler.cc
)

target_include_directories(miniedge
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)