#include "storage/disk_manager.h"

#include <cassert>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "gtest/gtest.h"

constexpr int MAX_FILES = 32;
constexpr int MAX_PAGES = 128;
const std::string TEST_DB_NAME = "DiskManagerTest_db";  // 以TEST_DB_NAME作为存放测试文件的根目录名

// Add by jiawen
class DiskManagerTest : public ::testing::Test {
   public:
    std::unique_ptr<DiskManager> disk_manager_;

   public:
    // This function is called before every test.
    void SetUp() override {
        ::testing::Test::SetUp();
        // 对于每个测试点，创建一个disk manager
        disk_manager_ = std::make_unique<DiskManager>();
        // 如果测试目录不存在，则先创建测试目录
        if (!disk_manager_->is_dir(TEST_DB_NAME)) {
            disk_manager_->create_dir(TEST_DB_NAME);
        }
        assert(disk_manager_->is_dir(TEST_DB_NAME));  // 检查是否创建目录成功
        // 进入测试目录
        if (chdir(TEST_DB_NAME.c_str()) < 0) {
            throw UnixError();
        }
    }

    // This function is called after every test.
    void TearDown() override {
        // 返回上一层目录
        if (chdir("..") < 0) {
            throw UnixError();
        }
        assert(disk_manager_->is_dir(TEST_DB_NAME));
    };

    /**
     * @brief 将buf填充size个字节的随机数据
     */
    void rand_buf(char *buf, int size) {
        srand((unsigned)time(nullptr));
        for (int i = 0; i < size; i++) {
            int rand_ch = rand() & 0xff;
            buf[i] = rand_ch;
        }
    }
};

/**
 * @brief 测试文件操作 create/open/close/destroy file
 * @note lab1 计分：5 points
 */
TEST_F(DiskManagerTest, FileOperation) {
    std::vector<std::string> filenames(MAX_FILES);   // MAX_FILES=32
    std::unordered_map<int, std::string> fd2name;    // fd -> filename
    for (size_t i = 0; i < filenames.size(); i++) {  // 创建MAX_FILES个文件
        auto &filename = filenames[i];
        filename = "FileOperationTestFile" + std::to_string(i);
        // 清理残留文件
        if (disk_manager_->is_file(filename)) {
            disk_manager_->destroy_file(filename);
        }
        // 测试异常：如果没有创建文件就打开文件
        try {
            disk_manager_->open_file(filename);
            assert(false);
        } catch (const FileNotFoundError &e) {
        }
        // 创建文件
        disk_manager_->create_file(filename);
        EXPECT_EQ(disk_manager_->is_file(filename), true);  // 检查是否创建文件成功
        try {
            disk_manager_->create_file(filename);
            assert(false);
        } catch (const FileExistsError &e) {
        }
        // 打开文件
        int fd = disk_manager_->open_file(filename);
        fd2name[fd] = filename;

        // 关闭后重新打开
        if (rand() % 5 == 0) {
            disk_manager_->close_file(fd);
            int new_fd = disk_manager_->open_file(filename);
            fd2name[new_fd] = filename;
        }
    }

    // 关闭&删除文件
    for (auto &entry : fd2name) {
        int fd = entry.first;
        auto &filename = entry.second;
        disk_manager_->close_file(fd);
        disk_manager_->destroy_file(filename);
        EXPECT_EQ(disk_manager_->is_file(filename), false);  // 检查是否删除文件成功
        try {
            disk_manager_->destroy_file(filename);
            assert(false);
        } catch (const FileNotFoundError &e) {
        }
    }
}

/**
 * @brief 测试读写页面，分配页面编号 read/write page, allocate page_no
 * @note lab1 计分：5 points
 */
TEST_F(DiskManagerTest, PageOperation) {
    const std::string filename = "PageOperationTestFile";
    // 清理残留文件
    if (disk_manager_->is_file(filename)) {
        disk_manager_->destroy_file(filename);
    }
    // 创建文件
    disk_manager_->create_file(filename);
    EXPECT_EQ(disk_manager_->is_file(filename), true);
    // 打开文件
    int fd = disk_manager_->open_file(filename);
    // 初始页面编号为0
    disk_manager_->set_fd2pageno(fd, 0);

    // 读写页面&分配页面编号（在单个文件上测试）
    char buf[PAGE_SIZE] = {0};
    char data[PAGE_SIZE] = {0};
    for (int page_no = 0; page_no < MAX_PAGES; page_no++) {
        // 分配页面编号
        int ret_page_no = disk_manager_->allocate_page(fd);  // 注意此处返回值是分配编号之前的值
        EXPECT_EQ(ret_page_no, page_no);
        // 读写页面
        rand_buf(data, PAGE_SIZE);                                // generate data
        disk_manager_->write_page(fd, page_no, data, PAGE_SIZE);  // write data to disk (data -> disk page)
        std::memset(buf, 0, sizeof(buf));                         // clear buf
        disk_manager_->read_page(fd, page_no, buf, PAGE_SIZE);    // read buf from disk (disk page -> buf)
        EXPECT_EQ(std::memcmp(buf, data, sizeof(buf)), 0);        // check if buf == data
    }

    // 关闭&删除文件
    disk_manager_->close_file(fd);
    disk_manager_->destroy_file(filename);
    EXPECT_EQ(disk_manager_->is_file(filename), false);
}
