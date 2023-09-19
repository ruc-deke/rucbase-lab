/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#pragma once

#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

class RMDBError : public std::exception {
   public:
    RMDBError() : _msg("Error: ") {}

    RMDBError(const std::string &msg) : _msg("Error: " + msg) {}

    const char *what() const noexcept override { return _msg.c_str(); }

    int get_msg_len() { return _msg.length(); }

    std::string _msg;
};

class InternalError : public RMDBError {
   public:
    InternalError(const std::string &msg) : RMDBError(msg) {}
};

// PF errors
class UnixError : public RMDBError {
   public:
    UnixError() : RMDBError(strerror(errno)) {}
};

class FileNotOpenError : public RMDBError {
   public:
    FileNotOpenError(int fd) : RMDBError("Invalid file descriptor: " + std::to_string(fd)) {}
};

class FileNotClosedError : public RMDBError {
   public:
    FileNotClosedError(const std::string &filename) : RMDBError("File is opened: " + filename) {}
};

class FileExistsError : public RMDBError {
   public:
    FileExistsError(const std::string &filename) : RMDBError("File already exists: " + filename) {}
};

class FileNotFoundError : public RMDBError {
   public:
    FileNotFoundError(const std::string &filename) : RMDBError("File not found: " + filename) {}
};

// RM errors
class RecordNotFoundError : public RMDBError {
   public:
    RecordNotFoundError(int page_no, int slot_no)
        : RMDBError("Record not found: (" + std::to_string(page_no) + "," + std::to_string(slot_no) + ")") {}
};

class InvalidRecordSizeError : public RMDBError {
   public:
    InvalidRecordSizeError(int record_size) : RMDBError("Invalid record size: " + std::to_string(record_size)) {}
};

// IX errors
class InvalidColLengthError : public RMDBError {
   public:
    InvalidColLengthError(int col_len) : RMDBError("Invalid column length: " + std::to_string(col_len)) {}
};

class IndexEntryNotFoundError : public RMDBError {
   public:
    IndexEntryNotFoundError() : RMDBError("Index entry not found") {}
};

// SM errors
class DatabaseNotFoundError : public RMDBError {
   public:
    DatabaseNotFoundError(const std::string &db_name) : RMDBError("Database not found: " + db_name) {}
};

class DatabaseExistsError : public RMDBError {
   public:
    DatabaseExistsError(const std::string &db_name) : RMDBError("Database already exists: " + db_name) {}
};

class TableNotFoundError : public RMDBError {
   public:
    TableNotFoundError(const std::string &tab_name) : RMDBError("Table not found: " + tab_name) {}
};

class TableExistsError : public RMDBError {
   public:
    TableExistsError(const std::string &tab_name) : RMDBError("Table already exists: " + tab_name) {}
};

class ColumnNotFoundError : public RMDBError {
   public:
    ColumnNotFoundError(const std::string &col_name) : RMDBError("Column not found: " + col_name) {}
};

class IndexNotFoundError : public RMDBError {
   public:
    IndexNotFoundError(const std::string &tab_name, const std::vector<std::string> &col_names) {
        _msg += "Index not found: " + tab_name + ".(";
        for(size_t i = 0; i < col_names.size(); ++i) {
            if(i > 0) _msg += ", ";
            _msg += col_names[i];
        }
        _msg += ")";
    }
};

class IndexExistsError : public RMDBError {
   public:
    IndexExistsError(const std::string &tab_name, const std::vector<std::string> &col_names) {
        _msg += "Index already exists: " + tab_name + ".(";
        for(size_t i = 0; i < col_names.size(); ++i) {
            if(i > 0) _msg += ", ";
            _msg += col_names[i];
        }
        _msg += ")";
    }
};

// QL errors
class InvalidValueCountError : public RMDBError {
   public:
    InvalidValueCountError() : RMDBError("Invalid value count") {}
};

class StringOverflowError : public RMDBError {
   public:
    StringOverflowError() : RMDBError("String is too long") {}
};

class IncompatibleTypeError : public RMDBError {
   public:
    IncompatibleTypeError(const std::string &lhs, const std::string &rhs)
        : RMDBError("Incompatible type error: lhs " + lhs + ", rhs " + rhs) {}
};

class AmbiguousColumnError : public RMDBError {
   public:
    AmbiguousColumnError(const std::string &col_name) : RMDBError("Ambiguous column: " + col_name) {}
};

class PageNotExistError : public RMDBError {
   public:
    PageNotExistError(const std::string &table_name, int page_no)
        : RMDBError("Page " + std::to_string(page_no) + " in table " + table_name + "not exits") {}
};