// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

/**
 * @file parser.cpp
 * @brief 实现 Parser 公共门面，并集中管理全部 Flex scanner 状态。
 */

#include "parser.h"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <climits>
#include <cstdlib>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <system_error>

#include "parser_internal.h"
#include "yacc.tab.h"

struct yy_buffer_state;
using YY_BUFFER_STATE = yy_buffer_state*;

YY_BUFFER_STATE yy_scan_bytes(const char* bytes, std::size_t length);
void yy_delete_buffer(YY_BUFFER_STATE buffer);

namespace rucbase::parser {
namespace {

std::mutex parser_mutex;

/** @brief 通过 RAII 管理由 yy_scan_bytes() 创建的 buffer。 */
class ScannerBuffer {
   public:
    explicit ScannerBuffer(YY_BUFFER_STATE buffer) : buffer_(buffer) {
        if (buffer_ == nullptr) {
            throw std::runtime_error("failed to allocate parser buffer");
        }
    }

    ~ScannerBuffer() { yy_delete_buffer(buffer_); }

    ScannerBuffer(const ScannerBuffer&) = delete;
    ScannerBuffer& operator=(const ScannerBuffer&) = delete;

   private:
    YY_BUFFER_STATE buffer_;
};

/**
 * @brief 在不复制字符串的情况下定位指定源码行。
 * @return @p target_line 超出 SQL 文本范围时返回 false。
 */
bool FindLine(const std::string_view sql, const int target_line, std::string_view* line) {
    if (target_line < 1 || line == nullptr) {
        return false;
    }

    std::size_t begin = 0;
    for (int current_line = 1; current_line < target_line; ++current_line) {
        const std::size_t newline = sql.find('\n', begin);
        if (newline == std::string_view::npos) {
            return false;
        }
        begin = newline + 1;
    }

    std::size_t end = sql.find('\n', begin);
    if (end == std::string_view::npos) {
        end = sql.size();
    }
    if (end > begin && sql[end - 1] == '\r') {
        --end;
    }
    *line = sql.substr(begin, end - begin);
    return true;
}

/** @brief 将字节偏移转换为 Parser 使用的从 1 开始计数的行号和列号。 */
ParseError ErrorAtOffset(const std::string_view sql, const std::size_t offset, std::string message) {
    int line = 1;
    int column = 1;
    for (std::size_t index = 0; index < offset; ++index) {
        if (sql[index] == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }
    }
    return {line, column, std::move(message)};
}

}  // namespace

ParseResult Parse(const std::string_view sql) {
    const std::size_t nul = sql.find('\0');
    if (nul != std::string_view::npos) {
        return {nullptr, ErrorAtOffset(sql, nul, "NUL byte is not allowed in SQL")};
    }
    if (sql.size() > static_cast<std::size_t>(INT_MAX)) {
        return {nullptr, ParseError{1, 1, "SQL statement is too large"}};
    }

    // Flex 仍持有进程级 scanner 状态，因此在 Parser 模块内部串行化访问；
    // 调用方获得的 Parse() 接口仍然是线程安全的。（PS：可以实现可重入，开启并行解析）
    std::lock_guard<std::mutex> lock(parser_mutex);
    ParseContext context;
    ScannerBuffer buffer(yy_scan_bytes(sql.data(), sql.size()));
    const int status = yyparse(&context);

    if (status != 0 && !context.error.has_value()) {
        context.error = ParseError{1, 1, "syntax error"};
    }
    if (status == 0 && context.statement == nullptr && !context.error.has_value()) {
        context.error = ParseError{1, 1, "empty SQL statement"};
    }
    return {.statement = std::move(context.statement), .error = std::move(context.error)};
}

std::string FormatError(const std::string_view sql, const ParseError& error) {
    const int line_number = std::max(error.line, 1);
    const int column_number = std::max(error.column, 1);

    std::ostringstream output;
    output << "Parser Error at line " << line_number << " column " << column_number << ": "
           << (error.message.empty() ? "syntax error" : error.message) << '\n';

    std::string_view source_line;
    if (FindLine(sql, line_number, &source_line)) {
        output << source_line << '\n';
        const std::size_t caret_offset = std::min(static_cast<std::size_t>(column_number - 1), source_line.size());
        for (std::size_t index = 0; index < caret_offset; ++index) {
            output << (source_line[index] == '\t' ? '\t' : ' ');
        }
        output << "^\n";
    }
    return output.str();
}

bool ParseIntegerLiteral(const char* text, const int length, int* value) {
    if (text == nullptr || length <= 0 || value == nullptr) {
        return false;
    }

    const char* begin = text;
    const char* end = text + length;
    if (*begin == '+') {
        ++begin;
    }
    const auto result = std::from_chars(begin, end, *value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool ParseFloatLiteral(const char* text, const int length, float* value) {
    if (text == nullptr || length <= 0 || value == nullptr) {
        return false;
    }

    std::string input(text, static_cast<std::size_t>(length));
    char* end = nullptr;
    errno = 0;
    const float parsed = std::strtof(input.c_str(), &end);
    if (errno == ERANGE || end != input.c_str() + input.size()) {
        return false;
    }
    *value = parsed;
    return true;
}

std::string DecodeStringLiteral(const char* text, const int length) {
    std::string value;
    if (text == nullptr || length < 2) {
        return value;
    }

    value.reserve(static_cast<std::size_t>(length - 2));
    for (int index = 1; index < length - 1; ++index) {
        if (text[index] == '\'' && index + 1 < length - 1 && text[index + 1] == '\'') {
            ++index;
        }
        value.push_back(text[index]);
    }
    return value;
}

}  // namespace rucbase::parser
