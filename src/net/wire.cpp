#include "net/wire.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace rucbase::wire {
namespace {

void AppendU8(std::string *out, uint8_t v) { out->push_back(static_cast<char>(v)); }

void AppendU16(std::string *out, uint16_t v) {
    const uint16_t be = htons(v);
    out->append(reinterpret_cast<const char *>(&be), sizeof(be));
}

void AppendU32(std::string *out, uint32_t v) {
    const uint32_t be = htonl(v);
    out->append(reinterpret_cast<const char *>(&be), sizeof(be));
}

void AppendU64(std::string *out, uint64_t v) {
    const uint32_t hi = htonl(static_cast<uint32_t>(v >> 32));
    const uint32_t lo = htonl(static_cast<uint32_t>(v & 0xffffffffu));
    out->append(reinterpret_cast<const char *>(&hi), sizeof(hi));
    out->append(reinterpret_cast<const char *>(&lo), sizeof(lo));
}

bool ReadU8(const std::string &buf, size_t *off, uint8_t *v) {
    if (*off + 1 > buf.size()) {
        return false;
    }
    *v = static_cast<uint8_t>(buf[*off]);
    *off += 1;
    return true;
}

bool ReadU16(const std::string &buf, size_t *off, uint16_t *v) {
    if (*off + 2 > buf.size()) {
        return false;
    }
    uint16_t be = 0;
    std::memcpy(&be, buf.data() + *off, 2);
    *v = ntohs(be);
    *off += 2;
    return true;
}

bool ReadU32(const std::string &buf, size_t *off, uint32_t *v) {
    if (*off + 4 > buf.size()) {
        return false;
    }
    uint32_t be = 0;
    std::memcpy(&be, buf.data() + *off, 4);
    *v = ntohl(be);
    *off += 4;
    return true;
}

bool ReadU64(const std::string &buf, size_t *off, uint64_t *v) {
    uint32_t hi = 0;
    uint32_t lo = 0;
    if (!ReadU32(buf, off, &hi) || !ReadU32(buf, off, &lo)) {
        return false;
    }
    *v = (static_cast<uint64_t>(hi) << 32) | lo;
    return true;
}

bool ReadBytes(const std::string &buf, size_t *off, size_t n, std::string *out) {
    if (*off + n > buf.size()) {
        return false;
    }
    out->assign(buf.data() + *off, n);
    *off += n;
    return true;
}

}  // namespace

bool ReadExact(int fd, void *buf, size_t n) {
    auto *p = static_cast<char *>(buf);
    size_t got = 0;
    while (got < n) {
        const ssize_t r = ::recv(fd, p + got, n - got, 0);
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (r == 0) {
            return false;
        }
        got += static_cast<size_t>(r);
    }
    return true;
}

bool WriteAll(int fd, const void *buf, size_t n) {
    auto *p = static_cast<const char *>(buf);
#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
    const int no_sigpipe = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof(no_sigpipe)) != 0) {
        return false;
    }
#endif
    size_t sent = 0;
    while (sent < n) {
        // A client disappearing while the server writes is a normal network
        // error, not a reason to terminate the whole database process.
#ifdef MSG_NOSIGNAL
        const ssize_t w = ::send(fd, p + sent, n - sent, MSG_NOSIGNAL);
#else
        const ssize_t w = ::send(fd, p + sent, n - sent, 0);
#endif
        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (w == 0) {
            return false;
        }
        sent += static_cast<size_t>(w);
    }
    return true;
}

static bool WriteHandshakeBytes(int fd) {
    uint8_t msg[8];
    std::memcpy(msg, kMagic, 4);
    const uint16_t major = htons(kMajor);
    const uint16_t minor = htons(kMinor);
    std::memcpy(msg + 4, &major, 2);
    std::memcpy(msg + 6, &minor, 2);
    return WriteAll(fd, msg, sizeof(msg));
}

static bool CheckHandshakeBytes(const uint8_t msg[8]) {
    if (std::memcmp(msg, kMagic, 4) != 0) {
        return false;
    }
    uint16_t major = 0;
    uint16_t minor = 0;
    std::memcpy(&major, msg + 4, 2);
    std::memcpy(&minor, msg + 6, 2);
    return ntohs(major) == kMajor && ntohs(minor) == kMinor;
}

bool WriteHandshake(int fd) { return WriteHandshakeBytes(fd); }

bool ReadAndCheckHandshake(int fd) {
    uint8_t msg[8];
    if (!ReadExact(fd, msg, sizeof(msg))) {
        return false;
    }
    if (!CheckHandshakeBytes(msg)) {
        return false;
    }
    return WriteAll(fd, msg, sizeof(msg));
}

bool ClientHandshake(int fd) {
    if (!WriteHandshakeBytes(fd)) {
        return false;
    }
    uint8_t echo[8];
    if (!ReadExact(fd, echo, sizeof(echo))) {
        return false;
    }
    return CheckHandshakeBytes(echo);
}

bool WriteFrame(int fd, uint8_t tag, uint8_t flags, const std::string &payload) {
    if (payload.size() > kMaxPayloadBytes) {
        return false;
    }
    if ((tag == kTagError || tag == kTagTransactionAbort) && payload.size() > kMaxDiagnosticBytes) {
        return false;
    }
    uint8_t header[8];
    const uint32_t len = htonl(static_cast<uint32_t>(payload.size()));
    std::memcpy(header, &len, 4);
    header[4] = tag;
    header[5] = flags;
    header[6] = 0;
    header[7] = 0;
    if (!WriteAll(fd, header, sizeof(header))) {
        return false;
    }
    if (payload.empty()) {
        return true;
    }
    return WriteAll(fd, payload.data(), payload.size());
}

bool ReadFrame(int fd, Frame *out) {
    uint8_t header[8];
    if (!ReadExact(fd, header, sizeof(header))) {
        return false;
    }
    uint32_t len_be = 0;
    std::memcpy(&len_be, header, 4);
    const uint32_t len = ntohl(len_be);
    if (len > kMaxPayloadBytes) {
        return false;
    }
    if (header[6] != 0 || header[7] != 0) {
        return false;  // reserved must be 0
    }
    out->tag = header[4];
    out->flags = header[5];
    out->payload.assign(len, '\0');
    if (len == 0) {
        return true;
    }
    return ReadExact(fd, out->payload.data(), len);
}

uint8_t ColTypeToWire(int col_type) {
    // Must match defs.h: TYPE_INT=0, TYPE_FLOAT=1, TYPE_STRING=2
    switch (col_type) {
        case 0:
            return kTypeInt32;
        case 1:
            return kTypeFloat32;
        case 2:
        default:
            return kTypeChar;
    }
}

std::string EncodeMeta(const std::vector<ColumnDef> &columns) {
    std::string payload;
    AppendU16(&payload, static_cast<uint16_t>(columns.size()));
    for (const auto &column : columns) {
        const std::string &name = column.name.empty() ? std::string("col") : column.name;
        const uint16_t name_len = static_cast<uint16_t>(std::min<size_t>(name.size(), 0xffffu));
        AppendU16(&payload, name_len);
        payload.append(name.data(), name_len);
        AppendU8(&payload, column.sql_type);
    }
    return payload;
}

std::string EncodeMetaSingleCharColumn(const std::string &column_name) {
    ColumnDef column;
    column.name = column_name.empty() ? "output" : column_name;
    column.sql_type = kTypeChar;
    return EncodeMeta({column});
}

static void AppendCell(std::string *payload, const Cell &cell) {
    if (cell.is_null) {
        AppendU8(payload, 0);
        return;
    }
    AppendU8(payload, 1);
    if (cell.sql_type == kTypeInt32) {
        uint32_t bits = 0;
        std::memcpy(&bits, &cell.int_val, sizeof(bits));
        AppendU32(payload, bits);
    } else if (cell.sql_type == kTypeFloat32) {
        uint32_t bits = 0;
        std::memcpy(&bits, &cell.float_val, sizeof(bits));
        AppendU32(payload, bits);
    } else {
        AppendU32(payload, static_cast<uint32_t>(cell.str_val.size()));
        payload->append(cell.str_val);
    }
}

std::string EncodeRow(const std::vector<Cell> &cells) {
    std::string payload;
    for (const auto &cell : cells) {
        AppendCell(&payload, cell);
    }
    return payload;
}

std::string EncodeRowSingleChar(const std::string &value) {
    Cell cell;
    cell.sql_type = kTypeChar;
    cell.str_val = value;
    return EncodeRow({cell});
}

std::string EncodeResultEnd(uint64_t row_count) {
    std::string payload;
    AppendU64(&payload, row_count);
    return payload;
}

static bool DecodeCharCell(const std::string &payload, size_t *off, std::string *text) {
    uint8_t present = 0;
    if (!ReadU8(payload, off, &present)) {
        return false;
    }
    if (present == 0) {
        text->clear();
        return true;
    }
    if (present != 1) {
        return false;
    }
    uint32_t n = 0;
    if (!ReadU32(payload, off, &n)) {
        return false;
    }
    return ReadBytes(payload, off, n, text);
}

static bool DecodeIntCell(const std::string &payload, size_t *off, std::string *text) {
    uint8_t present = 0;
    if (!ReadU8(payload, off, &present)) {
        return false;
    }
    if (present == 0) {
        text->clear();
        return true;
    }
    if (present != 1) {
        return false;
    }
    uint32_t bits = 0;
    if (!ReadU32(payload, off, &bits)) {
        return false;
    }
    const int32_t value = static_cast<int32_t>(bits);
    *text = std::to_string(value);
    return true;
}

static bool DecodeFloatCell(const std::string &payload, size_t *off, std::string *text) {
    uint8_t present = 0;
    if (!ReadU8(payload, off, &present)) {
        return false;
    }
    if (present == 0) {
        text->clear();
        return true;
    }
    if (present != 1) {
        return false;
    }
    uint32_t bits = 0;
    if (!ReadU32(payload, off, &bits)) {
        return false;
    }
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    *text = std::to_string(value);
    return true;
}

// Match RecordPrinter layout so concurrency/client golden files stay stable.
static constexpr size_t kPrettyColWidth = 16;

static std::string FormatTableCell(std::string value) {
    if (value.size() > kPrettyColWidth) {
        value = value.substr(0, kPrettyColWidth - 3) + "...";
    }
    std::ostringstream ss;
    ss << "| " << std::setw(static_cast<int>(kPrettyColWidth)) << value << " ";
    return ss.str();
}

static std::string FormatSeparator(size_t num_cols) {
    std::string line;
    for (size_t i = 0; i < num_cols; ++i) {
        line += "+" + std::string(kPrettyColWidth + 2, '-');
    }
    line += "+\n";
    return line;
}

static std::string FormatTable(const std::vector<std::string> &names,
                               const std::vector<std::vector<std::string>> &rows) {
    if (names.empty()) {
        return "";
    }
    std::string out = FormatSeparator(names.size());
    for (const auto &name : names) {
        out += FormatTableCell(name);
    }
    out += "|\n";
    out += FormatSeparator(names.size());
    for (const auto &row : rows) {
        for (size_t i = 0; i < names.size(); ++i) {
            out += FormatTableCell(i < row.size() ? row[i] : "");
        }
        out += "|\n";
    }
    out += FormatSeparator(names.size());
    out += "Total record(s): " + std::to_string(rows.size()) + "\n";
    return out;
}

bool ExecStream(int fd, const std::string &sql, std::string *response_text, std::string *diagnostic) {
    if (response_text != nullptr) {
        response_text->clear();
    }
    if (diagnostic != nullptr) {
        diagnostic->clear();
    }
    if (sql.empty() || sql.size() > kMaxPayloadBytes) {
        if (diagnostic != nullptr) {
            *diagnostic = "invalid SQL payload size";
        }
        return false;
    }
    if (!WriteFrame(fd, kTagExecStream, 0, sql)) {
        if (diagnostic != nullptr) {
            *diagnostic = "failed to send EXEC_STREAM";
        }
        return false;
    }

    std::vector<uint8_t> col_types;
    std::vector<std::string> col_names;
    std::vector<std::vector<std::string>> rows;
    enum class ResponseState { AwaitFirst, Rows };
    ResponseState state = ResponseState::AwaitFirst;
    bool saw_terminal = false;
    bool is_query = false;

    while (!saw_terminal) {
        Frame frame;
        if (!ReadFrame(fd, &frame)) {
            if (diagnostic != nullptr) {
                *diagnostic = "failed to read response frame";
            }
            return false;
        }
        if (frame.flags != 0) {
            if (diagnostic != nullptr) {
                *diagnostic = "non-zero response flags";
            }
            return false;
        }

        switch (frame.tag) {
            case kTagCommandOk:
                if (state != ResponseState::AwaitFirst || !frame.payload.empty()) {
                    if (diagnostic != nullptr) {
                        *diagnostic = "invalid COMMAND_OK";
                    }
                    return false;
                }
                saw_terminal = true;
                break;
            case kTagError:
            case kTagTransactionAbort:
                if (frame.payload.size() > kMaxDiagnosticBytes) {
                    if (diagnostic != nullptr) {
                        *diagnostic = "diagnostic payload exceeds 64 KiB";
                    }
                    return false;
                }
                if (diagnostic != nullptr) {
                    *diagnostic = frame.payload;
                }
                return false;
            case kTagMeta: {
                if (state != ResponseState::AwaitFirst) {
                    if (diagnostic != nullptr) {
                        *diagnostic = "duplicate META";
                    }
                    return false;
                }
                is_query = true;
                size_t off = 0;
                uint16_t col_count = 0;
                if (!ReadU16(frame.payload, &off, &col_count) || col_count == 0) {
                    if (diagnostic != nullptr) {
                        *diagnostic = "invalid META";
                    }
                    return false;
                }
                col_types.clear();
                col_names.clear();
                for (uint16_t i = 0; i < col_count; ++i) {
                    uint16_t name_len = 0;
                    if (!ReadU16(frame.payload, &off, &name_len) || name_len == 0) {
                        if (diagnostic != nullptr) {
                            *diagnostic = "invalid META column name";
                        }
                        return false;
                    }
                    std::string name;
                    if (!ReadBytes(frame.payload, &off, name_len, &name)) {
                        if (diagnostic != nullptr) {
                            *diagnostic = "invalid META column name bytes";
                        }
                        return false;
                    }
                    uint8_t sql_type = 0;
                    if (!ReadU8(frame.payload, &off, &sql_type)) {
                        if (diagnostic != nullptr) {
                            *diagnostic = "invalid META column type";
                        }
                        return false;
                    }
                    if (sql_type != kTypeInt32 && sql_type != kTypeFloat32 && sql_type != kTypeChar) {
                        if (diagnostic != nullptr) {
                            *diagnostic = "unknown META column type";
                        }
                        return false;
                    }
                    col_types.push_back(sql_type);
                    col_names.push_back(std::move(name));
                }
                if (off != frame.payload.size()) {
                    if (diagnostic != nullptr) {
                        *diagnostic = "trailing bytes in META";
                    }
                    return false;
                }
                state = ResponseState::Rows;
                break;
            }
            case kTagRow: {
                if (state != ResponseState::Rows || col_types.empty()) {
                    if (diagnostic != nullptr) {
                        *diagnostic = "ROW before META";
                    }
                    return false;
                }
                size_t off = 0;
                std::vector<std::string> row;
                row.reserve(col_types.size());
                for (uint8_t sql_type : col_types) {
                    std::string cell;
                    bool ok = false;
                    if (sql_type == kTypeChar) {
                        ok = DecodeCharCell(frame.payload, &off, &cell);
                    } else if (sql_type == kTypeInt32) {
                        ok = DecodeIntCell(frame.payload, &off, &cell);
                    } else if (sql_type == kTypeFloat32) {
                        ok = DecodeFloatCell(frame.payload, &off, &cell);
                    }
                    if (!ok) {
                        if (diagnostic != nullptr) {
                            *diagnostic = "invalid ROW cell";
                        }
                        return false;
                    }
                    row.push_back(std::move(cell));
                }
                if (off != frame.payload.size()) {
                    if (diagnostic != nullptr) {
                        *diagnostic = "trailing bytes in ROW";
                    }
                    return false;
                }
                rows.push_back(std::move(row));
                break;
            }
            case kTagResultEnd: {
                if (state != ResponseState::Rows) {
                    if (diagnostic != nullptr) {
                        *diagnostic = "RESULT_END before META";
                    }
                    return false;
                }
                size_t off = 0;
                uint64_t row_count = 0;
                if (!ReadU64(frame.payload, &off, &row_count) || off != frame.payload.size()) {
                    if (diagnostic != nullptr) {
                        *diagnostic = "invalid RESULT_END";
                    }
                    return false;
                }
                if (row_count != rows.size()) {
                    if (diagnostic != nullptr) {
                        *diagnostic = "RESULT_END row count mismatch";
                    }
                    return false;
                }
                saw_terminal = true;
                break;
            }
            default:
                if (diagnostic != nullptr) {
                    *diagnostic = "unknown response tag";
                }
                return false;
        }
    }

    if (response_text != nullptr) {
        if (is_query) {
            // Single-column CHAR blobs (help/show bridge) print raw text without a table chrome.
            if (col_types.size() == 1 && col_types[0] == kTypeChar && col_names.size() == 1 &&
                (col_names[0] == "output" || col_names[0] == "database")) {
                for (const auto &row : rows) {
                    if (!row.empty()) {
                        response_text->append(row[0]);
                    }
                }
            } else {
                *response_text = FormatTable(col_names, rows);
            }
        }
    }
    return true;
}

}  // namespace rucbase::wire
