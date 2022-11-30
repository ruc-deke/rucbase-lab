#include "execution/execution_manager.h"
#include "transaction_manager.h"
#include "gtest/gtest.h"
#include "interp.h"

#define BUFFER_LENGTH 8192

class TransactionTest : public::testing::Test {
public:
    std::string db_name_ = "Txn_Test_DB";
    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<BufferPoolManager> buffer_pool_manager_;
    std::unique_ptr<RmManager> rm_manager_;
    std::unique_ptr<IxManager> ix_manager_;
    std::unique_ptr<SmManager> sm_manager_;
    std::unique_ptr<QlManager> ql_manager_;
    std::unique_ptr<LogManager> log_manager_;
    std::unique_ptr<LockManager> lock_manager_;
    std::unique_ptr<TransactionManager> txn_manager_;
    std::unique_ptr<Interp> interp_;
    txn_id_t txn_id = INVALID_TXN_ID;
    char *result = new char[BUFFER_LENGTH];
    int offset;

public:
    // This function is called before every test.
    void SetUp() override {
        ::testing::Test::SetUp();
        // For each test, we create a new BufferPoolManager...
        disk_manager_ = std::make_unique<DiskManager>();
        buffer_pool_manager_ = std::make_unique<BufferPoolManager>(BUFFER_POOL_SIZE, disk_manager_.get());
        rm_manager_ = std::make_unique<RmManager>(disk_manager_.get(), buffer_pool_manager_.get());
        ix_manager_ = std::make_unique<IxManager>(disk_manager_.get(), buffer_pool_manager_.get());
        sm_manager_ = std::make_unique<SmManager>(disk_manager_.get(), buffer_pool_manager_.get(), rm_manager_.get(),
                                                  ix_manager_.get());
        ql_manager_ = std::make_unique<QlManager>(sm_manager_.get());
        log_manager_ = std::make_unique<LogManager>(disk_manager_.get());
        log_manager_->SetLogMode(false);
        lock_manager_ = std::make_unique<LockManager>();
        txn_manager_ = std::make_unique<TransactionManager>(lock_manager_.get(), sm_manager_.get());
        interp_ = std::make_unique<Interp>(sm_manager_.get(), ql_manager_.get(), txn_manager_.get());
        // create db and open db
        if (sm_manager_->is_dir(db_name_)) {
            sm_manager_->drop_db(db_name_);
        }
        sm_manager_->create_db(db_name_);
        sm_manager_->open_db(db_name_);
    }

    // This function is called after every test.
    void TearDown() override {
        sm_manager_->close_db();  // exit
        // sm_manager_->drop_db(db_name_);  // 若不删除数据库文件，则将保留最后一个测试点的数据库
    };

    // The below helper functions are useful for testing.
    void exec_sql(const std::string &sql) {
        YY_BUFFER_STATE yy_buffer = yy_scan_string(sql.c_str());
        assert(yyparse() == 0 && ast::parse_tree != nullptr);
        yy_delete_buffer(yy_buffer);
        memset(result, 0, BUFFER_LENGTH);
        offset = 0;
        Context *context = new Context(lock_manager_.get(), log_manager_.get(),
                                       nullptr, result, &offset);
        interp_->interp_sql(ast::parse_tree, &txn_id, context);  // 主要执行逻辑
    };
};

TEST_F(TransactionTest, BeginTest) {
    Transaction *txn = nullptr;
    txn = txn_manager_->Begin(txn, log_manager_.get());

    EXPECT_EQ(txn_manager_->txn_map.size(), 1);
    EXPECT_NE(txn, nullptr);
    EXPECT_EQ(txn->GetState(), TransactionState::DEFAULT);
}

// test commit
TEST_F(TransactionTest, CommitTest) {
    exec_sql("create table t1 (num int);");
    exec_sql("begin;");
    exec_sql("insert into t1 values(1);");
    exec_sql("insert into t1 values(2);");
    exec_sql("insert into t1 values(3);");
    exec_sql("update t1 set num = 4 where num = 1;");
    exec_sql("delete from t1 where num = 3;");
    exec_sql("commit;");
    exec_sql("select * from t1;");
    const char *str = "+------------------+\n"
        "|              num |\n"
        "+------------------+\n"
        "|                4 |\n"
        "|                2 |\n"
        "+------------------+\n"
        "Total record(s): 2\n";
    EXPECT_STREQ(result, str);
    // there should be 3 transactions
    EXPECT_EQ(txn_manager_->GetNextTxnId(), 3);
    Transaction *txn = txn_manager_->GetTransaction(1);
    EXPECT_EQ(txn->GetState(), TransactionState::COMMITTED);
}

// test abort
TEST_F(TransactionTest, AbortTest) {
    exec_sql("create table t1 (num int);");
    exec_sql("begin;");
    exec_sql("insert into t1 values(1);");
    exec_sql("insert into t1 values(2);");
    exec_sql("insert into t1 values(3);");
    exec_sql("update t1 set num = 4 where num = 1;");
    exec_sql("delete from t1 where num = 3;");
    exec_sql("abort;");
    exec_sql("select * from t1;");
    const char * str = "+------------------+\n"
        "|              num |\n"
        "+------------------+\n"
        "+------------------+\n"
        "Total record(s): 0\n";
    EXPECT_STREQ(result, str);
    EXPECT_EQ(txn_manager_->GetNextTxnId(), 3);
    Transaction *txn = txn_manager_->GetTransaction(1);
    EXPECT_EQ(txn->GetState(), TransactionState::ABORTED);
}

