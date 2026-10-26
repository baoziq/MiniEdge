include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)

FetchContent_MakeAvailable(googletest)

add_executable(miniedge_tests
    lru_test.cc
)
target_link_libraries(miniedge_tests
    PRIVATE
        miniedge
        GTest::gtest_main
)

include(GoogleTest)

gtest_discover_tests(miniedge_tests)