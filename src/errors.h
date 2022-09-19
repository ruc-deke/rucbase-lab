#pragma once

#include <cerrno>
#include <cstring>
#include <string>

class RedBaseError : public std::exception {
    std::string _msg;

   public:
    RedBaseError(const std::string &msg) : _msg("Error: " + msg) {}

    const char *what() const noexcept override { return _msg.c_str(); }
};

class InternalError : public RedBaseError {
   public:
    InternalError(const std::string &msg) : RedBaseError(msg) {}
};

// PF errors
class UnixError : public RedBaseError {
   public:
    UnixError() : RedBaseError(strerror(errno)) {}
};

class FileNotOpenError : public RedBaseError {
   public:
    FileNotOpenError(int fd) : RedBaseError("Invalid file descriptor: " + std::to_string(fd)) {}
};

class FileNotClosedError : public RedBaseError {
   public:
    FileNotClosedError(const std::string &filename) : RedBaseError("File is opened: " + filename) {}
};

class FileExistsError : public RedBaseError {
   public:
    FileExistsError(const std::string &filename) : RedBaseError("File already exists: " + filename) {}
};

class FileNotFoundError : public RedBaseError {
   public:
    FileNotFoundError(const std::string &filename) : RedBaseError("File not found: " + filename) {}
};

// RM errors
class RecordNotFoundError : public RedBaseError {
   public:
    RecordNotFoundError(int page_no, int slot_no)
        : RedBaseError("Record not found: (" + std::to_string(page_no) + "," + std::to_string(slot_no) + ")") {}
};

class InvalidRecordSizeError : public RedBaseError {
   public:
    InvalidRecordSizeError(int record_size) : RedBaseError("Invalid record size: " + std::to_string(record_size)) {}
};

// IX errors
class InvalidColLengthError : public RedBaseError {
   public:
    InvalidColLengthError(int col_len) : RedBaseError("Invalid column length: " + std::to_string(col_len)) {}
};

class IndexEntryNotFoundError : public RedBaseError {
   public:
    IndexEntryNotFoundError() : RedBaseError("Index entry not found") {}
};

// SM errors
class DatabaseNotFoundError : public RedBaseError {
   public:
    DatabaseNotFoundError(const std::string &db_name) : RedBaseError("Database not found: " + db_name) {}
};

class DatabaseExistsError : public RedBaseError {
   public:
    DatabaseExistsError(const std::string &db_name) : RedBaseError("Database already exists: " + db_name) {}
};

class TableNotFoundError : public RedBaseError {
   public:
    TableNotFoundError(const std::string &tab_name) : RedBaseError("Table not found: " + tab_name) {}
};

class TableExistsError : public RedBaseError {
   public:
    TableExistsError(const std::string &tab_name) : RedBaseError("Table already exists: " + tab_name) {}
};

class ColumnNotFoundError : public RedBaseError {
   public:
    ColumnNotFoundError(const std::string &col_name) : RedBaseError("Column not found: " + col_name) {}
};

class IndexNotFoundError : public RedBaseError {
   public:
    IndexNotFoundError(const std::string &tab_name, const std::string &col_name)
        : RedBaseError("Index not found: " + tab_name + '.' + col_name) {}
};

class IndexExistsError : public RedBaseError {
   public:
    IndexExistsError(const std::string &tab_name, const std::string &col_name)
        : RedBaseError("Index already exists: " + tab_name + '.' + col_name) {}
};

// QL errors
class InvalidValueCountError : public RedBaseError {
   public:
    InvalidValueCountError() : RedBaseError("Invalid value count") {}
};

class StringOverflowError : public RedBaseError {
   public:
    StringOverflowError() : RedBaseError("String is too long") {}
};

class IncompatibleTypeError : public RedBaseError {
   public:
    IncompatibleTypeError(const std::string &lhs, const std::string &rhs)
        : RedBaseError("Incompatible type error: lhs " + lhs + ", rhs " + rhs) {}
};

class AmbiguousColumnError : public RedBaseError {
   public:
    AmbiguousColumnError(const std::string &col_name) : RedBaseError("Ambiguous column: " + col_name) {}
};

class PageNotExistError : public RedBaseError {
   public:
    PageNotExistError(const std::string &table_name, int page_no)
        : RedBaseError("Page " + std::to_string(page_no) + " in table " + table_name + "not exits") {}
};