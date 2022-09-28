#include <chrono>

#include "gtest/gtest.h"
#include "transaction_manager.h"
#include "concurrency/lock_manager.h"
#include "execution/execution_manager.h"

const std::string TEST_DB_NAME = "LockManagerTestDB";

class LockManagerTest : public ::testing::Test {
   public:
    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<BufferPoolManager> buffer_pool_manager_;
    std::unique_ptr<RmManager> rm_manager_;
    std::unique_ptr<IxManager> ix_manager_;
    std::unique_ptr<SmManager> sm_manager_;
    std::unique_ptr<LogManager> log_manager_;
    std::unique_ptr<LockManager> lock_manager_;
    std::unique_ptr<TransactionManager> txn_manager_;

   public:
    void SetUp() override {
        ::testing::Test::SetUp();
        disk_manager_ = std::make_unique<DiskManager>();
        buffer_pool_manager_ = std::make_unique<BufferPoolManager>(BUFFER_POOL_SIZE, disk_manager_.get());
        rm_manager_ = std::make_unique<RmManager>(disk_manager_.get(), buffer_pool_manager_.get());
        ix_manager_ = std::make_unique<IxManager>(disk_manager_.get(), buffer_pool_manager_.get());
        sm_manager_ = std::make_unique<SmManager>(disk_manager_.get(), buffer_pool_manager_.get(), rm_manager_.get(),
                                                  ix_manager_.get());
        log_manager_ = std::make_unique<LogManager>(disk_manager_.get());
        lock_manager_ = std::make_unique<LockManager>();
        txn_manager_ = std::make_unique<TransactionManager>(lock_manager_.get(), sm_manager_.get());
        log_manager_->SetLogMode(false);
    }
};

/**
 * BasicTest系列只用于未处理死锁的lock_manager接口测试，如果加入了死锁预防，可以忽略BasicTest的结果，后续会对BasicTest进行处理
 * Deadlock_Prevention_Test用于测试死锁预防
 */

TEST_F(LockManagerTest, TransactionStateTest) {

}

// test shared lock on tuple under REPEATABLE_READ
TEST_F(LockManagerTest, BasicTest1_SHARED_TUPLE) {

}

// test exclusive lock on tuple under REPEATABLE_READ
TEST_F(LockManagerTest, BasicTest2_EXCLUSIVE_TUPLE) {

}

// test shared lock on table under REPEATABLE_READ
TEST_F(LockManagerTest, BasicTest3_SHARED_TABLE) {

}

// test exclusive lock on table under REPEATABLE_READ
TEST_F(LockManagerTest, BasicTest4_EXCLUSIVE_TABLE) {

}

// test intention shared lock on table under REPEATABLE_READ
TEST_F(LockManagerTest, BasicTest5_INTENTION_SHARED) {


}

// test intention exclusive lock on table under REPEATABLE_READ
TEST_F(LockManagerTest, BasicTest6_INTENTION_EXCLUSIVE) {

}

// test deadlock prevention
TEST_F(LockManagerTest, Deadlock_Prevetion_Test) {


}