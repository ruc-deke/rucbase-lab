// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

// Minimal RMDB wire protocol (docs/rmdb_wire.md), EXEC_STREAM path only.
// Byte order: big-endian for multi-byte integers.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace rucbase::wire {

constexpr uint16_t kMajor = 3;
constexpr uint16_t kMinor = 1;
// docs/rmdb_wire.md §1: 4-byte ASCII magic + major + minor (8 bytes total).
constexpr char kMagic[4] = {'R', 'U', 'C', 'B'};
constexpr uint32_t kMaxPayloadBytes = 1U << 20U;      // 1 MiB
constexpr uint32_t kMaxDiagnosticBytes = 64U << 10U;  // 64 KiB
constexpr uint32_t kDefaultIoTimeoutMs = 120u * 1000u;

// Client → server
constexpr uint8_t kTagExecStream = 0x20;

// Server → client
constexpr uint8_t kTagMeta = 0x01;
constexpr uint8_t kTagRow = 0x02;
constexpr uint8_t kTagCommandOk = 0x10;
constexpr uint8_t kTagResultEnd = 0x11;
constexpr uint8_t kTagTransactionAbort = 0x12;
constexpr uint8_t kTagError = 0x13;

// META-only response flag: render the following single CHAR column as raw text.
constexpr uint8_t kFlagRawText = 0x01;

// SQL type tags (§3)
constexpr uint8_t kTypeInt32 = 0x01;
constexpr uint8_t kTypeFloat32 = 0x02;
constexpr uint8_t kTypeChar = 0x03;

// Teaching extension: EXEC_STREAM SQL equal to this string returns the open DB name.
constexpr char kDatabaseNameRequest[] = "__RUCBASE_DATABASE_NAME__";

struct Frame {
    uint8_t tag = 0;
    uint8_t flags = 0;
    std::string payload;
};

// Socket I/O (loops until complete or error).
bool ReadExact(int fd, void* buf, size_t n);
bool WriteAll(int fd, const void* buf, size_t n);
// Applies close-on-exec, non-zero read/write deadlines and platform SIGPIPE protection.
// Call once for every newly connected or accepted socket.
bool ConfigureConnectedSocket(int fd, uint32_t timeout_ms = kDefaultIoTimeoutMs);

bool WriteHandshake(int fd);
bool ReadAndCheckHandshake(int fd);  // server: read peer, verify, echo same 8 bytes
bool ClientHandshake(int fd);        // client: write, read, verify echo

bool WriteFrame(int fd, uint8_t tag, uint8_t flags, const std::string& payload);
bool ReadFrame(int fd, Frame* out);

struct ColumnDef {
    std::string name;
    uint8_t sql_type = kTypeChar;  // kTypeInt32 / kTypeFloat32 / kTypeChar
};

// Typed cell for encoding/decoding. Only the field matching sql_type is used.
struct Cell {
    bool is_null = false;
    uint8_t sql_type = kTypeChar;
    int32_t int_val = 0;
    float float_val = 0.0f;
    std::string str_val;
};

// Checked encoders are the preferred server-side API. They never truncate or
// silently coerce invalid schema/value data.
bool TryEncodeMeta(const std::vector<ColumnDef>& columns, std::string* payload, std::string* diagnostic = nullptr);
bool TryEncodeRow(const std::vector<Cell>& cells, std::string* payload, std::string* diagnostic = nullptr);

// Compatibility wrappers for existing callers. Invalid input returns an empty
// payload; new code should use the checked forms above.
std::string EncodeMeta(const std::vector<ColumnDef>& columns);
std::string EncodeMetaSingleCharColumn(const std::string& column_name);
std::string EncodeRow(const std::vector<Cell>& cells);
std::string EncodeRowSingleChar(const std::string& value);
std::string EncodeResultEnd(uint64_t row_count);

// Maps engine ColType ordinals (INT/FLOAT/STRING) without silently accepting
// future or corrupted enum values.
bool ColTypeToWire(int col_type, uint8_t* wire_type);

enum class ExecuteStatus {
    CommandOk,
    ResultSet,
    SqlError,
    TransactionAbort,
    InvalidRequest,
    TransportError,
    ProtocolError,
    ConsumerStopped,
};

struct ExecuteOptions {
    // Preserve the historical fixed-width CLI text. Disable this when rows are
    // consumed programmatically to keep client memory bounded.
    bool format_text = true;
    std::function<bool(const std::vector<ColumnDef>&)> on_meta;
    std::function<bool(const std::vector<Cell>&)> on_row;
};

struct ExecuteResult {
    ExecuteStatus status = ExecuteStatus::InvalidRequest;
    std::vector<ColumnDef> columns;
    uint64_t row_count = 0;
    std::string text;
    std::string diagnostic;

    bool ok() const noexcept { return status == ExecuteStatus::CommandOk || status == ExecuteStatus::ResultSet; }

    // SQL errors and transaction aborts are terminal response frames, so the
    // connection remains synchronized and can execute another statement.
    bool connection_reusable() const noexcept {
        return status == ExecuteStatus::CommandOk || status == ExecuteStatus::ResultSet ||
               status == ExecuteStatus::SqlError || status == ExecuteStatus::TransactionAbort ||
               status == ExecuteStatus::InvalidRequest || status == ExecuteStatus::ConsumerStopped;
    }
};

// Structured client protocol entry point. Rows are decoded incrementally; they
// are only retained when the caller's formatter/callback does so.
ExecuteResult ExecStreamResult(int fd, const std::string& sql, const ExecuteOptions& options = ExecuteOptions{});

// Client helper: EXEC_STREAM + collect response text for teaching UIs/tests.
// - COMMAND_OK → empty ok text
// - META/ROW/RESULT_END → pretty table (historical fixed-width CLI layout)
// - ERROR / TRANSACTION_ABORT → returns false and fills *diagnostic
bool ExecStream(int fd, const std::string& sql, std::string* response_text, std::string* diagnostic);

}  // namespace rucbase::wire
