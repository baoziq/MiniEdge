cmake_minimum_required(VERSION 3.16)

project(MiniEdge
    VERSION 0.1.0
    LANGUAGES CXX
)

include(CTest)

add_library(miniedge
    src/tcp_listener.cc
    src/unique_fd.cc
    src/common.cc
    src/epoller.cc
    src/connection.cc
    src/http_parser.cc
    src/upstream_connector.cc
)

target_include_directories(miniedge
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(miniedge
    PUBLIC
        cxx_std_17
)

target_compile_options(miniedge
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

add_executable(miniedge_server
    src/main.cc
)

target_link_libraries(miniedge_server
    PRIVATE
        miniedge
)

target_compile_options(miniedge_server
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

target_compile_options(miniedge_server PRIVATE
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Debug>:-O0>
)

if(BUILD_TESTING)
    add_executable(http_parser_test
        tests/http_parser_test.cc
    )

    target_link_libraries(http_parser_test
        PRIVATE
            miniedge
    )

    target_compile_options(http_parser_test
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

    add_test(
        NAME http_parser_test
        COMMAND http_parser_test
    )

    add_executable(upstream_connector_test
        tests/upstream_connector_test.cc
        src/upstream_connector.cc
        src/common.cc
        src/unique_fd.cc
    )

    target_include_directories(upstream_connector_test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_compile_features(upstream_connector_test
        PRIVATE
            cxx_std_17
    )

    target_compile_options(upstream_connector_test
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

    add_test(
        NAME upstream_connector_test
        COMMAND upstream_connector_test
    )
endif()
