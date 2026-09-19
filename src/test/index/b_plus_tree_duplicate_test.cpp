// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

/**
 * @file b_plus_tree_duplicate_test.cpp
 * @brief Lab 2 重复键与唯一约束测试。
 *
 * 索引存放 (key, Rid)。普通索引允许同一 key 对应多条记录，并且这些记录可能
 * 因为结点分裂分布在多个相邻叶子中；唯一索引在插入重复 key 时抛出
 * DuplicateKeyError，并保持树不变。
 */

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <map>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

#define private public
#include "index/b_plus_tree.h"
#undef private  // 测试需要调小 tree_order_ 并检查叶子链表。

#include "b_plus_tree_invariant_checker.h"
#include "common/errors.h"
#include "index/index_manager.h"
#include "index/index_scan.h"
#include "record/rm.h"
#include "storage/buffer_pool_manager.h"
#include "system/sm.h"
#include "transaction/transaction.h"

namespace {

const std::string TEST_DB_NAME = "BPlusTreeDuplicateTest_db";
const std::string TEST_FILE_NAME = "table1";
const std::vector<std::string> TEST_COL = {"col1"};
const std::vector<ColMeta> TEST_COL_META = {
    {.tab_name = TEST_FILE_NAME, .name = "col1", .type = TYPE_INT, .len = sizeof(int), .offset = 0, .index = true}};

constexpr page_id_t kInsertFailed = -1;

Rid make_rid(int n) { return {.page_no = n / 1000 + 1, .slot_no = n % 1000}; }

bool rid_less(const Rid& left, const Rid& right) {
    return left.page_no != right.page_no ? left.page_no < right.page_no : left.slot_no < right.slot_no;
}

std::vector<Rid> sorted(std::vector<Rid> rids) {
    std::ranges::sort(rids, rid_less);
    return rids;
}

class BPlusTreeDuplicateTest : public ::testing::Test {
protected:
    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<BufferPoolManager> buffer_pool_manager_;
    std::unique_ptr<IndexManager> index_manager_;
    std::unique_ptr<BPlusTree> tree_;
    std::unique_ptr<Transaction> txn_;
    std::unique_ptr<RmManager> rm_;
    std::unique_ptr<SmManager> sm_;
    PinSnapshot pin_baseline_;

    void SetUp() override {
        disk_manager_ = std::make_unique<DiskManager>();
        buffer_pool_manager_ = std::make_unique<BufferPoolManager>(1024, disk_manager_.get());
        index_manager_ = std::make_unique<IndexManager>(disk_manager_.get(), buffer_pool_manager_.get());
        txn_ = std::make_unique<Transaction>(0);
        rm_ = std::make_unique<RmManager>(disk_manager_.get(), buffer_pool_manager_.get());
        sm_ = std::make_unique<SmManager>(disk_manager_.get(), buffer_pool_manager_.get(), rm_.get(),
                                          index_manager_.get());

        if (disk_manager_->is_dir(TEST_DB_NAME)) {
            const std::string cmd = "rm -rf " + TEST_DB_NAME;
            if (system(cmd.c_str()) < 0) {
                throw UnixError();
            }
        }
        sm_->create_db(TEST_DB_NAME);
        if (chdir(TEST_DB_NAME.c_str()) < 0) {
            throw UnixError();
        }
        std::vector<ColDef> coldef;
        coldef.push_back({.name = "col1", .type = TYPE_INT, .len = 4});
        coldef.push_back({.name = "col2", .type = TYPE_INT, .len = 4});
        sm_->create_table(TEST_FILE_NAME, coldef, nullptr);
        index_manager_->create_index(IndexMeta::make(TEST_FILE_NAME, TEST_COL_META));
        tree_ = index_manager_->open_index(TEST_FILE_NAME, TEST_COL);
        ASSERT_NE(tree_, nullptr);
        pin_baseline_ = buffer_pool_manager_->get_pin_snapshot();
    }

    void TearDown() override {
        EXPECT_EQ(buffer_pool_manager_->get_pin_snapshot(), pin_baseline_) << "buffer pool pin state changed";
        index_manager_->close_index(tree_.get());
        if (chdir("..") < 0) {
            throw UnixError();
        }
    }

    /** @brief 以给定唯一性重新创建空索引。 */
    void RecreateIndex(bool unique) {
        index_manager_->close_index(tree_.get());
        tree_.reset();
        index_manager_->destroy_index(TEST_FILE_NAME, TEST_COL);
        index_manager_->create_index(IndexMeta::make(TEST_FILE_NAME, TEST_COL_META, unique));
        tree_ = index_manager_->open_index(TEST_FILE_NAME, TEST_COL);
        ASSERT_NE(tree_, nullptr);
        pin_baseline_ = buffer_pool_manager_->get_pin_snapshot();
    }

    /** @brief 调小结点容量，使少量数据也能产生多层树和多个叶子。 */
    void UseSmallOrder(int order) {
        ASSERT_GT(order, 2);
        ASSERT_LE(order, tree_->file_header_->tree_order_);
        tree_->file_header_->tree_order_ = order;
    }

    ::testing::AssertionResult TreeInvariantsHold() const {
        const auto report = BPlusTreeInvariantChecker::check(*tree_);
        if (report.ok()) return ::testing::AssertionSuccess();
        return ::testing::AssertionFailure() << report.describe();
    }

    ::testing::AssertionResult Insert(int key, const Rid& rid) {
        if (tree_->insert_entry(reinterpret_cast<const char*>(&key), rid, txn_.get()) == kInsertFailed) {
            return ::testing::AssertionFailure() << "insert_entry(" << key << ") returned -1";
        }
        return ::testing::AssertionSuccess();
    }

    bool Delete(int key, const Rid& rid) {
        return tree_->delete_entry(reinterpret_cast<const char*>(&key), rid, txn_.get());
    }

    /** @brief 调用 get_value，并检查返回值与结果是否一致。 */
    std::vector<Rid> Lookup(int key) {
        std::vector<Rid> rids;
        const bool found = tree_->get_value(reinterpret_cast<const char*>(&key), &rids, txn_.get());
        EXPECT_EQ(found, !rids.empty()) << "get_value(" << key << ") return value disagrees with result";
        return rids;
    }

    /** @brief 用 [lower_bound(key), upper_bound(key)) 扫描同一 key 的全部记录。 */
    std::vector<Rid> ScanKey(int key) {
        const char* raw_key = reinterpret_cast<const char*>(&key);
        std::vector<Rid> rids;
        for (IndexScan scan(tree_.get(), tree_->lower_bound(raw_key), tree_->upper_bound(raw_key)); !scan.is_end();
             scan.next()) {
            rids.push_back(scan.rid());
        }
        return rids;
    }

    /** @brief 沿叶子链表读出全部 key，顺带统计每个 key 出现在多少个叶子中。 */
    std::vector<int> ScanAllKeys(std::map<int, int>* leaves_per_key = nullptr) {
        std::vector<int> keys;
        for (IndexScan scan(tree_.get(), tree_->leaf_begin(), tree_->leaf_end()); !scan.is_end(); scan.next()) {
            const IndexPosition& position = scan.position();
            IndexNode leaf = tree_->fetch_node(position.page_no);
            int key;
            memcpy(&key, leaf->get_key(position.slot_no), sizeof(key));
            if (leaves_per_key != nullptr && (position.slot_no == 0 || keys.empty() || keys.back() != key)) {
                ++(*leaves_per_key)[key];
            }
            keys.push_back(key);
        }
        return keys;
    }
};

/**
 * @brief 同一叶子中的重复 key 都能查到。
 * @note lab2 计分：2 points
 */
TEST_F(BPlusTreeDuplicateTest, NonUniqueKeepsDuplicatesInOneLeaf) {
    ASSERT_FALSE(tree_->is_unique());

    ASSERT_TRUE(Insert(7, make_rid(1)));
    ASSERT_TRUE(Insert(7, make_rid(2)));
    ASSERT_TRUE(TreeInvariantsHold());

    EXPECT_EQ(sorted(Lookup(7)), sorted({make_rid(1), make_rid(2)}));
}

/**
 * @brief 删除时必须同时匹配 key 和 Rid。
 * @note lab2 计分：2 points
 */
TEST_F(BPlusTreeDuplicateTest, NonUniqueDeleteRemovesOnlyMatchingRid) {
    ASSERT_TRUE(Insert(7, make_rid(1)));
    ASSERT_TRUE(Insert(7, make_rid(2)));

    EXPECT_FALSE(Delete(7, make_rid(3)));
    EXPECT_FALSE(Delete(8, make_rid(1)));
    ASSERT_TRUE(Delete(7, make_rid(1)));

    EXPECT_EQ(Lookup(7), std::vector<Rid>{make_rid(2)});
    ASSERT_TRUE(TreeInvariantsHold());
}

/**
 * @brief 同一 key 的记录跨越多个叶子后，查找和范围扫描仍要返回全部记录。
 * @note lab2 计分：3 points
 */
TEST_F(BPlusTreeDuplicateTest, DuplicatesSpanManyLeaves) {
    UseSmallOrder(3);

    constexpr int kSevens = 40;
    std::vector<std::pair<int, Rid>> entries;
    std::vector<Rid> sevens;
    entries.reserve(14 + kSevens);
    sevens.reserve(kSevens);
    for (int key = 1; key <= 14; ++key) {
        if (key != 6 && key != 7 && key != 8) entries.emplace_back(key, make_rid(key));
    }
    for (int i = 0; i < kSevens; ++i) {
        sevens.push_back(make_rid(100 + i));
        entries.emplace_back(7, sevens.back());
    }
    std::ranges::shuffle(entries, std::mt19937(20260918));
    for (const auto& [key, rid] : entries) {
        ASSERT_TRUE(Insert(key, rid));
    }
    ASSERT_TRUE(TreeInvariantsHold());

    std::map<int, int> leaves_per_key;
    const std::vector<int> keys = ScanAllKeys(&leaves_per_key);
    ASSERT_EQ(keys.size(), entries.size());
    ASSERT_TRUE(std::ranges::is_sorted(keys));
    ASSERT_GE(leaves_per_key[7], 3) << "测试数据应让 key=7 分布在多个叶子中";

    EXPECT_EQ(sorted(Lookup(7)), sorted(sevens));
    EXPECT_EQ(sorted(ScanKey(7)), sorted(sevens));
    EXPECT_EQ(Lookup(5), std::vector<Rid>{make_rid(5)});
    EXPECT_EQ(Lookup(9), std::vector<Rid>{make_rid(9)});
    EXPECT_TRUE(Lookup(6).empty());
    EXPECT_TRUE(Lookup(8).empty());
    EXPECT_TRUE(ScanKey(6).empty());
}

/**
 * @brief 按随机顺序删除分布在多个叶子中的重复 key。
 * @note lab2 计分：3 points
 */
TEST_F(BPlusTreeDuplicateTest, DeleteDuplicatesAcrossLeaves) {
    UseSmallOrder(3);

    constexpr int kSevens = 40;
    std::vector<Rid> sevens;
    sevens.reserve(kSevens);
    for (int key = 1; key <= 5; ++key) ASSERT_TRUE(Insert(key, make_rid(key)));
    for (int i = 0; i < kSevens; ++i) {
        sevens.push_back(make_rid(100 + i));
        ASSERT_TRUE(Insert(7, sevens.back()));
    }
    for (int key = 9; key <= 13; ++key) ASSERT_TRUE(Insert(key, make_rid(key)));
    ASSERT_TRUE(TreeInvariantsHold());

    EXPECT_FALSE(Delete(7, make_rid(999)));
    EXPECT_EQ(Lookup(7).size(), sevens.size());

    std::vector<Rid> remaining = sevens;
    std::ranges::shuffle(remaining, std::mt19937(7));
    while (!remaining.empty()) {
        const Rid victim = remaining.back();
        remaining.pop_back();
        ASSERT_TRUE(Delete(7, victim)) << "failed to delete rid slot " << victim.slot_no;
        ASSERT_FALSE(Delete(7, victim)) << "the same (key, rid) was deleted twice";
        ASSERT_TRUE(TreeInvariantsHold());
        ASSERT_EQ(sorted(Lookup(7)), sorted(remaining));
    }

    EXPECT_TRUE(Lookup(7).empty());
    for (int key : {1, 2, 3, 4, 5, 9, 10, 11, 12, 13}) {
        EXPECT_EQ(Lookup(key), std::vector<Rid>{make_rid(key)}) << "key=" << key;
    }
}

/**
 * @brief 小 key 域上的随机插入和删除，与 multimap 对照。
 * @note lab2 计分：2 points
 */
TEST_F(BPlusTreeDuplicateTest, RandomizedDuplicateWorkload) {
    UseSmallOrder(4);

    constexpr int kKeyRange = 24;
    constexpr int kOperations = 3000;
    std::mt19937 rng(42);
    std::multimap<int, Rid> mock;
    int next_rid = 0;

    for (int op = 0; op < kOperations; ++op) {
        const bool insert = mock.empty() || (mock.size() < 400 ? rng() % 10 < 6 : rng() % 10 < 3);
        if (insert) {
            const int key = static_cast<int>(rng() % kKeyRange);
            const Rid rid = make_rid(next_rid++);
            ASSERT_TRUE(Insert(key, rid));
            mock.emplace(key, rid);
        } else {
            auto victim = std::next(mock.begin(), static_cast<std::ptrdiff_t>(rng() % mock.size()));
            ASSERT_TRUE(Delete(victim->first, victim->second)) << "operation " << op;
            mock.erase(victim);
        }
        if (op % 100 == 0) {
            SCOPED_TRACE("after operation " + std::to_string(op));
            ASSERT_TRUE(TreeInvariantsHold());
        }
    }
    ASSERT_TRUE(TreeInvariantsHold());

    for (int key = 0; key < kKeyRange; ++key) {
        std::vector<Rid> expected;
        for (auto [it, end] = mock.equal_range(key); it != end; ++it) expected.push_back(it->second);
        EXPECT_EQ(sorted(Lookup(key)), sorted(expected)) << "key=" << key;
    }
    const std::vector<int> keys = ScanAllKeys();
    EXPECT_EQ(keys.size(), mock.size());
    EXPECT_TRUE(std::ranges::is_sorted(keys));
}

/**
 * @brief 唯一索引拒绝重复 key。
 * @note lab2 计分：1 point
 */
TEST_F(BPlusTreeDuplicateTest, UniqueIndexRejectsDuplicateKeys) {
    RecreateIndex(true);
    ASSERT_TRUE(tree_->is_unique());

    ASSERT_TRUE(Insert(7, make_rid(1)));
    int key = 7;
    EXPECT_THROW(tree_->insert_entry(reinterpret_cast<const char*>(&key), make_rid(2), txn_.get()),
                 DuplicateKeyError);

    EXPECT_EQ(Lookup(7), std::vector<Rid>{make_rid(1)});
}

/**
 * @brief 多层唯一索引中，任意位置的重复 key 都被拒绝，且树保持不变。
 * @note lab2 计分：1 point
 */
TEST_F(BPlusTreeDuplicateTest, UniqueRejectionLeavesTreeUnchanged) {
    RecreateIndex(true);
    UseSmallOrder(3);

    constexpr int kKeys = 60;
    for (int key = 1; key <= kKeys; ++key) ASSERT_TRUE(Insert(key, make_rid(key)));
    ASSERT_TRUE(TreeInvariantsHold());

    for (int key = 1; key <= kKeys; ++key) {
        EXPECT_THROW(tree_->insert_entry(reinterpret_cast<const char*>(&key), make_rid(1000 + key), txn_.get()),
                     DuplicateKeyError)
            << "key=" << key;
    }
    ASSERT_TRUE(TreeInvariantsHold());
    EXPECT_EQ(ScanAllKeys().size(), static_cast<size_t>(kKeys));
    for (int key = 1; key <= kKeys; ++key) {
        EXPECT_EQ(Lookup(key), std::vector<Rid>{make_rid(key)}) << "key=" << key;
    }
}

/**
 * @brief 删除后可以再次插入同一 key。
 * @note lab2 计分：1 point
 */
TEST_F(BPlusTreeDuplicateTest, UniqueAllowsReinsertAfterDelete) {
    RecreateIndex(true);
    UseSmallOrder(3);

    for (int key = 1; key <= 20; ++key) ASSERT_TRUE(Insert(key, make_rid(key)));
    ASSERT_TRUE(Delete(10, make_rid(10)));
    ASSERT_TRUE(Insert(10, make_rid(500)));
    EXPECT_EQ(Lookup(10), std::vector<Rid>{make_rid(500)});

    int key = 10;
    EXPECT_THROW(tree_->insert_entry(reinterpret_cast<const char*>(&key), make_rid(501), txn_.get()),
                 DuplicateKeyError);
    ASSERT_TRUE(TreeInvariantsHold());
}

}  // namespace
