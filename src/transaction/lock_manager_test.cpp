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
    Rid rid{0, 0};
    int tab_fd = 0;
    LockDataId lock_data_id(tab_fd, rid, LockDataType::RECORD);

    printf("before\n");
    std::thread t0([&] {
        printf("t0\n");
        Transaction txn0(0);
        bool res = lock_manager_->LockSharedOnRecord(&txn0, rid, tab_fd);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.GetState(), TransactionState::GROWING);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        lock_manager_->Unlock(&txn0, lock_data_id);
        EXPECT_EQ(txn0.GetState(), TransactionState::SHRINKING);
    });

    std::thread t1([&] {
        printf("t1\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        Transaction txn1(0);
        bool res = lock_manager_->LockSharedOnRecord(&txn1, rid, tab_fd);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn1.GetState(), TransactionState::GROWING);
        lock_manager_->Unlock(&txn1, lock_data_id);
        EXPECT_EQ(txn1.GetState(), TransactionState::SHRINKING);
    });

    t0.join();
    t1.join();
}

// test shared lock on tuple under REPEATABLE_READ
TEST_F(LockManagerTest, BasicTest1_SHARED_TUPLE) {
    std::vector<Rid> rids;
    std::vector<Transaction *> txns;
    int num = 10;

    for(int i = 0; i < num; ++i) {
        Rid rid{i, i};
        rids.push_back(rid);
        txns.push_back(txn_manager_->Begin(nullptr, log_manager_.get()));
        EXPECT_EQ(i, txns[i]->GetTransactionId());
    }

    auto task = [&](int txn_id) {
        bool res;
        for(const Rid &rid : rids) {
            res = lock_manager_->LockSharedOnRecord(txns[txn_id], rid, txn_id);
            EXPECT_TRUE(res);
            EXPECT_EQ(txns[txn_id]->GetState(), TransactionState::GROWING);
        }

        for(const Rid &rid : rids) {
            res = lock_manager_->Unlock(txns[txn_id], LockDataId(txn_id, rid, LockDataType::RECORD));
            EXPECT_TRUE(res);
            EXPECT_EQ(txns[txn_id]->GetState(), TransactionState::SHRINKING);
        }

        txn_manager_->Commit(txns[txn_id], log_manager_.get());
        EXPECT_EQ(txns[txn_id]->GetState(), TransactionState::COMMITTED);
    };

    std::vector<std::thread> threads;
    threads.reserve(num);

    for(int i = 0; i < num; ++i) {
        threads.emplace_back(std::thread{task, i});
    }

    for(int i = 0; i < num; ++i) {
        threads[i].join();
    }

    for(int i = 0; i < num; ++i) {
        delete txns[i];
    }
}

// test exclusive lock on tuple under REPEATABLE_READ
TEST_F(LockManagerTest, BasicTest2_EXCLUSIVE_TUPLE) {
    std::vector<Rid> rids;
    std::vector<Transaction *> txns;
    int num = 10;

    for(int i = 0; i < num; ++i) {
        Rid rid{i, i};
        rids.push_back(rid);
        txns.push_back(txn_manager_->Begin(nullptr, log_manager_.get()));
        EXPECT_EQ(i, txns[i]->GetTransactionId());
    }

    auto task = [&](int txn_id) {
        bool res;
        for(const Rid &rid : rids) {
            res = lock_manager_->LockExclusiveOnRecord(txns[txn_id], rid, txn_id);
            EXPECT_TRUE(res);
            EXPECT_EQ(txns[txn_id]->GetState(), TransactionState::GROWING);
        }

        for(const Rid &rid : rids) {
            res = lock_manager_->Unlock(txns[txn_id], LockDataId(txn_id, rid, LockDataType::RECORD));
            EXPECT_TRUE(res);
            EXPECT_EQ(txns[txn_id]->GetState(), TransactionState::SHRINKING);
        }

        txn_manager_->Commit(txns[txn_id], log_manager_.get());
        EXPECT_EQ(txns[txn_id]->GetState(), TransactionState::COMMITTED);
    };

    std::vector<std::thread> threads;
    threads.reserve(num);

    for(int i = 0; i < num; ++i) {
        threads.emplace_back(std::thread{task, i});
    }

    for(int i = 0; i < num; ++i) {
        threads[i].join();
    }

    for(int i = 0; i < num; ++i) {
        delete txns[i];
    }
}

// test shared lock on table under REPEATABLE_READ
TEST_F(LockManagerTest, BasicTest3_SHARED_TABLE) {
    std::vector<int> tab_fds;
    std::vector<Transaction *> txns;
    int num = 10;

    for(int i = 0; i < num; ++i) {
        tab_fds.push_back(i);
        txns.push_back(txn_manager_->Begin(nullptr, log_manager_.get()));
        EXPECT_EQ(i, txns[i]->GetTransactionId());
    }

    auto task = [&](int txn_id) {
        bool res;
        for(const int tab_fd : tab_fds) {
            res = lock_manager_->LockSharedOnTable(txns[txn_id], tab_fd);
            EXPECT_TRUE(res);
            EXPECT_EQ(txns[txn_id]->GetState(), TransactionState::GROWING);
        }

        for(const int tab_fd : tab_fds) {
            res = lock_manager_->Unlock(txns[txn_id], LockDataId(tab_fd, LockDataType::TABLE));
            EXPECT_TRUE(res);
            EXPECT_EQ(txns[txn_id]->GetState(), TransactionState::SHRINKING);
        }

        txn_manager_->Commit(txns[txn_id], log_manager_.get());
        EXPECT_EQ(txns[txn_id]->GetState(), TransactionState::COMMITTED);
    };

    std::vector<std::thread> threads;
    threads.reserve(num);

    for(int i = 0; i < num; ++i) {
        threads.emplace_back(std::thread{task, i});
    }

    for(int i = 0; i < num; ++i) {
        threads[i].join();
    }

    for(int i = 0; i < num; ++i) {
        delete txns[i];
    }
}

// test exclusive lock on table under REPEATABLE_READ
TEST_F(LockManagerTest, BasicTest4_EXCLUSIVE_TABLE) {
    std::vector<int> tab_fds;
    std::vector<Transaction *> txns;
    int num = 10;

    for(int i = 0; i < num; ++i) {
        tab_fds.push_back(i);
        txns.push_back(txn_manager_->Begin(nullptr, log_manager_.get()));
        EXPECT_EQ(i, txns[i]->GetTransactionId());
    }

    auto task = [&](int txn_id) {
        bool res;
        for(const int tab_fd : tab_fds) {
            res = lock_manager_->LockExclusiveOnTable(txns[txn_id], tab_fd);
            EXPECT_TRUE(res);
            EXPECT_EQ(txns[txn_id]->GetState(), TransactionState::GROWING);
        }

        for(const int tab_fd : tab_fds) {
            res = lock_manager_->Unlock(txns[txn_id], LockDataId(tab_fd, LockDataType::TABLE));
            EXPECT_TRUE(res);
            EXPECT_EQ(txns[txn_id]->GetState(), TransactionState::SHRINKING);
        }

        txn_manager_->Commit(txns[txn_id], log_manager_.get());
        EXPECT_EQ(txns[txn_id]->GetState(), TransactionState::COMMITTED);
    };

    std::vector<std::thread> threads;
    threads.reserve(num);

    for(int i = 0; i < num; ++i) {
        threads.emplace_back(std::thread{task, i});
    }

    for(int i = 0; i < num; ++i) {
        threads[i].join();
    }

    for(int i = 0; i < num; ++i) {
        delete txns[i];
    }
}

// test intention shared lock on table under REPEATABLE_READ
TEST_F(LockManagerTest, BasicTest5_INTENTION_SHARED) {
    // txnA -> table1.tuple{1,1} shared
    // txnB -> table1 exclusive
    Rid rid{0, 0};
    int tab_fd = 0;
    LockDataId tuple1(tab_fd, rid, LockDataType::RECORD);
    LockDataId table1(tab_fd, LockDataType::TABLE);

    std::vector<int> operation;

    std::thread t0([&] {
        Transaction txn0(0);
        bool res = lock_manager_->LockISOnTable(&txn0, tab_fd);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.GetState(), TransactionState::GROWING);
        res = lock_manager_->LockSharedOnRecord(&txn0, rid, tab_fd);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.GetState(), TransactionState::GROWING);

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        operation.push_back(0);
        res = lock_manager_->Unlock(&txn0, tuple1);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.GetState(), TransactionState::SHRINKING);
        res = lock_manager_->Unlock(&txn0, table1);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.GetState(), TransactionState::SHRINKING);
    });

    std::thread t1([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds (500));

       Transaction txn1(1);
       bool res = lock_manager_->LockExclusiveOnTable(&txn1, tab_fd);
       operation.push_back(1);
       EXPECT_EQ(res, true);
       EXPECT_EQ(txn1.GetState(), TransactionState::GROWING);

       res = lock_manager_->Unlock(&txn1, table1);
       EXPECT_EQ(res, true);
       EXPECT_EQ(txn1.GetState(), TransactionState::SHRINKING);
    });

    t0.join();
    t1.join();

    // 如果txn1加锁没有被阻塞，那么一定是先执行operation.push_back(1)，反之，先执行operation.push_back(0)
    std::vector<int> operation_expected;
    operation_expected.push_back(0);
    operation_expected.push_back(1);
    EXPECT_EQ(operation_expected, operation);

}

// test intention exclusive lock on table under REPEATABLE_READ
TEST_F(LockManagerTest, BasicTest6_INTENTION_EXCLUSIVE) {
    // txnA -> table1.tuple{1,1} exclusive
    // txnB -> table1 shared
    Rid rid{0, 0};
    int tab_fd = 0;
    LockDataId tuple1(tab_fd, rid, LockDataType::RECORD);
    LockDataId table1(tab_fd, LockDataType::TABLE);

    std::vector<int> operation;

    std::thread t0([&] {
        Transaction txn0(0);
        bool res = lock_manager_->LockIXOnTable(&txn0, tab_fd);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.GetState(), TransactionState::GROWING);
        res = lock_manager_->LockExclusiveOnRecord(&txn0, rid, tab_fd);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.GetState(), TransactionState::GROWING);

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        operation.push_back(0);
        res = lock_manager_->Unlock(&txn0, tuple1);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.GetState(), TransactionState::SHRINKING);
        res = lock_manager_->Unlock(&txn0, table1);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn0.GetState(), TransactionState::SHRINKING);
    });

    std::thread t1([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds (500));

        Transaction txn1(1);
        bool res = lock_manager_->LockSharedOnTable(&txn1, tab_fd);
        operation.push_back(1);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn1.GetState(), TransactionState::GROWING);

        res = lock_manager_->Unlock(&txn1, table1);
        EXPECT_EQ(res, true);
        EXPECT_EQ(txn1.GetState(), TransactionState::SHRINKING);
    });

    t0.join();
    t1.join();

    // 如果txn1加锁没有被阻塞，那么一定是先执行operation.push_back(1)，反之，先执行operation.push_back(0)
    std::vector<int> operation_expected;
    operation_expected.push_back(0);
    operation_expected.push_back(1);
    EXPECT_EQ(operation_expected, operation);
}

// test deadlock prevention
//TEST_F(LockManagerTest, Deadlock_Prevetion_Test) {
//    // txn1 -> table0.tuple{0,0} exclusive
//    // txn2 -> table0.tuple{1,1} exclusive
//    // txn1 -> table0.tuple{1,1} exclusive
//    // txn2 -> table1.tuple{0,0} exclusive
//
//    int table0 = 0;
//    Rid rid0{0, 0};
//    Rid rid1{1, 1};
//    Transaction txn0(0);
//    Transaction txn1(1);
//
//    std::thread t0([&] {
//        bool res;
//        res = lock_manager_->LockExclusiveOnRecord(&txn0, rid0, table0);
//        EXPECT_EQ(res, true);
//        EXPECT_EQ(txn0.GetState(), TransactionState::GROWING);
//
//        std::this_thread::sleep_for(std::chrono::milliseconds(200));
//        try {
//            res = lock_manager_->LockExclusiveOnRecord(&txn0, rid1, table0);
//        } catch (TransactionAbortException e) {
//            txn_manager_->Abort(&txn0, log_manager_.get());
//        }
//    });
//
//    std::thread t1([&] {
//        std::this_thread::sleep_for(std::chrono::milliseconds(100));
//        bool res;
//        res = lock_manager_->LockExclusiveOnRecord(&txn1, rid1, table0);
//        EXPECT_EQ(res, true);
//        EXPECT_EQ(txn1.GetState(), TransactionState::GROWING);
//
//        std::this_thread::sleep_for(std::chrono::milliseconds(200));
//        try {
//            res = lock_manager_->LockExclusiveOnRecord(&txn1, rid0, table0);
//        } catch (TransactionAbortException e) {
//            txn_manager_->Abort(&txn1, log_manager_.get());
//        }
//    });
//
//    t0.join();
//    t1.join();
//
//}