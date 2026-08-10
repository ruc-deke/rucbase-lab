// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <random>  // for std::default_random_engine

#include "gtest/gtest.h"

#define private public
#include "index/b_plus_tree.h"
#undef private  // 测试需要检查 B+ 树的内部不变式。

#include "index/index_manager.h"
#include "index/index_scan.h"
#include "record/rm.h"
#include "storage/buffer_pool_manager.h"
#include "system/sm.h"
#include "transaction/transaction.h"
const std::string TEST_DB_NAME = "BPlusTreeInsertTest_db";  // 以数据库名作为根目录
const std::string TEST_FILE_NAME = "table1";                // 测试文件名的前缀
// const int index_no = 0;                                     // 索引编号
const std::vector<std::string> TEST_COL = {"col1"};
const std::vector<ColMeta> TEST_COL_META = {
    {.tab_name = TEST_FILE_NAME, .name = "col1", .type = TYPE_INT, .len = sizeof(int), .offset = 0, .index = true}};
// 创建的索引文件名为"table1_col1.idx"（TEST_FILE_NAME + column name + .idx）

/** 注意：每个测试点只测试了单个文件！
 * 对于每个测试点，先创建和进入目录TEST_DB_NAME
 * 然后在此目录下创建和打开索引文件"table1_col1.idx"，记录BPlusTree */

// Add by jiawen
class BPlusTreeTests : public ::testing::Test {
public:
    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<BufferPoolManager> buffer_pool_manager_;
    std::unique_ptr<IndexManager> index_manager_;
    std::unique_ptr<BPlusTree> tree_;
    std::unique_ptr<Transaction> txn_;
    std::unique_ptr<RmManager> rm_;
    std::unique_ptr<SmManager> sm_;

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
        index_manager_->create_index(TEST_FILE_NAME, TEST_COL_META);
        assert(index_manager_->exists(TEST_FILE_NAME, TEST_COL));
        // 打开测试文件
        tree_ = index_manager_->open_index(TEST_FILE_NAME, TEST_COL);
        assert(tree_ != nullptr);
    }

    // This function is called after every test.
    void TearDown() override {
        index_manager_->close_index(tree_.get());
        // index_manager_->destroy_index(TEST_FILE_NAME, index_no);  // 若不删除数据库文件，则将保留最后一个测试点的数据

        // 返回上一层目录
        if (chdir("..") < 0) {
            throw UnixError();
        }
        assert(disk_manager_->is_dir(TEST_DB_NAME));
    };

    void ToGraph(const BPlusTree* tree, BPlusTreeNode* node, BufferPoolManager* bpm, std::ofstream& out) const {
        std::string leaf_prefix("LEAF_");
        std::string internal_prefix("INT_");
        if (node->is_leaf_page()) {
            BPlusTreeNode* leaf = node;
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
            BPlusTreeNode* inner = node;
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
                BPlusTreeNode* child_node = tree->fetch_node(inner->value_at(i));
                ToGraph(tree, child_node, bpm, out);  // 继续递归
                if (i > 0) {
                    BPlusTreeNode* sibling_node = tree->fetch_node(inner->value_at(i - 1));
                    if (!sibling_node->is_leaf_page() && !child_node->is_leaf_page()) {
                        out << "{rank=same " << internal_prefix << sibling_node->get_page_no() << " " << internal_prefix
                            << child_node->get_page_no() << "};\n";
                    }
                    bpm->unpin_page(sibling_node->get_page_id(), false);
                }
            }
        }
        bpm->unpin_page(node->get_page_id(), false);
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

        BPlusTreeNode* node = tree_->fetch_node(tree_->file_header_->root_page_);
        ToGraph(tree_.get(), node, bpm, out);
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
     * @brief 检查叶子层的前驱指针和后继指针
     *
     * @param tree
     */
    void check_leaf(const BPlusTree* tree) {
        // check leaf list
        page_id_t leaf_no = tree->file_header_->first_leaf_;
        while (leaf_no != INDEX_LEAF_HEADER_PAGE) {
            BPlusTreeNode* curr = tree->fetch_node(leaf_no);
            BPlusTreeNode* prev = tree->fetch_node(curr->get_prev_leaf());
            BPlusTreeNode* next = tree->fetch_node(curr->get_next_leaf());
            // Ensure prev->next == curr && next->prev == curr
            ASSERT_EQ(prev->get_next_leaf(), leaf_no);
            ASSERT_EQ(next->get_prev_leaf(), leaf_no);
            leaf_no = curr->get_next_leaf();
            buffer_pool_manager_->unpin_page(curr->get_page_id(), false);
            buffer_pool_manager_->unpin_page(prev->get_page_id(), false);
            buffer_pool_manager_->unpin_page(next->get_page_id(), false);
        }
    }

    /**
     * @brief dfs遍历整个树，检查孩子结点的第一个和最后一个key是否正确
     *
     * @param tree 树
     * @param now_page_no 当前遍历到的结点
     */
    void check_tree(const BPlusTree* tree, int now_page_no) {
        BPlusTreeNode* node = tree->fetch_node(now_page_no);
        if (node->is_leaf_page()) {
            buffer_pool_manager_->unpin_page(node->get_page_id(), false);
            return;
        }
        for (int i = 0; i < node->get_size(); i++) {                     // 遍历node的所有孩子
            BPlusTreeNode* child = tree->fetch_node(node->value_at(i));  // 第i个孩子
            // check parent
            assert(child->get_parent_page_no() == now_page_no);
            // check first key
            int node_key = node->key_at(i);  // node的第i个key
            int child_first_key = child->key_at(0);
            int child_last_key = child->key_at(child->get_size() - 1);
            if (i != 0) {
                // 除了第0个key之外，node的第i个key与其第i个孩子的第0个key的值相同
                ASSERT_EQ(node_key, child_first_key);
            }
            if (i + 1 < node->get_size()) {
                // 满足制约大小关系
                ASSERT_LT(child_last_key, node->key_at(i + 1));  // child_last_key < node->KeyAt(i + 1)
            }

            buffer_pool_manager_->unpin_page(child->get_page_id(), false);

            check_tree(tree, node->value_at(i));  // 递归子树
        }
        buffer_pool_manager_->unpin_page(node->get_page_id(), false);
    }

    /**
     * @brief
     *
     * @param tree
     * @param mock 函数外部记录插入/删除后的(key,rid)
     */
    void check_all(BPlusTree* tree, const std::multimap<int, Rid>& mock) {
        check_tree(tree, tree->file_header_->root_page_);
        if (!tree->is_empty()) {
            check_leaf(tree);
        }

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

/**
 * @brief 插入10个key，范围为1~10，插入的value取key的低32位，使用GetValue()函数测试插入的value(即Rid)是否正确
 * 每次插入后都会调用Draw()函数生成一个B+树的图
 *
 * @note lab2 计分：10 points
 */
TEST_F(BPlusTreeTests, InsertTest) {
    const int64_t scale = 10;
    const int order = 3;

    assert(order > 2 && order <= tree_->file_header_->tree_order_);
    tree_->file_header_->tree_order_ = order;

    std::vector<int64_t> keys;
    for (int64_t key = 1; key <= scale; key++) {
        keys.push_back(key);
    }

    const char* index_key;
    for (auto key : keys) {
        int32_t value = static_cast<int32_t>(static_cast<uint32_t>(key));  // key的低32位
        Rid rid = {.page_no = static_cast<int32_t>(static_cast<uint64_t>(key) >> 32U),
                   .slot_no = value};  // page_id = (key>>32), slot_num = (key & 0xFFFFFFFF)
        index_key = (const char*)&key;
        bool insert_ret = tree_->insert_entry(index_key, rid, txn_.get());  // 调用Insert
        ASSERT_EQ(insert_ret, true);

        // Draw(buffer_pool_manager_.get(), "insert" + std::to_string(key) + ".dot");
    }

    std::vector<Rid> rids;
    for (auto key : keys) {
        rids.clear();
        index_key = (const char*)&key;
        tree_->get_value(index_key, &rids, txn_.get());  // 调用GetValue
        EXPECT_EQ(rids.size(), 1);

        int32_t value = static_cast<int32_t>(static_cast<uint32_t>(key));
        EXPECT_EQ(rids[0].slot_no, value);
    }

    // 找不到未插入的数据
    for (int key = scale + 1; key <= scale + 100; key++) {
        rids.clear();
        index_key = (const char*)&key;
        tree_->get_value(index_key, &rids, txn_.get());  // 调用GetValue
        EXPECT_EQ(rids.size(), 0);
    }
}

/**
 * @brief 随机插入1~10000
 *
 * @note lab2 计分：20 points
 */
TEST_F(BPlusTreeTests, LargeScaleTest) {
    const int64_t scale = 10000;
    const int order = 256;

    assert(order > 2 && order <= tree_->file_header_->tree_order_);
    tree_->file_header_->tree_order_ = order;

    std::vector<int64_t> keys;
    for (int64_t key = 1; key <= scale; key++) {
        keys.push_back(key);
    }

    // randomized the insertion order
    auto rng = std::default_random_engine{};
    std::shuffle(keys.begin(), keys.end(), rng);

    const char* index_key;
    for (auto key : keys) {
        int32_t value = static_cast<int32_t>(static_cast<uint32_t>(key));  // key的低32位
        Rid rid = {.page_no = static_cast<int32_t>(static_cast<uint64_t>(key) >> 32U),
                   .slot_no = value};  // page_id = (key>>32), slot_num = (key & 0xFFFFFFFF)
        index_key = (const char*)&key;
        bool insert_ret = tree_->insert_entry(index_key, rid, txn_.get());  // 调用Insert
        ASSERT_EQ(insert_ret, true);
    }

    // test GetValue
    std::vector<Rid> rids;
    for (auto key : keys) {
        rids.clear();
        index_key = (const char*)&key;
        tree_->get_value(index_key, &rids, txn_.get());  // 调用GetValue
        EXPECT_EQ(rids.size(), 1);

        int64_t value = static_cast<uint32_t>(key);
        EXPECT_EQ(rids[0].slot_no, value);
    }

    // test Ixscan
    int64_t start_key = 1;
    int64_t current_key = start_key;
    IndexScan scan(tree_.get(), tree_->leaf_begin(), tree_->leaf_end());
    while (!scan.is_end()) {
        int32_t insert_page_no = static_cast<int32_t>(static_cast<uint64_t>(current_key) >> 32U);
        int32_t insert_slot_no = static_cast<int32_t>(static_cast<uint32_t>(current_key));
        Rid rid = scan.rid();
        EXPECT_EQ(rid.page_no, insert_page_no);
        EXPECT_EQ(rid.slot_no, insert_slot_no);
        current_key++;
        scan.next();
    }
    EXPECT_EQ(current_key, keys.size() + 1);
}
