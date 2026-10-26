#include "lru.h"

#include <cstdlib>
#include <gtest/gtest.h>

TEST(LRUCacheTest, BasicPutAndGet) {
    LRU<int, int> cache(2);

    cache.put(1, 100);
    cache.put(2, 200);

    EXPECT_EQ(cache.get(1), 100);
    EXPECT_EQ(cache.get(2), 200);
}

TEST(LRUCacheTest, Eviction) {
    LRU<int, int> cache(2);

    cache.put(1, 100);
    cache.put(2, 200);

    cache.get(1);      // 1 变成最近使用
    cache.put(3, 300); // 应当淘汰 2

    EXPECT_EQ(cache.get(1), 100);
    EXPECT_EQ(cache.get(2), std::nullopt);
    EXPECT_EQ(cache.get(3), 300);
}