#undef NDEBUG

#include <cassert>
#include <string>

#include "gtest/gtest.h"
#include "record/rm_manager.h"
#include "sm.h"
#define BUFFER_LENGTH 8192

// 测试SmManager的函数
TEST(SystemManagerTest, SimpleTest) {
    std::string db = "db";
    std::string tab1 = "tab1";
    std::string tab2 = "tab2";

    // 创建SmManager类的对象sm_manager
    auto disk_manager = std::make_unique<DiskManager>();
    auto buffer_pool_manager = std::make_unique<BufferPoolManager>(BUFFER_POOL_SIZE, disk_manager.get());
    auto rm_manager = std::make_unique<RmManager>(disk_manager.get(), buffer_pool_manager.get());
    auto ix_manager = std::make_unique<IxManager>(disk_manager.get(), buffer_pool_manager.get());
    auto sm_manager =
        std::make_unique<SmManager>(disk_manager.get(), buffer_pool_manager.get(), rm_manager.get(), ix_manager.get());
    char *result = new char[BUFFER_LENGTH];
    int offset = 0;
    Context *context = new Context(nullptr, nullptr, nullptr, result, &offset);

    if (sm_manager->is_dir(db)) {
        sm_manager->drop_db(db);
    }
    // Cannot use a database that does not exist
    try {
        sm_manager->open_db(db);
        assert(0);
    } catch (DatabaseNotFoundError &) {
    }
    // Create database
    sm_manager->create_db(db);
    // Cannot re-create database
    try {
        sm_manager->create_db(db);
        assert(0);
    } catch (DatabaseExistsError &) {
    }
    // Open database
    sm_manager->open_db(db);
    std::vector<ColDef> col_defs = {{.name = "a", .type = TYPE_INT, .len = 4},
                                    {.name = "b", .type = TYPE_FLOAT, .len = 4},
                                    {.name = "c", .type = TYPE_STRING, .len = 256}};
    // Create table 1
    sm_manager->create_table(tab1, col_defs, context);
    // Cannot re-create table
    try {
        sm_manager->create_table(tab1, col_defs, context);
        assert(0);
    } catch (TableExistsError &) {
    }
    // Create table 2
    sm_manager->create_table(tab2, col_defs, context);
    // Create index for table 1
    sm_manager->create_index(tab1, "a", context);
    sm_manager->create_index(tab1, "c", context);
    // Cannot re-create index
    try {
        sm_manager->create_index(tab1, "a", context);
        assert(0);
    } catch (IndexExistsError &) {
    }
    // Create index for table 2
    sm_manager->create_index(tab2, "b", context);
    // Drop index of table 1
    sm_manager->drop_index(tab1, "a", context);
    // Cannot drop index that does not exist
    try {
        sm_manager->drop_index(tab1, "b", context);
        assert(0);
    } catch (IndexNotFoundError &) {
    }
    // Drop index
    sm_manager->drop_table(tab1, context);
    // Cannot drop table that does not exist
    try {
        sm_manager->drop_table(tab1, context);
        assert(0);
    } catch (TableNotFoundError &) {
    }
    // Clean up
    sm_manager->close_db();
    sm_manager->drop_db(db);
}