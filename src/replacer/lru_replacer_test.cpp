//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lru_replacer_test.cpp
//
// Identification: test/buffer/lru_replacer_test.cpp
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "replacer/lru_replacer.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <random>
#include <set>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

/**
 * @brief 简单测试LRUReplacer的基本功能
 * @note lab1 计分：5 points
 */
TEST(LRUReplacerTest, SimpleTest) {
    LRUReplacer lru_replacer(7);

    // Scenario: unpin six elements, i.e. add them to the replacer.
    lru_replacer.Unpin(1);
    lru_replacer.Unpin(2);
    lru_replacer.Unpin(3);
    lru_replacer.Unpin(4);
    lru_replacer.Unpin(5);
    lru_replacer.Unpin(6);
    lru_replacer.Unpin(1);
    EXPECT_EQ(6, lru_replacer.Size());

    // Scenario: get three victims from the lru.
    int value;
    lru_replacer.Victim(&value);
    EXPECT_EQ(1, value);
    lru_replacer.Victim(&value);
    EXPECT_EQ(2, value);
    lru_replacer.Victim(&value);
    EXPECT_EQ(3, value);

    // Scenario: pin elements in the replacer.
    // Note that 3 has already been victimized, so pinning 3 should have no effect.
    lru_replacer.Pin(3);
    lru_replacer.Pin(4);
    EXPECT_EQ(2, lru_replacer.Size());

    // Scenario: unpin 4. We expect that the reference bit of 4 will be set to 1.
    lru_replacer.Unpin(4);

    // Scenario: continue looking for victims. We expect these victims.
    lru_replacer.Victim(&value);
    EXPECT_EQ(5, value);
    lru_replacer.Victim(&value);
    EXPECT_EQ(6, value);
    lru_replacer.Victim(&value);
    EXPECT_EQ(4, value);
}

/**
 * @brief 加大数据量，测试LRUReplacer的基本功能
 * @note lab1 计分：5 points
 */
TEST(LRUReplacerTest, MixTest) {
    int result;
    int value_size = 10000;
    auto lru_replacer = new LRUReplacer(value_size);
    std::vector<int> value(value_size);
    for (int i = 0; i < value_size; i++) {
        value[i] = i;
    }
    auto rng = std::default_random_engine{};
    std::shuffle(value.begin(), value.end(), rng);

    for (int i = 0; i < value_size; i++) {
        lru_replacer->Unpin(value[i]);
    }
    EXPECT_EQ(value_size, lru_replacer->Size());

    // Pin and unpin 777
    lru_replacer->Pin(777);
    lru_replacer->Unpin(777);
    // Pin and unpin 0
    EXPECT_EQ(1, lru_replacer->Victim(&result));
    EXPECT_EQ(value[0], result);
    lru_replacer->Unpin(value[0]);

    for (int i = 0; i < value_size / 2; i++) {
        if (value[i] != value[0] && value[i] != 777) {
            lru_replacer->Pin(value[i]);
            lru_replacer->Unpin(value[i]);
        }
    }

    std::vector<int> lru_array;
    for (int i = value_size / 2; i < value_size; ++i) {
        if (value[i] != value[0] && value[i] != 777) {
            lru_array.push_back(value[i]);
        }
    }
    lru_array.push_back(777);
    lru_array.push_back(value[0]);
    for (int i = 0; i < value_size / 2; ++i) {
        if (value[i] != value[0] && value[i] != 777) {
            lru_array.push_back(value[i]);
        }
    }
    EXPECT_EQ(value_size, lru_replacer->Size());

    for (int e : lru_array) {
        EXPECT_EQ(true, lru_replacer->Victim(&result));
        EXPECT_EQ(e, result);
    }
    EXPECT_EQ(value_size - lru_array.size(), lru_replacer->Size());

    delete lru_replacer;
}

/**
 * @brief 并发测试LRUReplacer
 * @note lab1 计分：10 points
 */
TEST(LRUReplacerTest, ConcurrencyTest) {
    const int num_threads = 5;
    const int num_runs = 50;
    for (int run = 0; run < num_runs; run++) {
        int value_size = 1000;
        std::shared_ptr<LRUReplacer> lru_replacer{new LRUReplacer(value_size)};
        std::vector<std::thread> threads;
        int result;
        std::vector<int> value(value_size);
        for (int i = 0; i < value_size; i++) {
            value[i] = i;
        }
        auto rng = std::default_random_engine{};
        std::shuffle(value.begin(), value.end(), rng);

        for (int tid = 0; tid < num_threads; tid++) {
            threads.push_back(std::thread([tid, &lru_replacer, &value]() {
                int share = 1000 / 5;
                for (int i = 0; i < share; i++) {
                    lru_replacer->Unpin(value[tid * share + i]);
                }
            }));
        }

        for (int i = 0; i < num_threads; i++) {
            threads[i].join();
        }
        std::vector<int> out_values;
        for (int i = 0; i < value_size; i++) {
            EXPECT_EQ(1, lru_replacer->Victim(&result));
            out_values.push_back(result);
        }
        std::sort(value.begin(), value.end());
        std::sort(out_values.begin(), out_values.end());
        EXPECT_EQ(value, out_values);
        EXPECT_EQ(0, lru_replacer->Victim(&result));
    }
}
