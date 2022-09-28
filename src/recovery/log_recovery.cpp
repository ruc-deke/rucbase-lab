#include "log_recovery.h"
#include "record/rm.h"
#include "system/sm_manager.h"

/**
 * 重做未刷入磁盘的写操作
 * 只需要考虑DML操作，暂时不需要考虑DDL操作
 */
void LogRecovery::Redo() {
    // Todo:
    // 1. 从磁盘的日志文件中顺序读取日志记录
    // 2. 根据日志对应操作的类型，执行相应的操作
    // 2.1 如果是事务相关操作，则需要维护事务活动列表active_txns_
    // 2.2 如果是写操作，需要比较该日志的日志序列号和对应数据页的page_lsn_，判断是否要执行写操作

}

/**
 * 撤销未完成事务的写操作
 * 只需要考虑DML操作，暂时不需要考虑DDL操作
 */
void LogRecovery::Undo() {
    // Todo:
    // 1. 遍历事务活动列表active_txns_获取所有未完成事务
    // 2. 根据日志中的prev_lsn_信息遍历该事务已经执行的所有写操作
    // 3. 撤销该事务的所有写操作

}