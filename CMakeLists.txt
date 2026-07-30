cmake_minimum_required(VERSION 3.16)

project(MiniEdge
    VERSION 0.1.0
    LANGUAGES CXX
)

add_library(miniedge
    src/tcp_listener.cc
    src/unique_fd.cc
    src/common.cc
    src/epoller.cc
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