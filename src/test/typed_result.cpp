// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "typed_result.h"

#include <charconv>
#include <cmath>
#include <limits>
#include <ostream>
#include <string_view>
#include <vector>

namespace rucbase::test {
namespace {

bool IsIgnoredSql(const std::string& sql) {
    const size_t first = sql.find_first_not_of(" \t\r\n");
    return first == std::string::npos || sql.compare(first, 2, "--") == 0;
}

void WriteJsonString(std::ostream& output, std::string_view value) {
    constexpr char kHexDigits[] = "0123456789abcdef";
    output.put('"');
    for (const unsigned char byte : value) {
        switch (byte) {
            case '"':
                output << "\\\"";
                break;
            case '\\':
                output << "\\\\";
                break;
            case '\b':
                output << "\\b";
                break;
            case '\f':
                output << "\\f";
                break;
            case '\n':
                output << "\\n";
                break;
            case '\r':
                output << "\\r";
                break;
            case '\t':
                output << "\\t";
                break;
            default:
                if (byte < 0x20U) {
                    output << "\\u00" << kHexDigits[byte >> 4U] << kHexDigits[byte & 0x0fU];
                } else {
                    output.put(static_cast<char>(byte));
                }
                break;
        }
    }
    output.put('"');
}

const char* TypeName(uint8_t sql_type) {
    switch (sql_type) {
        case wire::kTypeInt32:
            return "int32";
        case wire::kTypeFloat32:
            return "float32";
        case wire::kTypeChar:
            return "string";
        default:
            return nullptr;
    }
}

bool WriteFloat32(std::ostream& output, float value) {
    if (!std::isfinite(value)) {
        return false;
    }

    char buffer[64]{};
    const auto conversion = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general,
                                          std::numeric_limits<float>::max_digits10);
    if (conversion.ec != std::errc{}) {
        return false;
    }
    const std::string_view representation(buffer, static_cast<size_t>(conversion.ptr - buffer));
    output << representation;
    if (representation.find_first_of(".eE") == std::string_view::npos) {
        output << ".0";
    }
    return output.good();
}

bool WriteCell(std::ostream& output, const wire::Cell& cell, uint8_t expected_type) {
    if (cell.sql_type != expected_type || TypeName(cell.sql_type) == nullptr) {
        return false;
    }
    if (cell.is_null) {
        output << "null";
    } else if (cell.sql_type == wire::kTypeInt32) {
        output << cell.int_val;
    } else if (cell.sql_type == wire::kTypeFloat32) {
        return WriteFloat32(output, cell.float_val);
    } else {
        // Teaching fixtures use UTF-8 CHAR data; JSON escaping only needs to
        // handle control characters, quotes and backslashes here.
        WriteJsonString(output, cell.str_val);
    }
    return output.good();
}

bool WriteResultSet(std::ostream& output,
                    const wire::ExecuteResult& result,
                    const std::vector<std::vector<wire::Cell>>& rows) {
    if (result.columns.empty()) {
        return false;
    }
    for (const auto& column : result.columns) {
        if (TypeName(column.sql_type) == nullptr) {
            return false;
        }
    }

    output << "{\"status\":\"result_set\",\"columns\":[";
    for (size_t index = 0; index < result.columns.size(); ++index) {
        const auto& column = result.columns[index];
        const char* type_name = TypeName(column.sql_type);
        if (index != 0) {
            output.put(',');
        }
        output << "{\"name\":";
        WriteJsonString(output, column.name);
        output << ",\"type\":\"" << type_name << "\"}";
    }

    output << "],\"rows\":[";
    for (size_t row_index = 0; row_index < rows.size(); ++row_index) {
        const auto& row = rows[row_index];
        if (row.size() != result.columns.size()) {
            return false;
        }
        if (row_index != 0) {
            output.put(',');
        }
        output.put('[');
        for (size_t column_index = 0; column_index < row.size(); ++column_index) {
            if (column_index != 0) {
                output.put(',');
            }
            if (!WriteCell(output, row[column_index], result.columns[column_index].sql_type)) {
                return false;
            }
        }
        output.put(']');
    }
    output << "]}\n";
    return output.good();
}

}  // namespace

wire::ExecuteResult ExecuteWithoutText(wire::Client* client, const std::string& sql) {
    if (client == nullptr) {
        wire::ExecuteResult result;
        result.status = wire::ExecuteStatus::InvalidRequest;
        result.diagnostic = "null Wire client";
        return result;
    }
    if (IsIgnoredSql(sql)) {
        wire::ExecuteResult result;
        result.status = wire::ExecuteStatus::CommandOk;
        return result;
    }
    wire::ExecuteOptions options;
    options.format_text = false;
    return client->Execute(sql, options);
}

bool ExecuteAndWriteTypedResult(wire::Client* client,
                                const std::string& sql,
                                std::ostream& output,
                                std::ostream& diagnostics) {
    if (client == nullptr) {
        diagnostics << "EXEC_STREAM failed: null Wire client\n";
        return false;
    }
    if (IsIgnoredSql(sql)) {
        return true;
    }

    std::vector<std::vector<wire::Cell>> rows;
    wire::ExecuteOptions options;
    options.format_text = false;
    options.on_row = [&rows](const std::vector<wire::Cell>& row) {
        rows.push_back(row);
        return true;
    };
    const wire::ExecuteResult result = client->Execute(sql, options);

    switch (result.status) {
        case wire::ExecuteStatus::CommandOk:
            return true;
        case wire::ExecuteStatus::ResultSet:
            if (WriteResultSet(output, result, rows)) {
                return true;
            }
            diagnostics << "EXEC_STREAM failed: inconsistent typed result\n";
            return false;
        case wire::ExecuteStatus::SqlError:
            output << "{\"status\":\"sql_error\"}\n";
            return output.good();
        case wire::ExecuteStatus::TransactionAbort:
            output << "{\"status\":\"transaction_abort\"}\n";
            return output.good();
        case wire::ExecuteStatus::InvalidRequest:
        case wire::ExecuteStatus::TransportError:
        case wire::ExecuteStatus::ProtocolError:
        case wire::ExecuteStatus::ConsumerStopped:
            diagnostics << "EXEC_STREAM failed: " << (result.diagnostic.empty() ? "unknown error" : result.diagnostic)
                        << '\n';
            return false;
    }
    diagnostics << "EXEC_STREAM failed: unknown execution status\n";
    return false;
}

}  // namespace rucbase::test
