#include "concurrency/lock_manager.h"
#include "transaction_manager.h"
#include "execution/execution_manager.h"
#include "interp.h"
#include "gtest/gtest.h"

#define BUFFER_LENGTH 8192
const std::string TEST_DB_NAME = "ConcurrencyTestDB";

enum class LockMode { SHARED, EXCLUSIVE, INTENTION_SHARED, INTENTION_EXCLUSIVE};
std::string lockModeStr[4] = {"shared", "exclusive", "intention shared", "intention exclusive"};
class LockOperation {
   public:
    LockMode lock_mode_;
    LockDataType data_type_;
    int table_;
    Rid rid_;
};
Rid default_rid{-1, -1};

enum class OperationMode { INSERT, DELETE, SELECT, UPDATE};
std::string operationStr[4] = {"insert", "delete", "select", "update"};

class ConcurrencyTest : public ::testing::Test {
   public:
    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<BufferPoolManager> buffer_pool_manager;
    std::unique_ptr<RmManager> rm_manager_;
    std::unique_ptr<SmManager> sm_manager_;
    std::unique_ptr<QlManager> ql_manager_;
    std::unique_ptr<IxManager> ix_manager_;
    std::unique_ptr<LockManager> lock_manager_;
    std::unique_ptr<LogManager> log_manager_;
    std::unique_ptr<TransactionManager> txn_manager_;
    std::unique_ptr<Interp> interp_;
    txn_id_t txn_id = INVALID_TXN_ID;
    char *result = new char[BUFFER_LENGTH];
    int offset;

   public:
    void SetUp() override {
        ::testing::Test::SetUp();
        disk_manager_ = std::make_unique<DiskManager>();
        buffer_pool_manager = std::make_unique<BufferPoolManager>(BUFFER_POOL_SIZE, disk_manager_.get());
        rm_manager_ = std::make_unique<RmManager>(disk_manager_.get(), buffer_pool_manager.get());
        ix_manager_ = std::make_unique<IxManager>(disk_manager_.get(), buffer_pool_manager.get());
        sm_manager_ = std::make_unique<SmManager>(disk_manager_.get(), buffer_pool_manager.get(), rm_manager_.get(),
                                                  ix_manager_.get());
        ql_manager_ = std::make_unique<QlManager>(sm_manager_.get());
        log_manager_ = std::make_unique<LogManager>(disk_manager_.get());
        log_manager_->SetLogMode(false);
        lock_manager_ = std::make_unique<LockManager>();
        txn_manager_ = std::make_unique<TransactionManager>(lock_manager_.get(), sm_manager_.get());
        interp_ = std::make_unique<Interp>(sm_manager_.get(), ql_manager_.get(), txn_manager_.get());
        if(sm_manager_->is_dir(TEST_DB_NAME)) {
            sm_manager_->drop_db(TEST_DB_NAME);
        }
        sm_manager_->create_db(TEST_DB_NAME);
        sm_manager_->open_db(TEST_DB_NAME);
    }

    void TearDown() override {
        sm_manager_->close_db();
    }

    void exec_sql(const std::string &sql) {
        std::cout<<sql<<std::endl;
        YY_BUFFER_STATE yy_buffer = yy_scan_string(sql.c_str());
        assert(yyparse() == 0 && ast::parse_tree != nullptr);
        yy_delete_buffer(yy_buffer);
        memset(result, 0, BUFFER_LENGTH);
        offset = 0;
        Context *context = new Context(lock_manager_.get(), log_manager_.get(),
                                       nullptr, result, &offset);
        interp_->interp_sql(ast::parse_tree, &txn_id, context);  // 主要执行逻辑
    }

    void RunLockOperation(Transaction *txn, const LockOperation &operation) {
        switch (operation.lock_mode_) {
            case LockMode::SHARED: {
                if(operation.data_type_ == LockDataType::RECORD) {
                    lock_manager_->LockSharedOnRecord(txn, operation.rid_, operation.table_);
                }
                else {
                    lock_manager_->LockSharedOnTable(txn, operation.table_);
                }
            } break;
            case LockMode::EXCLUSIVE: {
                if(operation.data_type_ == LockDataType::TABLE) {
                    lock_manager_->LockExclusiveOnRecord(txn, operation.rid_, operation.table_);
                }
                else {
                    lock_manager_->LockSharedOnTable(txn, operation.table_);
                }
            } break;
            case LockMode::INTENTION_SHARED: {
                lock_manager_->LockISOnTable(txn, operation.table_);
            } break;
            case LockMode::INTENTION_EXCLUSIVE: {
                lock_manager_->LockIXOnTable(txn, operation.table_);
            } break;
            default:
                break;
        }
    }
};

TEST_F(ConcurrencyTest, LockManager_Concurrency_Test) {

}

TEST_F(ConcurrencyTest, Transaction_Concurrency_Simple_Test) {

}

