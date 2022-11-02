//===----------------------------------------------------------------------===//
//
//                         Rucbase
//
// b_plus_tree_delete_test.cpp
//
// Identification: src/index/b_plus_tree_delete_test.cpp
//
// Copyright (c) 2022, RUC Deke Group
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// b_plus_tree_delete_test.cpp
//
// Identification: test/storage/b_plus_tree_delete_test.cpp
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <algorithm>
#include <cstdio>
#include <random>  // for std::default_random_engine

#include "gtest/gtest.h"

#define private public
#include "ix.h"
#undef private  // for use private variables in "ix.h"

#include "storage/buffer_pool_manager.h"

const std::string TEST_DB_NAME = "BPlusTreeDeleteTest_db";  // 以数据库名作为根目录
const std::string TEST_FILE_NAME = "table1";                // 测试文件名的前缀
const int index_no = 0;                                     // 索引编号
// 创建的索引文件名为"table1.0.idx"（TEST_FILE_NAME + index_no + .idx）

/** 注意：每个测试点只测试了单个文件！
 * 对于每个测试点，先创建和进入目录TEST_DB_NAME
 * 然后在此目录下创建和打开索引文件"table1.0.idx"，记录IxIndexHandle */

// Add by jiawen
class BPlusTreeTests : public ::testing::Test {
   public:
    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<BufferPoolManager> buffer_pool_manager_;
    std::unique_ptr<IxManager> ix_manager_;
    std::unique_ptr<IxIndexHandle> ih_;
    std::unique_ptr<Transaction> txn_;

   public:
    // This function is called before every test.
    void SetUp() override {
        ::testing::Test::SetUp();
        // For each test, we create a new IxManager
        disk_manager_ = std::make_unique<DiskManager>();
        buffer_pool_manager_ = std::make_unique<BufferPoolManager>(100, disk_manager_.get());
        ix_manager_ = std::make_unique<IxManager>(disk_manager_.get(), buffer_pool_manager_.get());
        txn_ = std::make_unique<Transaction>(0);

        // 如果测试目录不存在，则先创建测试目录
        if (!disk_manager_->is_dir(TEST_DB_NAME)) {
            disk_manager_->create_dir(TEST_DB_NAME);
        }
        assert(disk_manager_->is_dir(TEST_DB_NAME));
        // 进入测试目录
        if (chdir(TEST_DB_NAME.c_str()) < 0) {
            throw UnixError();
        }
        // 如果测试文件存在，则先删除原文件（最后留下来的文件存的是最后一个测试点的数据）
        if (ix_manager_->exists(TEST_FILE_NAME, index_no)) {
            ix_manager_->destroy_index(TEST_FILE_NAME, index_no);
        }
        // 创建测试文件
        ix_manager_->create_index(TEST_FILE_NAME, index_no, TYPE_INT, sizeof(int));
        assert(ix_manager_->exists(TEST_FILE_NAME, index_no));
        // 打开测试文件
        ih_ = ix_manager_->open_index(TEST_FILE_NAME, index_no);
        assert(ih_ != nullptr);
    }

    // This function is called after every test.
    void TearDown() override {
        ix_manager_->close_index(ih_.get());
        // ix_manager_->destroy_index(TEST_FILE_NAME, index_no);

        // 返回上一层目录
        if (chdir("..") < 0) {
            throw UnixError();
        }
        assert(disk_manager_->is_dir(TEST_DB_NAME));
    };

    void ToGraph(const IxIndexHandle *ih, IxNodeHandle *node, BufferPoolManager *bpm, std::ofstream &out) const {
        std::string leaf_prefix("LEAF_");
        std::string internal_prefix("INT_");
        if (node->IsLeafPage()) {
            IxNodeHandle *leaf = node;
            // Print node name
            out << leaf_prefix << leaf->GetPageNo();
            // Print node properties
            out << "[shape=plain color=green ";
            // Print data of the node
            out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
            // Print data
            out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">page_no=" << leaf->GetPageNo() << "</TD></TR>\n";
            out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">"
                << "max_size=" << leaf->GetMaxSize() << ",min_size=" << leaf->GetMinSize() << "</TD></TR>\n";
            out << "<TR>";
            for (int i = 0; i < leaf->GetSize(); i++) {
                out << "<TD>" << leaf->KeyAt(i) << "</TD>\n";
            }
            out << "</TR>";
            // Print table end
            out << "</TABLE>>];\n";
            // Print Leaf node link if there is a next page
            if (leaf->GetNextLeaf() != INVALID_PAGE_ID && leaf->GetNextLeaf() > 1) {
                // 注意加上一个大于1的判断条件，否则若GetNextPageNo()是1，会把1那个结点也画出来
                out << leaf_prefix << leaf->GetPageNo() << " -> " << leaf_prefix << leaf->GetNextLeaf() << ";\n";
                out << "{rank=same " << leaf_prefix << leaf->GetPageNo() << " " << leaf_prefix << leaf->GetNextLeaf()
                    << "};\n";
            }

            // Print parent links if there is a parent
            if (leaf->GetParentPageNo() != INVALID_PAGE_ID) {
                out << internal_prefix << leaf->GetParentPageNo() << ":p" << leaf->GetPageNo() << " -> " << leaf_prefix
                    << leaf->GetPageNo() << ";\n";
            }
        } else {
            IxNodeHandle *inner = node;
            // Print node name
            out << internal_prefix << inner->GetPageNo();
            // Print node properties
            out << "[shape=plain color=pink ";  // why not?
            // Print data of the node
            out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
            // Print data
            out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">page_no=" << inner->GetPageNo() << "</TD></TR>\n";
            out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">"
                << "max_size=" << inner->GetMaxSize() << ",min_size=" << inner->GetMinSize() << "</TD></TR>\n";
            out << "<TR>";
            for (int i = 0; i < inner->GetSize(); i++) {
                out << "<TD PORT=\"p" << inner->ValueAt(i) << "\">";
                if (inner->KeyAt(i) != 0) {  // 原判断条件是if (i > 0)
                    out << inner->KeyAt(i);
                } else {
                    out << " ";
                }
                out << "</TD>\n";
            }
            out << "</TR>";
            // Print table end
            out << "</TABLE>>];\n";
            // Print Parent link
            if (inner->GetParentPageNo() != INVALID_PAGE_ID) {
                out << internal_prefix << inner->GetParentPageNo() << ":p" << inner->GetPageNo() << " -> "
                    << internal_prefix << inner->GetPageNo() << ";\n";
            }
            // Print leaves
            for (int i = 0; i < inner->GetSize(); i++) {
                IxNodeHandle *child_node = ih->FetchNode(inner->ValueAt(i));
                ToGraph(ih, child_node, bpm, out);  // 继续递归
                if (i > 0) {
                    IxNodeHandle *sibling_node = ih->FetchNode(inner->ValueAt(i - 1));
                    if (!sibling_node->IsLeafPage() && !child_node->IsLeafPage()) {
                        out << "{rank=same " << internal_prefix << sibling_node->GetPageNo() << " " << internal_prefix
                            << child_node->GetPageNo() << "};\n";
                    }
                    bpm->UnpinPage(sibling_node->GetPageId(), false);
                }
            }
        }
        bpm->UnpinPage(node->GetPageId(), false);
    }

    /**
     * @brief 生成B+树可视化图
     *
     * @param bpm 缓冲池
     * @param outf dot文件名
     */
    void Draw(BufferPoolManager *bpm, const std::string &outf) {
        std::ofstream out(outf);
        out << "digraph G {" << std::endl;
        IxNodeHandle *node = ih_->FetchNode(ih_->file_hdr_.root_page);
        ToGraph(ih_.get(), node, bpm, out);
        out << "}" << std::endl;
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
     * @param ih
     */
    void check_leaf(const IxIndexHandle *ih) {
        // check leaf list
        page_id_t leaf_no = ih->file_hdr_.first_leaf;
        while (leaf_no != IX_LEAF_HEADER_PAGE) {
            IxNodeHandle *curr = ih->FetchNode(leaf_no);
            IxNodeHandle *prev = ih->FetchNode(curr->GetPrevLeaf());
            IxNodeHandle *next = ih->FetchNode(curr->GetNextLeaf());
            // Ensure prev->next == curr && next->prev == curr
            ASSERT_EQ(prev->GetNextLeaf(), leaf_no);
            ASSERT_EQ(next->GetPrevLeaf(), leaf_no);
            leaf_no = curr->GetNextLeaf();
            buffer_pool_manager_->UnpinPage(curr->GetPageId(), false);
            buffer_pool_manager_->UnpinPage(prev->GetPageId(), false);
            buffer_pool_manager_->UnpinPage(next->GetPageId(), false);
        }
    }

    /**
     * @brief dfs遍历整个树，检查孩子结点的第一个和最后一个key是否正确
     *
     * @param ih 树
     * @param now_page_no 当前遍历到的结点
     */
    void check_tree(const IxIndexHandle *ih, int now_page_no) {
        IxNodeHandle *node = ih->FetchNode(now_page_no);
        if (node->IsLeafPage()) {
            buffer_pool_manager_->UnpinPage(node->GetPageId(), false);
            return;
        }
        for (int i = 0; i < node->GetSize(); i++) {                 // 遍历node的所有孩子
            IxNodeHandle *child = ih->FetchNode(node->ValueAt(i));  // 第i个孩子
            // check parent
            assert(child->GetParentPageNo() == now_page_no);
            // check first key
            int node_key = node->KeyAt(i);  // node的第i个key
            int child_first_key = child->KeyAt(0);
            int child_last_key = child->KeyAt(child->GetSize() - 1);
            if (i != 0) {
                // 除了第0个key之外，node的第i个key与其第i个孩子的第0个key的值相同
                ASSERT_EQ(node_key, child_first_key);
            }
            if (i + 1 < node->GetSize()) {
                // 满足制约大小关系
                ASSERT_LT(child_last_key, node->KeyAt(i + 1));  // child_last_key < node->KeyAt(i + 1)
            }

            buffer_pool_manager_->UnpinPage(child->GetPageId(), false);

            check_tree(ih, node->ValueAt(i));  // 递归子树
        }
        buffer_pool_manager_->UnpinPage(node->GetPageId(), false);
    }

    /**
     * @brief
     *
     * @param ih
     * @param mock 函数外部记录插入/删除后的(key,rid)
     */
    void check_all(IxIndexHandle *ih, const std::multimap<int, Rid> &mock) {
        check_tree(ih, ih->file_hdr_.root_page);
        if (!ih->IsEmpty()) {
            check_leaf(ih);
        }

        for (auto &entry : mock) {
            int mock_key = entry.first;
            // test lower bound
            {
                auto mock_lower = mock.lower_bound(mock_key);        // multimap的lower_bound方法
                Iid iid = ih->lower_bound((const char *)&mock_key);  // IxIndexHandle的lower_bound方法
                Rid rid = ih->get_rid(iid);
                ASSERT_EQ(rid, mock_lower->second);
            }
            // test upper bound
            {
                auto mock_upper = mock.upper_bound(mock_key);
                Iid iid = ih->upper_bound((const char *)&mock_key);
                if (iid != ih->leaf_end()) {
                    Rid rid = ih->get_rid(iid);
                    ASSERT_EQ(rid, mock_upper->second);
                }
            }
        }

        // test scan
        IxScan scan(ih, ih->leaf_begin(), ih->leaf_end(), buffer_pool_manager_.get());
        auto it = mock.begin();
        int leaf_no = ih->file_hdr_.first_leaf;
        assert(leaf_no == scan.iid().page_no);
        // 注意在scan里面是iid的slot_no进行自增
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
 * @brief insert 1~10 and delete 1~9 (will draw pictures)
 * 
 * @note lab2 计分：10 points
 */
TEST_F(BPlusTreeTests, InsertAndDeleteTest1) {
    const int64_t scale = 10;
    const int64_t delete_scale = 9;  // 删除的个数最好小于scale，等于的话会变成空树
    const int order = 4;

    assert(order > 2 && order <= ih_->file_hdr_.btree_order);
    ih_->file_hdr_.btree_order = order;

    std::vector<int64_t> keys;
    for (int64_t key = 1; key <= scale; key++) {
        keys.push_back(key);
    }

    // insert keys
    const char *index_key;
    for (auto key : keys) {
        int32_t value = key & 0xFFFFFFFF;  // key的低32位
        Rid rid = {.page_no = static_cast<int32_t>(key >> 32),
                   .slot_no = value};  // page_id = (key>>32), slot_num = (key & 0xFFFFFFFF)
        index_key = (const char *)&key;
        bool insert_ret = ih_->insert_entry(index_key, rid, txn_.get());  // 调用Insert
        ASSERT_EQ(insert_ret, true);
    }
    Draw(buffer_pool_manager_.get(), "insert10.dot");

    // scan keys by GetValue()
    std::vector<Rid> rids;
    for (auto key : keys) {
        rids.clear();
        index_key = (const char *)&key;
        ih_->GetValue(index_key, &rids, txn_.get());  // 调用GetValue
        EXPECT_EQ(rids.size(), 1);

        int64_t value = key & 0xFFFFFFFF;
        EXPECT_EQ(rids[0].slot_no, value);
    }

    // delete keys
    std::vector<int64_t> delete_keys;
    for (int64_t key = 1; key <= delete_scale; key++) {  // 1~9
        delete_keys.push_back(key);
    }
    for (auto key : delete_keys) {
        index_key = (const char *)&key;
        bool delete_ret = ih_->delete_entry(index_key, txn_.get());  // 调用Delete
        ASSERT_EQ(delete_ret, true);

        Draw(buffer_pool_manager_.get(), "InsertAndDeleteTest1_delete" + std::to_string(key) + ".dot");
    }

    // scan keys by Ixscan
    int64_t start_key = *delete_keys.rbegin() + 1;
    int64_t current_key = start_key;
    int64_t size = 0;

    IxScan scan(ih_.get(), ih_->leaf_begin(), ih_->leaf_end(), buffer_pool_manager_.get());
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

/**
 * @brief insert 1~10 and delete 1,2,3,4,7,5 (will draw pictures)
 *
 * @note lab2 计分：10 points
 */
TEST_F(BPlusTreeTests, InsertAndDeleteTest2) {
    const int64_t scale = 10;
    const int order = 4;

    assert(order > 2 && order <= ih_->file_hdr_.btree_order);
    ih_->file_hdr_.btree_order = order;

    std::vector<int64_t> keys;
    for (int64_t key = 1; key <= scale; key++) {
        keys.push_back(key);
    }

    // insert keys
    const char *index_key;
    for (auto key : keys) {
        int32_t value = key & 0xFFFFFFFF;  // key的低32位
        Rid rid = {.page_no = static_cast<int32_t>(key >> 32),
                   .slot_no = value};  // page_id = (key>>32), slot_num = (key & 0xFFFFFFFF)
        index_key = (const char *)&key;
        bool insert_ret = ih_->insert_entry(index_key, rid, txn_.get());  // 调用Insert
        ASSERT_EQ(insert_ret, true);
    }
    Draw(buffer_pool_manager_.get(), "insert10.dot");

    // scan keys by GetValue()
    std::vector<Rid> rids;
    for (auto key : keys) {
        rids.clear();
        index_key = (const char *)&key;
        ih_->GetValue(index_key, &rids, txn_.get());  // 调用GetValue
        EXPECT_EQ(rids.size(), 1);

        int64_t value = key & 0xFFFFFFFF;
        EXPECT_EQ(rids[0].slot_no, value);
    }

    // delete keys
    std::vector<int64_t> delete_keys = {1, 2, 3, 4, 7, 5};
    for (auto key : delete_keys) {
        index_key = (const char *)&key;
        bool delete_ret = ih_->delete_entry(index_key, txn_.get());  // 调用Delete
        ASSERT_EQ(delete_ret, true);

        Draw(buffer_pool_manager_.get(), "InsertAndDeleteTest2_delete" + std::to_string(key) + ".dot");
    }
}

/**
 * @brief 随机插入和删除多个键值对
 * 
 * @note lab2 计分：20 points
 */
TEST_F(BPlusTreeTests, LargeScaleTest) {
    const int order = 255;  // 若order太小，而插入数据过多，将会超出缓冲池
    const int scale = 20000;

    if (order >= 2 && order <= ih_->file_hdr_.btree_order) {
        ih_->file_hdr_.btree_order = order;
    }
    int add_cnt = 0;
    int del_cnt = 0;
    std::multimap<int, Rid> mock;
    mock.clear();
    while (add_cnt + del_cnt < scale) {
        double dice = rand() * 1. / RAND_MAX;
        double insert_prob = 1. - mock.size() / (0.5 * scale);
        if (mock.empty() || dice < insert_prob) {
            // Insert
            int rand_key = rand() % scale;
            if (mock.find(rand_key) != mock.end()) {  // 防止插入重复的key
                // printf("重复key=%d!\n", rand_key);
                continue;
            }
            Rid rand_val = {.page_no = rand(), .slot_no = rand()};
            bool insert_ret = ih_->insert_entry((const char *)&rand_key, rand_val, txn_.get());  // 调用Insert
            ASSERT_EQ(insert_ret, true);
            mock.insert(std::make_pair(rand_key, rand_val));
            add_cnt++;
            // Draw(buffer_pool_manager_.get(),
            //      "MixTest2_" + std::to_string(num) + "_insert" + std::to_string(rand_key) + ".dot");
        } else {
            // Delete
            if (mock.size() == 1) {  // 只剩最后一个结点时不删除，以防变成空树
                continue;
            }
            int rand_idx = rand() % mock.size();
            auto it = mock.begin();
            for (int k = 0; k < rand_idx; k++) {
                it++;
            }
            int key = it->first;
            // printf("delete rand key=%d\n", key);
            bool delete_ret = ih_->delete_entry((const char *)&key, txn_.get());
            ASSERT_EQ(delete_ret, true);
            mock.erase(it);
            del_cnt++;
            // Draw(buffer_pool_manager_.get(),
            //      "MixTest2_" + std::to_string(num) + "_delete" + std::to_string(key) + ".dot");
        }
        // check_all(ih_.get(), mock);
    }
    std::cout << "Insert keys count: " << add_cnt << '\n' << "Delete keys count: " << del_cnt << '\n';
    check_all(ih_.get(), mock);
}
