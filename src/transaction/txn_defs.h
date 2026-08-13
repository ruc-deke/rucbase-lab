// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <string>
#include <utility>

#include "common/config.h"
#include "common/defs.h"
#include "record/rm_defs.h"

/* 标识事务状态 */
enum class TransactionState { DEFAULT, GROWING, SHRINKING, COMMITTED, ABORTED };

/* 系统的隔离级别，当前赛题中为可串行化隔离级别 */
enum class IsolationLevel { READ_UNCOMMITTED, REPEATABLE_READ, READ_COMMITTED, SERIALIZABLE };

/* 事务写操作类型，包括插入、删除、更新三种操作 */
enum class WType { INSERT_TUPLE = 0, DELETE_TUPLE, UPDATE_TUPLE };

/**
 * @brief 事务的写操作记录，用于事务的回滚
 * INSERT
 * --------------------------------
 * | wtype | tab_name | tuple_rid |
 * --------------------------------
 * DELETE / UPDATE
 * ----------------------------------------------
 * | wtype | tab_name | tuple_rid | tuple_value |
 * ----------------------------------------------
 */
class WriteRecord {
public:
    WriteRecord() = default;

    // 接收端「下沉」参数：按值接收 + 成员 std::move，调用方传临时量时只搬移一次。
    // constructor for insert operation
    WriteRecord(WType wtype, std::string tab_name, const Rid& rid)
        : wtype_(wtype),
          tab_name_(std::move(tab_name)),
          rid_(rid) {}

    // constructor for delete & update operation
    WriteRecord(WType wtype, std::string tab_name, const Rid& rid, RmRecord record)
        : wtype_(wtype),
          tab_name_(std::move(tab_name)),
          rid_(rid),
          record_(std::move(record)) {}

    ~WriteRecord() = default;

    [[nodiscard]] WType write_type() const noexcept { return wtype_; }
    [[nodiscard]] const std::string& table_name() const noexcept { return tab_name_; }
    [[nodiscard]] const Rid& rid() const noexcept { return rid_; }
    [[nodiscard]] const RmRecord& record() const noexcept { return record_; }

private:
    WType wtype_ = WType::INSERT_TUPLE;
    std::string tab_name_;
    Rid rid_{};
    RmRecord record_{};
};

/* 多粒度锁，加锁对象的类型，包括记录和表 */
enum class LockDataType { TABLE = 0, RECORD = 1 };

/**
 * @description: 加锁对象的唯一标识
 */
class LockDataId {
public:
    /* 表级锁 */
    LockDataId(int fd, LockDataType type) {
        assert(type == LockDataType::TABLE);
        fd_ = fd;
        type_ = type;
        rid_.page_no = -1;
        rid_.slot_no = -1;
    }

    /* 行级锁 */
    LockDataId(int fd, const Rid& rid, LockDataType type) {
        assert(type == LockDataType::RECORD);
        fd_ = fd;
        rid_ = rid;
        type_ = type;
    }

    /** @brief 将锁对象字段编码为无符号哈希输入，避免有符号移位。 */
    uint64_t hash_key() const noexcept {
        if (type_ == LockDataType::TABLE) {
            // fd_
            return static_cast<uint32_t>(fd_);
        } else {
            // fd_, rid_.page_no, rid.slot_no
            return (static_cast<uint64_t>(type_) << 63U) | (static_cast<uint64_t>(static_cast<uint32_t>(fd_)) << 31U) |
                   (static_cast<uint64_t>(static_cast<uint32_t>(rid_.page_no)) << 16U) |
                   static_cast<uint32_t>(rid_.slot_no);
        }
    }

    bool operator==(const LockDataId& other) const noexcept {
        if (type_ != other.type_) return false;
        if (fd_ != other.fd_) return false;
        return rid_ == other.rid_;
    }
    int fd_;
    Rid rid_;
    LockDataType type_;
};

template <>
struct std::hash<LockDataId> {
    size_t operator()(const LockDataId& obj) const noexcept { return std::hash<uint64_t>{}(obj.hash_key()); }
};

/* 事务回滚原因 */
enum class AbortReason { LOCK_ON_SHIRINKING = 0, UPGRADE_CONFLICT, DEADLOCK_PREVENTION };

/* 事务回滚异常，在rmdb.cpp中进行处理 */
class TransactionAbortException : public std::exception {
    txn_id_t txn_id_;
    AbortReason abort_reason_;

public:
    explicit TransactionAbortException(txn_id_t txn_id, AbortReason abort_reason)
        : txn_id_(txn_id),
          abort_reason_(abort_reason) {}

    txn_id_t get_transaction_id() const noexcept { return txn_id_; }
    [[nodiscard]] AbortReason abort_reason() const noexcept { return abort_reason_; }
    [[nodiscard]] std::string info() const {
        switch (abort_reason_) {
            case AbortReason::LOCK_ON_SHIRINKING: {
                return "Transaction " + std::to_string(txn_id_) +
                       " aborted because it cannot request locks on SHRINKING phase\n";
            } break;

            case AbortReason::UPGRADE_CONFLICT: {
                return "Transaction " + std::to_string(txn_id_) +
                       " aborted because another transaction is waiting for upgrading\n";
            } break;

            case AbortReason::DEADLOCK_PREVENTION: {
                return "Transaction " + std::to_string(txn_id_) + " aborted for deadlock prevention\n";
            } break;

            default: {
                return "Transaction aborted\n";
            } break;
        }
    }
};
