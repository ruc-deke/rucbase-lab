// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <cerrno>
#include <cstring>
#include <exception>
#include <string>
#include <utility>
#include <vector>

class RMDBError : public std::exception {
public:
    RMDBError() : RMDBError(std::string{}) {}

    explicit RMDBError(std::string message) : message_("Error: " + std::move(message)) {}

    const char* what() const noexcept override { return message_.c_str(); }

private:
    std::string message_;
};

class InternalError : public RMDBError {
public:
    explicit InternalError(const std::string& msg) : RMDBError(msg) {}
};

class NotImplementedError : public RMDBError {
public:
    explicit NotImplementedError(const std::string& component) : RMDBError(component + " is not implemented") {}
};

// PF errors
class UnixError : public RMDBError {
public:
    UnixError() : RMDBError(strerror(errno)) {}
};

class FileNotOpenError : public RMDBError {
public:
    explicit FileNotOpenError(int fd) : RMDBError("Invalid file descriptor: " + std::to_string(fd)) {}
};

class FileNotClosedError : public RMDBError {
public:
    explicit FileNotClosedError(const std::string& filename) : RMDBError("File is opened: " + filename) {}
};

class FileExistsError : public RMDBError {
public:
    explicit FileExistsError(const std::string& filename) : RMDBError("File already exists: " + filename) {}
};

class FileNotFoundError : public RMDBError {
public:
    explicit FileNotFoundError(const std::string& filename) : RMDBError("File not found: " + filename) {}
};

// RM errors
class RecordNotFoundError : public RMDBError {
public:
    RecordNotFoundError(int page_no, int slot_no)
        : RMDBError("Record not found: (" + std::to_string(page_no) + "," + std::to_string(slot_no) + ")") {}
};

class InvalidRecordSizeError : public RMDBError {
public:
    explicit InvalidRecordSizeError(int record_size)
        : RMDBError("Invalid record size: " + std::to_string(record_size)) {}
};

// IX errors
class InvalidColLengthError : public RMDBError {
public:
    explicit InvalidColLengthError(int col_len) : RMDBError("Invalid column length: " + std::to_string(col_len)) {}
};

class IndexEntryNotFoundError : public RMDBError {
public:
    IndexEntryNotFoundError() : RMDBError("Index entry not found") {}
};

class DuplicateKeyError : public RMDBError {
public:
    DuplicateKeyError() : RMDBError("Duplicate key on unique index") {}
};

// SM errors
class DatabaseNotFoundError : public RMDBError {
public:
    explicit DatabaseNotFoundError(const std::string& db_name) : RMDBError("Database not found: " + db_name) {}
};

class DatabaseExistsError : public RMDBError {
public:
    explicit DatabaseExistsError(const std::string& db_name) : RMDBError("Database already exists: " + db_name) {}
};

class TableNotFoundError : public RMDBError {
public:
    explicit TableNotFoundError(const std::string& tab_name) : RMDBError("Table not found: " + tab_name) {}
};

class TableExistsError : public RMDBError {
public:
    explicit TableExistsError(const std::string& tab_name) : RMDBError("Table already exists: " + tab_name) {}
};

class ColumnNotFoundError : public RMDBError {
public:
    explicit ColumnNotFoundError(const std::string& col_name) : RMDBError("Column not found: " + col_name) {}
};

class ColumnExistsError : public RMDBError {
public:
    explicit ColumnExistsError(const std::string& col_name) : RMDBError("Column already exists: " + col_name) {}
};

namespace rucbase::error_detail {

inline std::string format_index_name(const std::string& tab_name, const std::vector<std::string>& col_names) {
    std::string name = tab_name + ".(";
    for (size_t i = 0; i < col_names.size(); ++i) {
        if (i > 0) {
            name += ", ";
        }
        name += col_names[i];
    }
    name += ')';
    return name;
}

}  // namespace rucbase::error_detail

class IndexNotFoundError : public RMDBError {
public:
    IndexNotFoundError(const std::string& tab_name, const std::vector<std::string>& col_names)
        : RMDBError("Index not found: " + rucbase::error_detail::format_index_name(tab_name, col_names)) {}
};

class IndexExistsError : public RMDBError {
public:
    IndexExistsError(const std::string& tab_name, const std::vector<std::string>& col_names)
        : RMDBError("Index already exists: " + rucbase::error_detail::format_index_name(tab_name, col_names)) {}
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
    IncompatibleTypeError(const std::string& lhs, const std::string& rhs)
        : RMDBError("Incompatible type error: lhs " + lhs + ", rhs " + rhs) {}
};

class AmbiguousColumnError : public RMDBError {
public:
    explicit AmbiguousColumnError(const std::string& col_name) : RMDBError("Ambiguous column: " + col_name) {}
};

class PageNotExistError : public RMDBError {
public:
    PageNotExistError(const std::string& table_name, int page_no)
        : RMDBError("Page " + std::to_string(page_no) + " in table " + table_name + "not exits") {}
};
