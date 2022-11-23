#undef NDEBUG

#include "execution.h"
#include "gtest/gtest.h"
#include "interp_test.h"

#define BUFFER_LENGTH 8192

std::string db_name_ = "ExecutorTest_db";
std::unique_ptr<DiskManager> disk_manager_ = std::make_unique<DiskManager>();
std::unique_ptr<BufferPoolManager> buffer_pool_manager_ =
    std::make_unique<BufferPoolManager>(BUFFER_POOL_SIZE, disk_manager_.get());
std::unique_ptr<RmManager> rm_manager_ = std::make_unique<RmManager>(disk_manager_.get(), buffer_pool_manager_.get());
std::unique_ptr<IxManager> ix_manager_ = std::make_unique<IxManager>(disk_manager_.get(), buffer_pool_manager_.get());
std::unique_ptr<SmManager> sm_manager_ =
    std::make_unique<SmManager>(disk_manager_.get(), buffer_pool_manager_.get(), rm_manager_.get(), ix_manager_.get());

std::unique_ptr<QlManager> ql_manager_ = std::make_unique<QlManager>(sm_manager_.get());

std::unique_ptr<InterpForTest> interp_ = std::make_unique<InterpForTest>(sm_manager_.get(), ql_manager_.get());

char *result = new char[BUFFER_LENGTH];
int offset;

char *exec_sql(const std::string &sql) {
    std::cout << "rucbase> " + sql << std::endl;
    YY_BUFFER_STATE yy_buffer = yy_scan_string(sql.c_str());
    assert(yyparse() == 0 && ast::parse_tree != nullptr);
    yy_delete_buffer(yy_buffer);
    memset(result, 0, BUFFER_LENGTH);
    offset = 0;
    Context *context = new Context(nullptr, nullptr, new Transaction(0), result, &offset);
    interp_->interp_sql(ast::parse_tree, context);  // 主要执行逻辑
    // std::cout << result << std::endl;
    return result;
};

int main(int argc, char *argv[]) {
    if (argc == 2) {
        if (!sm_manager_->is_dir(db_name_)) {
            sm_manager_->create_db(db_name_);
        }
        sm_manager_->open_db(db_name_);
        std::cout << exec_sql(argv[1]) << std::endl;
    }
    sm_manager_->close_db();
}
