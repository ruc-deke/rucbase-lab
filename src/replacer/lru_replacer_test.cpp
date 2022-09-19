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

#include <algorithm>
#include <cstdio>
#include <memory>
#include <random>
#include <set>
#include <thread>  // NOLINT
#include <vector>

#include "replacer/lru_replacer.h"
#include "gtest/gtest.h"

TEST(LRUReplacerTest, SampleTest) {
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

TEST(LRUReplacerTest, Victim) {
  auto lru_replacer = new LRUReplacer(1010);

  // Empty and try removing
  int result;
  EXPECT_EQ(0, lru_replacer->Victim(&result)) << "Check your return value behavior for LRUReplacer::Victim";

  // Unpin one and remove
  lru_replacer->Unpin(11);
  EXPECT_EQ(1, lru_replacer->Victim(&result)) << "Check your return value behavior for LRUReplacer::Victim";
  EXPECT_EQ(11, result);

  // Unpin, remove and verify
  lru_replacer->Unpin(1);
  lru_replacer->Unpin(1);
  EXPECT_EQ(1, lru_replacer->Victim(&result));
  EXPECT_EQ(1, result);
  lru_replacer->Unpin(3);
  lru_replacer->Unpin(4);
  lru_replacer->Unpin(1);
  lru_replacer->Unpin(3);
  lru_replacer->Unpin(4);
  lru_replacer->Unpin(10);
  EXPECT_EQ(1, lru_replacer->Victim(&result));
  EXPECT_EQ(3, result);
  EXPECT_EQ(1, lru_replacer->Victim(&result));
  EXPECT_EQ(4, result);
  EXPECT_EQ(1, lru_replacer->Victim(&result));
  EXPECT_EQ(1, result);
  EXPECT_EQ(1, lru_replacer->Victim(&result));
  EXPECT_EQ(10, result);
  EXPECT_EQ(0, lru_replacer->Victim(&result)) << "Check your return value behavior for LRUReplacer::Victim";

  lru_replacer->Unpin(5);
  lru_replacer->Unpin(6);
  lru_replacer->Unpin(7);
  lru_replacer->Unpin(8);
  lru_replacer->Unpin(6);
  EXPECT_EQ(1, lru_replacer->Victim(&result));
  EXPECT_EQ(5, result);
  lru_replacer->Unpin(7);
  EXPECT_EQ(1, lru_replacer->Victim(&result));
  EXPECT_EQ(6, result);
  EXPECT_EQ(1, lru_replacer->Victim(&result));
  EXPECT_EQ(7, result);
  EXPECT_EQ(1, lru_replacer->Victim(&result));
  EXPECT_EQ(8, result);
  EXPECT_EQ(0, lru_replacer->Victim(&result)) << "Check your return value behavior for LRUReplacer::Victim";
  lru_replacer->Unpin(10);
  lru_replacer->Unpin(10);
  EXPECT_EQ(1, lru_replacer->Victim(&result));
  EXPECT_EQ(10, result);
  EXPECT_EQ(0, lru_replacer->Victim(&result));
  EXPECT_EQ(0, lru_replacer->Victim(&result));
  EXPECT_EQ(0, lru_replacer->Victim(&result));

  for (int i = 0; i < 1000; i++) {
    lru_replacer->Unpin(i);
  }
  for (int i = 10; i < 1000; i++) {
    EXPECT_EQ(1, lru_replacer->Victim(&result));
    EXPECT_EQ(i - 10, result);
  }
  EXPECT_EQ(10, lru_replacer->Size());

  delete lru_replacer;
}

TEST(LRUReplacerTest, Pin) {
  auto lru_replacer = new LRUReplacer(1010);

  // Empty and try removing
  int result;
  lru_replacer->Pin(0);
  lru_replacer->Pin(1);

  // Unpin one and remove
  lru_replacer->Unpin(11);
  lru_replacer->Pin(11);
  lru_replacer->Pin(11);
  EXPECT_EQ(false, lru_replacer->Victim(&result));
  lru_replacer->Pin(1);
  EXPECT_EQ(false, lru_replacer->Victim(&result));

  // Unpin, remove and verify
  lru_replacer->Unpin(1);
  lru_replacer->Unpin(1);
  lru_replacer->Pin(1);
  EXPECT_EQ(false, lru_replacer->Victim(&result));
  lru_replacer->Unpin(3);
  lru_replacer->Unpin(4);
  lru_replacer->Unpin(1);
  lru_replacer->Unpin(3);
  lru_replacer->Unpin(4);
  lru_replacer->Unpin(10);
  lru_replacer->Pin(3);
  EXPECT_EQ(1, lru_replacer->Victim(&result));
  EXPECT_EQ(4, result);
  EXPECT_EQ(1, lru_replacer->Victim(&result));
  EXPECT_EQ(1, result);
  EXPECT_EQ(1, lru_replacer->Victim(&result));
  EXPECT_EQ(10, result);
  EXPECT_EQ(0, lru_replacer->Victim(&result));

  lru_replacer->Unpin(5);
  lru_replacer->Unpin(6);
  lru_replacer->Unpin(7);
  lru_replacer->Unpin(8);
  lru_replacer->Unpin(6);
  lru_replacer->Pin(7);
  EXPECT_EQ(1, lru_replacer->Victim(&result));
  EXPECT_EQ(5, result);
  EXPECT_EQ(1, lru_replacer->Victim(&result));
  EXPECT_EQ(6, result);
  EXPECT_EQ(1, lru_replacer->Victim(&result));
  EXPECT_EQ(8, result);
  EXPECT_EQ(false, lru_replacer->Victim(&result));
  lru_replacer->Unpin(10);
  lru_replacer->Unpin(10);
  lru_replacer->Unpin(11);
  lru_replacer->Unpin(11);
  EXPECT_EQ(1, lru_replacer->Victim(&result));
  EXPECT_EQ(10, result);
  lru_replacer->Pin(11);
  EXPECT_EQ(0, lru_replacer->Victim(&result));

  for (int i = 0; i <= 1000; i++) {
    lru_replacer->Unpin(i);
  }
  int j = 0;
  for (int i = 100; i < 1000; i += 2) {
    lru_replacer->Pin(i);
    EXPECT_EQ(true, lru_replacer->Victim(&result));
    if (j <= 99) {
      EXPECT_EQ(j, result);
      j++;
    } else {
      EXPECT_EQ(j + 1, result);
      j += 2;
    }
  }
  lru_replacer->Pin(result);

  delete lru_replacer;
}

TEST(LRUReplacerTest, Size) {
  auto lru_replacer = new LRUReplacer(10010);

  EXPECT_EQ(0, lru_replacer->Size());
  lru_replacer->Unpin(1);
  EXPECT_EQ(1, lru_replacer->Size());
  lru_replacer->Unpin(2);
  EXPECT_EQ(2, lru_replacer->Size());
  lru_replacer->Unpin(3);
  EXPECT_EQ(3, lru_replacer->Size());
  lru_replacer->Unpin(3);
  EXPECT_EQ(3, lru_replacer->Size());
  lru_replacer->Unpin(5);
  EXPECT_EQ(4, lru_replacer->Size());
  lru_replacer->Unpin(6);
  EXPECT_EQ(5, lru_replacer->Size());
  lru_replacer->Unpin(1);
  EXPECT_EQ(5, lru_replacer->Size());

  // pop element from replacer
  int result;
  for (int i = 5; i >= 1; i--) {
    lru_replacer->Victim(&result);
    EXPECT_EQ(i - 1, lru_replacer->Size());
  }
  EXPECT_EQ(0, lru_replacer->Size());

  for (int i = 0; i < 10000; i++) {
    lru_replacer->Unpin(i);
    EXPECT_EQ(i + 1, lru_replacer->Size());
  }
  for (int i = 0; i < 10000; i += 2) {
    lru_replacer->Pin(i);
    EXPECT_EQ(9999 - (i / 2), lru_replacer->Size());
  }

  delete lru_replacer;
}

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
      threads.push_back(std::thread([tid, &lru_replacer, &value]() {  // NOLINT
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

TEST(LRUReplacerTest, IntegratedTest) {
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
