// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "parser_defs.h"

#include <algorithm>
#include <sstream>

namespace rucbase::parser {
namespace {

thread_local ParseError last_parse_error;

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

}  // namespace

void ResetParseError() { last_parse_error = {}; }

void RecordParseError(const int line, const int column, const char* message) {
    last_parse_error.line = std::max(line, 1);
    last_parse_error.column = std::max(column, 1);
    last_parse_error.message = message == nullptr ? "syntax error" : message;
}

ParseError GetParseError() { return last_parse_error; }

std::string FormatParseError(const std::string_view sql, const ParseError& error) {
    const int line_number = std::max(error.line, 1);
    const int column_number = std::max(error.column, 1);

    std::ostringstream output;
    output << "Parser Error at line " << line_number << " column " << column_number << ": "
           << (error.message.empty() ? "syntax error" : error.message) << '\n';

    std::string_view source_line;
    if (FindLine(sql, line_number, &source_line)) {
        output << source_line << '\n';
        const std::size_t caret_offset =
            std::min(static_cast<std::size_t>(column_number - 1), source_line.size());
        for (std::size_t index = 0; index < caret_offset; ++index) {
            output << (source_line[index] == '\t' ? '\t' : ' ');
        }
        output << "^\n";
    }
    return output.str();
}

}  // namespace rucbase::parser
