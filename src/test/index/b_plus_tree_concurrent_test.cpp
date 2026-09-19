// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <chrono>  // NOLINT
#include <cinttypes>
#include <cstdio>
#include <fstream>
#include <functional>
#include <random>  // for std::default_random_engine
#include <thread>  // NOLINT
#include <utility>

#include "gtest/gtest.h"

#define private public
#include "index/b_plus_tree.h"
#undef private  // 测试需要检查 B+ 树的内部不变式。

#include "b_plus_tree_invariant_checker.h"
#include "index/index_manager.h"
#include "index/index_scan.h"
#include "record/rm.h"
#include "storage/buffer_pool_manager.h"
#include "system/sm.h"
#include "transaction/transaction.h"

const std::string TEST_DB_NAME = "BPlusTreeConcurrentTest_db";  // 以数据库名作为根目录
const std::string TEST_FILE_NAME = "table1";                    // 测试文件名的前缀
const std::vector<std::string> TEST_COL = {"col1"};
const std::vector<ColMeta> TEST_COL_META = {
    {.tab_name = TEST_FILE_NAME, .name = "col1", .type = TYPE_INT, .len = sizeof(int), .offset = 0, .index = true}};
// 创建的索引文件名为"table1_col1.idx"（TEST_FILE_NAME + column name + .idx）

/** 注意：每个测试点只测试了单个文件！
 * 对于每个测试点，先创建和进入目录TEST_DB_NAME
 * 然后在此目录下创建和打开索引文件"table1_col1.idx"，记录BPlusTree */

class BPlusTreeConcurrentTest : public ::testing::Test {
public:
    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<BufferPoolManager> buffer_pool_manager_;
    std::unique_ptr<IndexManager> index_manager_;
    std::unique_ptr<BPlusTree> tree_;
    std::unique_ptr<Transaction> txn_;
    std::unique_ptr<RmManager> rm_;
    std::unique_ptr<SmManager> sm_;
    PinSnapshot pin_baseline_;

public:
    // This function is called before every test.
    void SetUp() override {
        ::testing::Test::SetUp();
        // For each test, we create a new IndexManager
        disk_manager_ = std::make_unique<DiskManager>();
        buffer_pool_manager_ = std::make_unique<BufferPoolManager>(200, disk_manager_.get());
        index_manager_ = std::make_unique<IndexManager>(disk_manager_.get(), buffer_pool_manager_.get());
        txn_ = std::make_unique<Transaction>(0);
        rm_ = std::make_unique<RmManager>(disk_manager_.get(), buffer_pool_manager_.get());
        sm_ = std::make_unique<SmManager>(disk_manager_.get(), buffer_pool_manager_.get(), rm_.get(),
                                          index_manager_.get());

        // 如果测试目录不存在，则先创建测试目录
        if (disk_manager_->is_dir(TEST_DB_NAME)) {
            std::string cmd = "rm -rf " + TEST_DB_NAME;
            if (system(cmd.c_str()) < 0) {
                throw UnixError();
            }
        }
        sm_->create_db(TEST_DB_NAME);
        assert(disk_manager_->is_dir(TEST_DB_NAME));
        // 进入测试目录
        if (chdir(TEST_DB_NAME.c_str()) < 0) {
            throw UnixError();
        }
        // 如果测试文件存在，则先删除原文件（最后留下来的文件存的是最后一个测试点的数据）
        if (index_manager_->exists(TEST_FILE_NAME, TEST_COL)) {
            index_manager_->destroy_index(TEST_FILE_NAME, TEST_COL);
        }
        std::vector<ColDef> coldef;
        coldef.push_back({.name = "col1", .type = TYPE_INT, .len = 4});
        coldef.push_back({.name = "col2", .type = TYPE_INT, .len = 4});
        sm_->create_table(TEST_FILE_NAME, coldef, nullptr);
        index_manager_->create_index(IndexMeta::make(TEST_FILE_NAME, TEST_COL_META));
        assert(index_manager_->exists(TEST_FILE_NAME, TEST_COL));
        // 打开测试文件
        tree_ = index_manager_->open_index(TEST_FILE_NAME, TEST_COL);
        assert(tree_ != nullptr);
        pin_baseline_ = buffer_pool_manager_->get_pin_snapshot();
    }

    // This function is called after every test.
    void TearDown() override {
        EXPECT_EQ(buffer_pool_manager_->get_pin_snapshot(), pin_baseline_) << "buffer pool pin state changed";
        index_manager_->close_index(tree_.get());
        // index_manager_->destroy_index(TEST_FILE_NAME, index_no);  // 若不删除数据库文件，则将保留最后一个测试点的数据

        // 返回上一层目录
        if (chdir("..") < 0) {
            throw UnixError();
        }
        assert(disk_manager_->is_dir(TEST_DB_NAME));
    };

    ::testing::AssertionResult tree_invariants_hold() const {
        const auto report = BPlusTreeInvariantChecker::check(*tree_);
        if (report.ok()) return ::testing::AssertionSuccess();
        return ::testing::AssertionFailure() << report.describe();
    }

    void ToGraph(const BPlusTree* tree, IndexNode node, BufferPoolManager* bpm, std::ofstream& out) const {
        std::string leaf_prefix("LEAF_");
        std::string internal_prefix("INT_");
        if (node->is_leaf_page()) {
            BPlusTreeNode* leaf = &node.node();
            // Print node name
            out << leaf_prefix << leaf->get_page_no();
            // Print node properties
            out << "[shape=plain color=green ";
            // Print data of the node
            out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
            // Print data
            out << "<TR><TD COLSPAN=\"" << leaf->get_size() << "\">page_no=" << leaf->get_page_no() << "</TD></TR>\n";
            out << "<TR><TD COLSPAN=\"" << leaf->get_size() << "\">"
                << "max_size=" << leaf->get_max_size() << ",min_size=" << leaf->get_min_size() << "</TD></TR>\n";
            out << "<TR>";
            for (int i = 0; i < leaf->get_size(); i++) {
                out << "<TD>" << *reinterpret_cast<int*>(leaf->get_key(i)) << "</TD>\n";
            }
            out << "</TR>";
            // Print table end
            out << "</TABLE>>];\n";
            // Print Leaf node link if there is a next page
            if (leaf->get_next_leaf() != INVALID_PAGE_ID && leaf->get_next_leaf() > 1) {
                // 注意加上一个大于1的判断条件，否则若GetNextPageNo()是1，会把1那个结点也画出来
                out << leaf_prefix << leaf->get_page_no() << " -> " << leaf_prefix << leaf->get_next_leaf() << ";\n";
                out << "{rank=same " << leaf_prefix << leaf->get_page_no() << " " << leaf_prefix
                    << leaf->get_next_leaf() << "};\n";
            }

            // Print parent links if there is a parent
            if (leaf->get_parent_page_no() != INVALID_PAGE_ID) {
                out << internal_prefix << leaf->get_parent_page_no() << ":p" << leaf->get_page_no() << " -> "
                    << leaf_prefix << leaf->get_page_no() << ";\n";
            }
        } else {
            BPlusTreeNode* inner = &node.node();
            // Print node name
            out << internal_prefix << inner->get_page_no();
            // Print node properties
            out << "[shape=plain color=pink ";  // why not?
            // Print data of the node
            out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
            // Print data
            out << "<TR><TD COLSPAN=\"" << inner->get_size() << "\">page_no=" << inner->get_page_no() << "</TD></TR>\n";
            out << "<TR><TD COLSPAN=\"" << inner->get_size() << "\">"
                << "max_size=" << inner->get_max_size() << ",min_size=" << inner->get_min_size() << "</TD></TR>\n";
            out << "<TR>";
            for (int i = 0; i < inner->get_size(); i++) {
                out << "<TD PORT=\"p" << inner->value_at(i) << "\">";
                out << inner->key_at(i);
                // if (inner->KeyAt(i) != 0) {  // 原判断条件是if (i > 0)
                //     out << inner->KeyAt(i);
                // } else {
                //     out << " ";
                // }
                out << "</TD>\n";
            }
            out << "</TR>";
            // Print table end
            out << "</TABLE>>];\n";
            // Print Parent link
            if (inner->get_parent_page_no() != INVALID_PAGE_ID) {
                out << internal_prefix << inner->get_parent_page_no() << ":p" << inner->get_page_no() << " -> "
                    << internal_prefix << inner->get_page_no() << ";\n";
            }
            // Print leaves
            for (int i = 0; i < inner->get_size(); i++) {
                IndexNode child_node = tree->fetch_node(inner->value_at(i));
                const bool child_is_leaf = child_node->is_leaf_page();
                const page_id_t child_page_no = child_node->get_page_no();
                ToGraph(tree, std::move(child_node), bpm, out);
                if (i > 0) {
                    IndexNode sibling_node = tree->fetch_node(inner->value_at(i - 1));
                    if (!sibling_node->is_leaf_page() && !child_is_leaf) {
                        out << "{rank=same " << internal_prefix << sibling_node->get_page_no() << " " << internal_prefix
                            << child_page_no << "};\n";
                    }
                }
            }
        }
    }

    /**
     * @brief 生成B+树可视化图
     *
     * @param bpm 缓冲池
     * @param outf dot文件名
     */
    void Draw(BufferPoolManager* bpm, const std::string& outf) {
        std::ofstream out(outf);
        out << "digraph G {\n";

        ToGraph(tree_.get(), tree_->fetch_node(tree_->file_header_->root_page_), bpm, out);
        out << "}\n";
        out.close();

        // 由dot文件生成png文件
        std::string prefix = outf;
        prefix.replace(outf.rfind(".dot"), 4, "");
        std::string png_name = prefix + ".png";
        std::string cmd = "dot -Tpng " + outf + " -o " + png_name;
        system(cmd.c_str());

        // printf("Generate picture: build/%s/%s\n", TEST_DB_NAME.c_str(), png_name.c_str());
        printf("Generate picture: %s\n", png_name.c_str());
    }

    /**------ 以下为辅助检查函数 ------*/

    /**
     * @brief
     *
     * @param tree
     * @param mock 函数外部记录插入/删除后的(key,rid)
     */
    void check_all(BPlusTree* tree, const std::multimap<int, Rid>& mock) {
        ASSERT_TRUE(tree_invariants_hold());

        for (auto& [mock_key, _] : mock) {
            // test lower bound
            {
                auto mock_lower = mock.lower_bound(mock_key);                        // multimap的lower_bound方法
                IndexPosition position = tree->lower_bound((const char*)&mock_key);  // BPlusTree的lower_bound方法
                Rid rid = tree->get_rid(position);
                ASSERT_EQ(rid, mock_lower->second);
            }
            // test upper bound
            {
                auto mock_upper = mock.upper_bound(mock_key);
                IndexPosition position = tree->upper_bound((const char*)&mock_key);
                if (position != tree->leaf_end()) {
                    Rid rid = tree->get_rid(position);
                    ASSERT_EQ(rid, mock_upper->second);
                }
            }
        }

        // test scan
        IndexScan scan(tree, tree->leaf_begin(), tree->leaf_end());
        auto it = mock.begin();
        int leaf_no = tree->file_header_->first_leaf_;
        assert(leaf_no == scan.position().page_no);
        // 注意在scan里面是position的slot_no进行自增
        while (!scan.is_end() && it != mock.end()) {
            Rid mock_rid = it->second;
            Rid rid = scan.rid();
            ASSERT_EQ(rid, mock_rid);
            // go to next slot_no
            it++;
            scan.next();
        }
        ASSERT_EQ(scan.is_end(), true);
        ASSERT_EQ(it, mock.end());
    }
};

// helper function to launch multiple threads
template <typename... Args>
void LaunchParallelTest(uint64_t num_threads, Args&&... args) {
    std::vector<std::thread> thread_group;
    thread_group.reserve(static_cast<size_t>(num_threads));

    // Launch a group of threads
    for (uint64_t thread_itr = 0; thread_itr < num_threads; ++thread_itr) {
        thread_group.push_back(std::thread(args..., thread_itr));
    }

    // Join the threads with the main thread
    for (uint64_t thread_itr = 0; thread_itr < num_threads; ++thread_itr) {
        thread_group[thread_itr].join();
    }
}

// only for DEBUG
int getThreadId() {
    // std::scoped_lock latch{latch_};
    std::stringstream ss;
    ss << std::this_thread::get_id();
    // ss << transaction->GetThreadId();
    uint64_t thread_id = std::stoull(ss.str());
    return static_cast<int>(thread_id % 13);
}

// helper function to insert
// 每个线程只插入 key % thread_num == thread_itr 的那部分 key，因此同一条 (key, rid) 不会被插入两次；
// 各线程的 key 交错分布，仍然会在同一批叶子上竞争。
void InsertHelper(BPlusTree* tree, const std::vector<int64_t>& keys, uint64_t thread_num, uint64_t thread_itr) {
    // create transaction
    Transaction* transaction = new Transaction(0);  // 注意，每个线程都有一个事务；不能从上层传入一个共用的事务
    const auto is_mine = [&](int64_t key) { return static_cast<uint64_t>(key) % thread_num == thread_itr; };

    const char* index_key;
    for (auto key : keys) {
        if (!is_mine(key)) continue;
        int32_t value = static_cast<int32_t>(static_cast<uint32_t>(key));
        Rid rid = {.page_no = static_cast<int32_t>(static_cast<uint64_t>(key) >> 32U), .slot_no = value};
        index_key = (const char*)&key;
        tree->insert_entry(index_key, rid, transaction);
    }

    std::vector<Rid> rids;
    for (auto key : keys) {
        if (!is_mine(key)) continue;
        rids.clear();
        index_key = (const char*)&key;
        tree->get_value(index_key, &rids, transaction);  // 调用GetValue
        EXPECT_EQ(rids.size(), 1);

        int64_t value = static_cast<uint32_t>(key);
        EXPECT_EQ(rids[0].slot_no, value);
    }

    delete transaction;
}

// helper function to delete
void DeleteHelper(BPlusTree* tree, const std::vector<int64_t>& keys, __attribute__((unused)) uint64_t thread_itr = 0) {
    // create transaction
    Transaction* transaction = new Transaction(0);  // 注意，每个线程都有一个事务；不能从上层传入一个共用的事务

    const char* index_key;
    for (auto key : keys) {
        index_key = (const char*)&key;
        Rid rid = {.page_no = static_cast<int32_t>(static_cast<uint64_t>(key) >> 32U),
                   .slot_no = static_cast<int32_t>(static_cast<uint32_t>(key))};
        tree->delete_entry(index_key, rid, transaction);
    }

    delete transaction;
}

/**
 * @brief concurrent insert 1~10000
 *
 * @note lab2 计分：10 points
 */
TEST_F(BPlusTreeConcurrentTest, InsertScaleTest) {
    const int64_t scale = 10000;
    const int thread_num = 50;
    const int order = 255;

    assert(order > 2 && order <= tree_->file_header_->tree_order_);
    tree_->file_header_->tree_order_ = order;

    // keys to Insert
    std::vector<int64_t> keys;
    for (int64_t key = 1; key <= scale; key++) {
        keys.push_back(key);
    }

    // randomized the insertion order
    auto rng = std::default_random_engine{};
    std::shuffle(keys.begin(), keys.end(), rng);

    // 这里调用了insert_entry，并且用thread_num个进程并发插入（并发查找也放进去了）
    LaunchParallelTest(thread_num, InsertHelper, tree_.get(), keys, static_cast<uint64_t>(thread_num));
    ASSERT_TRUE(tree_invariants_hold());
    printf("Insert key 1~%" PRId64 " finished\n", scale);

    int64_t start_key = 1;
    int64_t current_key = start_key;

    IndexScan scan(tree_.get(), tree_->leaf_begin(), tree_->leaf_end());
    while (!scan.is_end()) {
        auto rid = scan.rid();
        EXPECT_EQ(rid.page_no, 0);
        EXPECT_EQ(rid.slot_no, current_key);
        current_key = current_key + 1;
        scan.next();
    }
    EXPECT_EQ(current_key, keys.size() + 1);
}

/**
 * @brief concurrent insert 1~10000 and delete 1~9900
 *
 * @note lab2 计分：15 points
 */
TEST_F(BPlusTreeConcurrentTest, MixScaleTest) {
    const int64_t scale = 10000;
    const int64_t delete_scale = 9900;
    const int thread_num = 50;
    const int order = 255;

    assert(order > 2 && order <= tree_->file_header_->tree_order_);
    tree_->file_header_->tree_order_ = order;

    // keys to Insert
    std::vector<int64_t> keys;
    for (int64_t key = 1; key <= scale; key++) {
        keys.push_back(key);
    }
    // 这里调用了insert_entry，并且用thread_num个进程并发插入（包括并发查找）
    LaunchParallelTest(thread_num, InsertHelper, tree_.get(), keys, static_cast<uint64_t>(thread_num));
    ASSERT_TRUE(tree_invariants_hold());
    printf("Insert key 1~%" PRId64 " finished\n", scale);

    // keys to Delete
    std::vector<int64_t> delete_keys;
    for (int64_t key = 1; key <= delete_scale; key++) {
        delete_keys.push_back(key);
    }
    LaunchParallelTest(thread_num, DeleteHelper, tree_.get(), delete_keys);
    ASSERT_TRUE(tree_invariants_hold());
    printf("Delete key 1~%" PRId64 " finished\n", delete_scale);

    int64_t start_key = *delete_keys.rbegin() + 1;
    int64_t current_key = start_key;
    int64_t size = 0;

    IndexScan scan(tree_.get(), tree_->leaf_begin(), tree_->leaf_end());
    while (!scan.is_end()) {
        auto rid = scan.rid();
        EXPECT_EQ(rid.page_no, 0);
        EXPECT_EQ(rid.slot_no, current_key);
        current_key++;
        size++;
        scan.next();
    }
    EXPECT_EQ(size, keys.size() - delete_keys.size());
}
