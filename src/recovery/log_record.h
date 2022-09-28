#pragma once

#include "system/sm_meta.h"
#include "log_defs.h"
#include "record/rm_defs.h"

enum class LogRecordType { INVALID = 0, CREATE_TABLE, MARK_DROP_TABLE, APPLY_DROP_TABLE, 
                            CREATE_INDEX, MARK_DROP_INDEX, APPLY_DROP_INDEX,
                            INSERT, UPDATE, DELETE, BEGIN, COMMIT, ABORT, NEW_PAGE};

static std::string log_record_type[15] = {"INVALID", "CREATE_TABLE", "MARK_DROP_TABLE","APPLY_DROP_TABLE",
                                   "CREATE_INDEX", "MARK_DROP_INDEX", "APPLY_DROP_INDEX",
                                   "INSERT", "UPDATE", "DELETE", "BEGIN", "COMMIT", "ABORT", "NEW_PAGE"};

/**
 * @brief for every write operation, you should write ahead a corresponding log record
 * 
 * LOG_HEADER
 * ---------------------------------------------
 * | size | lsn | txn_id | prev_lsn | log_type |
 * ---------------------------------------------
 * transaction(begin/abort/commit)
 * --------------
 * | LOG_HEADER |
 * --------------
 * create_table (the db_meta has not been flushed into the disk)
 * ---------------------------
 * | LOG_HEADER | table_meta |(table_name_size, table_name, col_num, col_num*(col_type, len, offset, index))
 * ---------------------------
 * mark_drop_table (the table file is not deleted)
 * ---------------------------
 * | LOG_HEADER | table_meta |
 * ---------------------------
 * apply_drop_table (the table file has been deleted)
 * -------------------------
 * | LOG_HEADER | tab_name |
 * -------------------------
 * create_index (TODO: to be supplemented)
 * ------------------------------------
 * | LOG_HEADER | tab_name | col_name |
 * ------------------------------------
 * mark_drop_index (TODO: to be supplemented)
 * ------------------------------------
 * | LOG_HEADER | tab_name | col_name |
 * ------------------------------------
 * apply_drop_index (TODO: to be supplemented)
 * ------------------------------------
 * | LOG_HEADER | tab_name | col_name |
 * ------------------------------------
 * insert
 * -----------------------------------------------------------------------------------
 * | LOG_HEADER | tuple_rid | tuple_size | tuple_data | table_name_size | table_name |
 * -----------------------------------------------------------------------------------
 * delete
 * -----------------------------------------------------------------------------------
 * | LOG_HEADER | tuple_rid | tuple_size | tuple_data | table_name_size | table_name |
 * -----------------------------------------------------------------------------------
 * update
 * -----------------------------------------------------------------------------------------------------
 * | LOG_HEADER | tuple_rid | old_size | old_data | new_size | new_data | table_name_size | table_name |
 * -----------------------------------------------------------------------------------------------------
 * new_page
 * -------------------------------------------------------
 * | LOG_HEADER | page_no | table_name_size | table_name |
 * -------------------------------------------------------
 */

class LogRecord {

    friend class LogManager;
    friend class LogRecovery;

public:
    LogRecord() = default;

    // constructor for transaction_operation (begin/commit/abort)
    LogRecord(txn_id_t txn_id, lsn_t prev_lsn, LogRecordType log_type)
        : size_(HEADER_SIZE), txn_id_(txn_id), prev_lsn_(prev_lsn), log_type_(log_type) {
            assert(log_type == LogRecordType::BEGIN || log_type == LogRecordType::COMMIT || log_type == LogRecordType::ABORT);
        }

    LogRecord(txn_id_t txn_id, lsn_t prev_lsn, LogRecordType log_type, const std::string &table_name)
            : txn_id_(txn_id), prev_lsn_(prev_lsn), log_type_(log_type){
            tab_name_size_ = table_name.size();
            tab_name_ = new char[tab_name_size_];
            memcpy(tab_name_, table_name.c_str(),  tab_name_size_);
            size_ = HEADER_SIZE + sizeof(int) + tab_name_size_;
    }

    // constructor for update operation
    LogRecord(txn_id_t txn_id, lsn_t prev_lsn, LogRecordType log_type, const Rid &rid,
            const RmRecord &old_tuple, const RmRecord &new_tuple, const std::string &table_name) 
        : txn_id_(txn_id), prev_lsn_(prev_lsn), log_type_(log_type), 
        update_rid_(rid), old_tuple_(old_tuple), new_tuple_(new_tuple) {
            tab_name_size_ = table_name.size();
            tab_name_ = new char[tab_name_size_];
            memcpy(tab_name_, table_name.c_str(), tab_name_size_);
            size_ = HEADER_SIZE + sizeof(Rid) + sizeof(int) * 3 + old_tuple_.size + new_tuple_.size + tab_name_size_;
        }

    // constructor for insert / delete operation
    LogRecord(txn_id_t txn_id, lsn_t prev_lsn, LogRecordType log_type, const Rid &rid, 
            const RmRecord &record, const std::string &table_name)
        : txn_id_(txn_id), prev_lsn_(prev_lsn), log_type_(log_type) {
            if(log_type == LogRecordType::INSERT) {
                insert_rid_ = rid;
                insert_tuple_ = record;
            }
            else {
                assert(log_type == LogRecordType::DELETE);
                delete_rid_ = rid;
                delete_tuple_ = record;
            }
            tab_name_size_ = table_name.size();
            tab_name_ = new char[tab_name_size_];
            memcpy(tab_name_, table_name.c_str(), tab_name_size_);
            size_ = HEADER_SIZE + sizeof(Rid) + sizeof(int) * 2 + record.size + tab_name_size_;
        }

    // constructor for new_page operation
    LogRecord(txn_id_t txn_id, lsn_t prev_lsn, LogRecordType log_type, int page_no, const std::string &table_name)
        :txn_id_(txn_id), prev_lsn_(prev_lsn), log_type_(log_type), new_page_no_(page_no) {
            tab_name_size_ = table_name.size();
            tab_name_ = new char[tab_name_size_];
            memcpy(tab_name_, table_name.c_str(), tab_name_size_);
            size_ = HEADER_SIZE + sizeof(int) * 2 + tab_name_size_;
        }

    ~LogRecord() = default;

    inline Rid &GetInsertRid() { return insert_rid_; }

    inline Rid &GetDeleteRid() { return delete_rid_; }

    inline Rid &GetUpdateRid() { return update_rid_; }

    inline lsn_t GetLsn() { return lsn_; }

    inline lsn_t GetPrevLsn() { return prev_lsn_; }

    inline txn_id_t GetTxnId() { return txn_id_; }

    inline int32_t GetSize() { return size_; }

    inline LogRecordType &GetLogRecordType() { return log_type_; }

    inline RmRecord &GetInsertRecord() { return insert_tuple_; }

    inline RmRecord &GetDeleteRecord() { return delete_tuple_; }

    inline RmRecord &GetOldRecord() { return old_tuple_; }

    inline RmRecord &GetNewRecord() { return new_tuple_; }

    inline TabMeta &GetTabMeta() { return tab_meta_; }

    inline std::string GetTableName() {
        std::cout<<tab_name_size_<<std::endl;
        std::string str(tab_name_, tab_name_ + tab_name_size_);
        return str;
    }

    inline void DeserializeTableName(const char * data) {
        tab_name_size_ = *reinterpret_cast<const int *>(data);
        tab_name_ = new char[tab_name_size_];
        memcpy(tab_name_, data + sizeof(int), tab_name_size_);
    }

    inline void SerializeTableName(char *log_buffer, int &pos) {
        std::memcpy(log_buffer + pos, &tab_name_size_, sizeof(int));
        pos += sizeof(int);
        std::memcpy(log_buffer + pos, tab_name_, tab_name_size_);
        pos += sizeof(tab_name_size_);
    }

    // used for debug
    inline void PrintLogRecord() {
        std::cout << "lsn: " << lsn_
                    << ", log type: " << log_record_type[static_cast<int>(log_type_)]
                    << ", txn id: " << txn_id_;
        std::cout << std::endl;
    }

private:
    int32_t size_{0};
    lsn_t lsn_{INVALID_LSN};
    txn_id_t txn_id_{INVALID_TXN_ID};
    lsn_t prev_lsn_{INVALID_LSN};
    LogRecordType log_type_{LogRecordType::INVALID};
    int tab_name_size_;
    char *tab_name_;
    TabMeta tab_meta_;

    // update
    Rid update_rid_;
    RmRecord old_tuple_;
    RmRecord new_tuple_;

    // insert 
    Rid insert_rid_;
    RmRecord insert_tuple_;

    // delete
    Rid delete_rid_;
    RmRecord delete_tuple_;

    // new_page
    int new_page_no_;

    static constexpr int HEADER_SIZE = 20;
};