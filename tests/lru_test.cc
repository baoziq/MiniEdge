#include "lru.h"

#include <cstdlib>
#include <iostream>
#include <string_view>

void expect(bool condition, std::string_view test_name) {
    if (!condition) {
        std::cerr << "[FAILED] " << test_name << '\n';
        std::exit(EXIT_FAILURE);
    }
    std::cout << "[PASSED] " << test_name << '\n';
}

void test_basic() {
    LRU lru(3);
    lru.put(1, 1);
    lru.put(2, 2);
    lru.put(3, 3);
    int res = lru.get(2);
    expect(res == 2, "test_basic");
}

void test1() {
    LRU lru(3);
    lru.put(1, 1);
    lru.put(2, 2);
    lru.put(3, 3);
    lru.put(4, 4);
    int res = lru.get(1);
    expect(res == -1, "test1");
}

void test2() {
    LRU lru(3);
    lru.put(1, 1);
    lru.put(2, 2);
    lru.put(3, 3);
    lru.get(1);
    lru.put(4, 4);
    int res = lru.get(2);
    expect(res == -1, "test2");
}

int main() {
    test_basic();
    test1();
    test2();
    return 0;
}