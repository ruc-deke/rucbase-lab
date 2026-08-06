#pragma once

// Minimal RMDB wire protocol (docs/rmdb_wire.md), EXEC_STREAM path only.
// Byte order: big-endian for multi-byte integers.

#include <cstdint>
#include <string>
#include <vector>

namespace rucbase::wire {

constexpr uint16_t kMajor = 3;
constexpr uint16_t kMinor = 0;
// docs/rmdb_wire.md §1: 4-byte ASCII magic + major + minor (8 bytes total).
constexpr char kMagic[4] = {'R', 'U', 'C', 'B'};
constexpr uint32_t kMaxPayloadBytes = 1u << 20;  // 1 MiB
constexpr uint32_t kMaxDiagnosticBytes = 64u << 10;  // 64 KiB

// Client → server
constexpr uint8_t kTagExecStream = 0x20;

// Server → client
constexpr uint8_t kTagMeta = 0x01;
constexpr uint8_t kTagRow = 0x02;
constexpr uint8_t kTagCommandOk = 0x10;
constexpr uint8_t kTagResultEnd = 0x11;
constexpr uint8_t kTagTransactionAbort = 0x12;
constexpr uint8_t kTagError = 0x13;

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
bool ReadExact(int fd, void *buf, size_t n);
bool WriteAll(int fd, const void *buf, size_t n);

bool WriteHandshake(int fd);
bool ReadAndCheckHandshake(int fd);  // server: read peer, verify, echo same 8 bytes
bool ClientHandshake(int fd);        // client: write, read, verify echo

bool WriteFrame(int fd, uint8_t tag, uint8_t flags, const std::string &payload);
bool ReadFrame(int fd, Frame *out);

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

std::string EncodeMeta(const std::vector<ColumnDef> &columns);
std::string EncodeMetaSingleCharColumn(const std::string &column_name);
std::string EncodeRow(const std::vector<Cell> &cells);
std::string EncodeRowSingleChar(const std::string &value);
std::string EncodeResultEnd(uint64_t row_count);

uint8_t ColTypeToWire(int col_type);  // maps engine ColType ordinals: INT/FLOAT/STRING

// Client helper: EXEC_STREAM + collect response text for teaching UIs/tests.
// - COMMAND_OK → empty ok text
// - META/ROW/RESULT_END → pretty table (RecordPrinter-compatible layout)
// - ERROR / TRANSACTION_ABORT → returns false and fills *diagnostic
bool ExecStream(int fd, const std::string &sql, std::string *response_text, std::string *diagnostic);

}  // namespace rucbase::wire
