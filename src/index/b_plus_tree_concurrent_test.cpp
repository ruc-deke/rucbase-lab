//===----------------------------------------------------------------------===//
//
//                         Rucbase
//
// b_plus_tree_concurrent_test.cpp
//
// Identification: src/index/b_plus_tree_concurrent_test.cpp
//
// Copyright (c) 2022, RUC Deke Group
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// b_plus_tree_concurrent_test.cpp
//
// Identification: test/storage/b_plus_tree_concurrent_test.cpp
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <chrono>  // NOLINT
#include <cstdio>
#include <functional>
#include <random>  // for std::default_random_engine
#include <thread>  // NOLINT

#include "gtest/gtest.h"

#define private public
#include "ix.h"
#undef private  // for use private variables in "ix.h"

#include "storage/buffer_pool_manager.h"

const std::string TEST_DB_NAME = "BPlusTreeConcurrentTest_db";  // 以数据库名作为根目录
const std::string TEST_FILE_NAME = "table1";                    // 测试文件名的前缀
const int index_no = 0;                                         // 索引编号
// 创建的索引文件名为"table1.0.idx"（TEST_FILE_NAME + index_no + .idx）

/** 注意：每个测试点只测试了单个文件！
 * 对于每个测试点，先创建和进入目录TEST_DB_NAME
 * 然后在此目录下创建和打开索引文件"table1.0.idx"，记录IxIndexHandle */

// Add by jiawen
class BPlusTreeConcurrentTest : public ::testing::Test {
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
        // ix_manager_->destroy_index(TEST_FILE_NAME, index_no);  // 若不删除数据库文件，则将保留最后一个测试点的数据

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
};

// helper function to launch multiple threads
template <typename... Args>
void LaunchParallelTest(uint64_t num_threads, Args &&...args) {
    std::vector<std::thread> thread_group;

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
void InsertHelper(IxIndexHandle *tree, const std::vector<int64_t> &keys,
                  __attribute__((unused)) uint64_t thread_itr = 0) {
    // create transaction
    Transaction *transaction = new Transaction(0);  // 注意，每个线程都有一个事务；不能从上层传入一个共用的事务

    const char *index_key;
    for (auto key : keys) {
        int32_t value = key & 0xFFFFFFFF;
        Rid rid = {.page_no = static_cast<int32_t>(key >> 32), .slot_no = value};
        index_key = (const char *)&key;
        tree->insert_entry(index_key, rid, transaction);
    }

    std::vector<Rid> rids;
    for (auto key : keys) {
        rids.clear();
        index_key = (const char *)&key;
        tree->GetValue(index_key, &rids, transaction);  // 调用GetValue
        EXPECT_EQ(rids.size(), 1);

        int64_t value = key & 0xFFFFFFFF;
        EXPECT_EQ(rids[0].slot_no, value);
    }

    delete transaction;
}

// helper function to delete
void DeleteHelper(IxIndexHandle *tree, const std::vector<int64_t> &keys,
                  __attribute__((unused)) uint64_t thread_itr = 0) {
    // create transaction
    Transaction *transaction = new Transaction(0);  // 注意，每个线程都有一个事务；不能从上层传入一个共用的事务

    const char *index_key;
    for (auto key : keys) {
        index_key = (const char *)&key;
        tree->delete_entry(index_key, transaction);
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

    assert(order > 2 && order <= ih_->file_hdr_.btree_order);
    ih_->file_hdr_.btree_order = order;

    // keys to Insert
    std::vector<int64_t> keys;
    for (int64_t key = 1; key <= scale; key++) {
        keys.push_back(key);
    }

    // randomized the insertion order
    auto rng = std::default_random_engine{};
    std::shuffle(keys.begin(), keys.end(), rng);

    // 这里调用了insert_entry，并且用thread_num个进程并发插入（并发查找也放进去了）
    LaunchParallelTest(thread_num, InsertHelper, ih_.get(), keys);
    printf("Insert key 1~%ld finished\n", scale);

    int64_t start_key = 1;
    int64_t current_key = start_key;

    IxScan scan(ih_.get(), ih_->leaf_begin(), ih_->leaf_end(), buffer_pool_manager_.get());
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
 * @note lab2 计分：20 points
 */
TEST_F(BPlusTreeConcurrentTest, MixScaleTest) {
    const int64_t scale = 10000;
    const int64_t delete_scale = 9900;
    const int thread_num = 50;
    const int order = 255;

    assert(order > 2 && order <= ih_->file_hdr_.btree_order);
    ih_->file_hdr_.btree_order = order;

    // keys to Insert
    std::vector<int64_t> keys;
    for (int64_t key = 1; key <= scale; key++) {
        keys.push_back(key);
    }
    // 这里调用了insert_entry，并且用thread_num个进程并发插入（包括并发查找）
    LaunchParallelTest(thread_num, InsertHelper, ih_.get(), keys);
    printf("Insert key 1~%ld finished\n", scale);

    // keys to Delete
    std::vector<int64_t> delete_keys;
    for (int64_t key = 1; key <= delete_scale; key++) {
        delete_keys.push_back(key);
    }
    LaunchParallelTest(thread_num, DeleteHelper, ih_.get(), delete_keys);
    printf("Delete key 1~%ld finished\n", delete_scale);

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
