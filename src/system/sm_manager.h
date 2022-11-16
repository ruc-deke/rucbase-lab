#pragma once

#include "index/ix.h"
// #include "record/rm.h"
#include "common/context.h"
#include "record/rm_file_handle.h"
#include "sm_defs.h"
#include "sm_meta.h"

class Context;

struct ColDef {
    std::string name;  // Column name
    ColType type;      // Type of column
    int len;           // Length of column
};

// SmManager类似于CMU中的Catalog
// 管理数据库中db, table, index的元数据，支持create/drop/open/close等操作
// 每个SmManager对应一个db
class SmManager {
   public:
    DbMeta db_;  // create_db时将会将DbMeta写入文件，open_db时将会从文件中读出DbMeta
    std::unordered_map<std::string, std::unique_ptr<RmFileHandle>> fhs_;   // file name -> record file handle
    std::unordered_map<std::string, std::unique_ptr<IxIndexHandle>> ihs_;  // file name -> index file handle
   private:
    DiskManager *disk_manager_;
    BufferPoolManager *buffer_pool_manager_;
    RmManager *rm_manager_;
    IxManager *ix_manager_;
    // TODO: 全部改成私有变量，并且改成指针形式
    // DbMeta *db_;
    // std::map<std::string, std::unique_ptr<RmFileHandle>> *fhs_;
    // std::map<std::string, std::unique_ptr<IxIndexHandle>> *ihs_;

   public:
    SmManager(DiskManager *disk_manager, BufferPoolManager *buffer_pool_manager, RmManager *rm_manager,
              IxManager *ix_manager)
        : disk_manager_(disk_manager),
          buffer_pool_manager_(buffer_pool_manager),
          rm_manager_(rm_manager),
          ix_manager_(ix_manager) {
        // db_ = new DbMeta();
    }

    ~SmManager() {
        // delete db_;
    }

    // TODO: Get private variables （注意，这里的get方法都必须返回指针，否则上层调用会出问题）
    // DbMeta *get_db() { return db_; }

    // std::map<std::string, std::unique_ptr<RmFileHandle>> *get_fhs() { return fhs_; }

    // std::map<std::string, std::unique_ptr<IxIndexHandle>> *get_ihs() { return ihs_; }

    RmManager *get_rm_manager() { return rm_manager_; }  // called in some excutors to modify record

    IxManager *get_ix_manager() { return ix_manager_; }  // called in some excutors to modify index

    BufferPoolManager *get_bpm() { return buffer_pool_manager_; }

    // Database management
    bool is_dir(const std::string &db_name);

    void create_db(const std::string &db_name);

    void drop_db(const std::string &db_name);

    void open_db(const std::string &db_name);

    void close_db();

    // Table management
    void show_tables(Context *context);

    void desc_table(const std::string &tab_name, Context *context);

    void create_table(const std::string &tab_name, const std::vector<ColDef> &col_defs, Context *context);

    void drop_table(const std::string &tab_name, Context *context);

    void apply_drop_table(const std::string &tab_name, Context *context);

    // Index management
    void create_index(const std::string &tab_name, const std::string &col_name, Context *context);

    void drop_index(const std::string &tab_name, const std::string &col_name, Context *context);

    void apply_drop_index(const std::string &tab_name, const std::string &col_name, Context *context);

    // Transaction rollback management
    /**
     * @brief rollback the insert operation
     *
     * @param tab_name the name of the table
     * @param rid the rid of the record
     * @param txn the transaction
     */
    void rollback_insert(const std::string &tab_name, const Rid &rid, Context *context);

    /**
     * @brief rollback the delete operation
     *
     * @param tab_name the name of the table
     * @param rid the value of the deleted record
     * @param txn the transaction
     */
    void rollback_delete(const std::string &tab_name, const RmRecord &record, Context *context);

    /**
     * @brief rollback the update operation
     *
     * @param tab_name the name of the table
     * @param rid the rid of the record
     * @param record the old value of the record
     * @param txn the transaction
     */
    void rollback_update(const std::string &tab_name, const Rid &rid, const RmRecord &record, Context *context);

    /**
     * @brief rollback the table drop operation
     *
     * @param tab_name the name of the deleted table
     */
    void rollback_drop_table(const std::string &tab_name, Context *context);

    /**
     * @brief rollback the table create operation
     *
     * @param tab_name the name of the created table
     */
    void rollback_create_table(const std::string &tab_name, Context *context);

    /**
     * @brief rollback the create index operation
     *
     * @param tab_name the name of the table
     * @param col_name the name of the column on which index is created
     */
    void rollback_create_index(const std::string &tab_name, const std::string &col_name, Context *context);

    /**
     * @brief rollback the drop index operation
     *
     * @param tab_name the name of the table
     * @param col_name the name of the column on which index is created
     */
    void rollback_drop_index(const std::string &tab_name, const std::string &col_name, Context *context);
};
