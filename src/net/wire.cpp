// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "net/wire.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <exception>
#include <limits>
#include <utility>

namespace rucbase::wire {
namespace {

enum class IoReadStatus { Ok, Eof, Timeout, Error };

enum class FrameReadStatus { Ok, TransportError, ProtocolError };

void SetDiagnostic(std::string* diagnostic, std::string message) {
    if (diagnostic != nullptr) {
        *diagnostic = std::move(message);
    }
}

bool IsKnownSqlType(uint8_t sql_type) {
    return sql_type == kTypeInt32 || sql_type == kTypeFloat32 || sql_type == kTypeChar;
}

IoReadStatus ReadExactInternal(int fd, void* buf, size_t n, std::string* diagnostic) {
    if (fd < 0 || (buf == nullptr && n != 0)) {
        SetDiagnostic(diagnostic, "invalid read arguments");
        return IoReadStatus::Error;
    }

    auto* p = static_cast<char*>(buf);
    size_t got = 0;
    while (got < n) {
        const size_t chunk = std::min(n - got, static_cast<size_t>(std::numeric_limits<ssize_t>::max()));
        const ssize_t r = ::recv(fd, p + got, chunk, 0);
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                SetDiagnostic(diagnostic, "socket read timed out");
                return IoReadStatus::Timeout;
            }
            SetDiagnostic(diagnostic, std::string("socket read failed: ") + std::strerror(errno));
            return IoReadStatus::Error;
        }
        if (r == 0) {
            SetDiagnostic(diagnostic, "peer closed the connection");
            return IoReadStatus::Eof;
        }
        got += static_cast<size_t>(r);
    }
    return IoReadStatus::Ok;
}

void AppendU8(std::string* out, uint8_t v) { out->push_back(static_cast<char>(v)); }

void AppendU16(std::string* out, uint16_t v) {
    const uint16_t be = htons(v);
    out->append(reinterpret_cast<const char*>(&be), sizeof(be));
}

void AppendU32(std::string* out, uint32_t v) {
    const uint32_t be = htonl(v);
    out->append(reinterpret_cast<const char*>(&be), sizeof(be));
}

void AppendU64(std::string* out, uint64_t v) {
    const uint32_t hi = htonl(static_cast<uint32_t>(v >> 32U));
    const uint32_t lo = htonl(static_cast<uint32_t>(v & 0xffffffffu));
    out->append(reinterpret_cast<const char*>(&hi), sizeof(hi));
    out->append(reinterpret_cast<const char*>(&lo), sizeof(lo));
}

bool ReadU8(const std::string& buf, size_t* off, uint8_t* v) {
    if (off == nullptr || v == nullptr || *off > buf.size() || buf.size() - *off < 1) {
        return false;
    }
    *v = static_cast<uint8_t>(buf[*off]);
    *off += 1;
    return true;
}

bool ReadU16(const std::string& buf, size_t* off, uint16_t* v) {
    if (off == nullptr || v == nullptr || *off > buf.size() || buf.size() - *off < 2) {
        return false;
    }
    uint16_t be = 0;
    std::memcpy(&be, buf.data() + *off, 2);
    *v = ntohs(be);
    *off += 2;
    return true;
}

bool ReadU32(const std::string& buf, size_t* off, uint32_t* v) {
    if (off == nullptr || v == nullptr || *off > buf.size() || buf.size() - *off < 4) {
        return false;
    }
    uint32_t be = 0;
    std::memcpy(&be, buf.data() + *off, 4);
    *v = ntohl(be);
    *off += 4;
    return true;
}

bool ReadU64(const std::string& buf, size_t* off, uint64_t* v) {
    if (v == nullptr) {
        return false;
    }
    uint32_t hi = 0;
    uint32_t lo = 0;
    if (!ReadU32(buf, off, &hi) || !ReadU32(buf, off, &lo)) {
        return false;
    }
    *v = (static_cast<uint64_t>(hi) << 32U) | static_cast<uint64_t>(lo);
    return true;
}

bool ReadBytes(const std::string& buf, size_t* off, size_t n, std::string* out) {
    if (off == nullptr || out == nullptr || *off > buf.size() || n > buf.size() - *off) {
        return false;
    }
    out->assign(buf.data() + *off, n);
    *off += n;
    return true;
}

bool WriteVectorsAll(int fd, iovec* vectors, size_t vector_count) {
    while (vector_count > 0) {
        msghdr message{};
        message.msg_iov = vectors;
        message.msg_iovlen = static_cast<decltype(message.msg_iovlen)>(vector_count);
#ifdef MSG_NOSIGNAL
        const ssize_t written = ::sendmsg(fd, &message, MSG_NOSIGNAL);
#else
        const ssize_t written = ::sendmsg(fd, &message, 0);
#endif
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (written == 0) {
            return false;
        }

        auto consumed = static_cast<size_t>(written);
        while (vector_count > 0 && consumed >= vectors[0].iov_len) {
            consumed -= vectors[0].iov_len;
            ++vectors;
            --vector_count;
        }
        if (vector_count > 0 && consumed != 0) {
            vectors[0].iov_base = static_cast<char*>(vectors[0].iov_base) + consumed;
            vectors[0].iov_len -= consumed;
        }
    }
    return true;
}

FrameReadStatus ReadFrameInternal(int fd, Frame* out, std::string* diagnostic) {
    if (out == nullptr) {
        SetDiagnostic(diagnostic, "null frame output");
        return FrameReadStatus::ProtocolError;
    }

    uint8_t header[8];
    if (ReadExactInternal(fd, header, sizeof(header), diagnostic) != IoReadStatus::Ok) {
        return FrameReadStatus::TransportError;
    }

    uint32_t len_be = 0;
    std::memcpy(&len_be, header, sizeof(len_be));
    const uint32_t len = ntohl(len_be);
    if (len > kMaxPayloadBytes) {
        SetDiagnostic(diagnostic, "frame payload exceeds 1 MiB");
        return FrameReadStatus::ProtocolError;
    }
    if ((header[4] == kTagError || header[4] == kTagTransactionAbort) && len > kMaxDiagnosticBytes) {
        SetDiagnostic(diagnostic, "diagnostic payload exceeds 64 KiB");
        return FrameReadStatus::ProtocolError;
    }
    if (header[6] != 0 || header[7] != 0) {
        SetDiagnostic(diagnostic, "non-zero reserved frame field");
        return FrameReadStatus::ProtocolError;
    }

    Frame frame;
    frame.tag = header[4];
    frame.flags = header[5];
    frame.payload.assign(len, '\0');
    if (len != 0 && ReadExactInternal(fd, frame.payload.data(), len, diagnostic) != IoReadStatus::Ok) {
        return FrameReadStatus::TransportError;
    }
    *out = std::move(frame);
    return FrameReadStatus::Ok;
}

}  // namespace

bool ReadExact(int fd, void* buf, size_t n) { return ReadExactInternal(fd, buf, n, nullptr) == IoReadStatus::Ok; }

bool WriteAll(int fd, const void* buf, size_t n) {
    if (fd < 0 || (buf == nullptr && n != 0)) {
        return false;
    }
    auto* p = static_cast<const char*>(buf);
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
        const size_t chunk = std::min(n - sent, static_cast<size_t>(std::numeric_limits<ssize_t>::max()));
        const ssize_t w = ::send(fd, p + sent, chunk, MSG_NOSIGNAL);
#else
        const size_t chunk = std::min(n - sent, static_cast<size_t>(std::numeric_limits<ssize_t>::max()));
        const ssize_t w = ::send(fd, p + sent, chunk, 0);
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

bool ConfigureConnectedSocket(int fd, uint32_t timeout_ms) {
    if (fd < 0 || timeout_ms == 0) {
        return false;
    }

    const int descriptor_flags = ::fcntl(fd, F_GETFD);
    if (descriptor_flags < 0) {
        return false;
    }
    const int close_on_exec_flags =
        static_cast<int>(static_cast<unsigned int>(descriptor_flags) | static_cast<unsigned int>(FD_CLOEXEC));
    if (::fcntl(fd, F_SETFD, close_on_exec_flags) < 0) {
        return false;
    }

#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
    const int no_sigpipe = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof(no_sigpipe)) != 0) {
        return false;
    }
#endif

    const timeval timeout{.tv_sec = static_cast<time_t>(timeout_ms / 1000U),
                          .tv_usec = static_cast<suseconds_t>((timeout_ms % 1000U) * 1000U)};
    if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
        ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) {
        return false;
    }

    sockaddr_storage local_address{};
    socklen_t local_length = sizeof(local_address);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&local_address), &local_length) != 0) {
        return false;
    }
    if (local_address.ss_family == AF_INET || local_address.ss_family == AF_INET6) {
        const int no_delay = 1;
        if (::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay)) != 0) {
            return false;
        }
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

bool WriteFrame(int fd, uint8_t tag, uint8_t flags, const std::string& payload) {
    if (fd < 0 || payload.size() > kMaxPayloadBytes) {
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

    iovec vectors[2]{};
    vectors[0].iov_base = header;
    vectors[0].iov_len = sizeof(header);
    size_t vector_count = 1;
    if (!payload.empty()) {
        vectors[1].iov_base = const_cast<char*>(payload.data());
        vectors[1].iov_len = payload.size();
        vector_count = 2;
    }
    return WriteVectorsAll(fd, vectors, vector_count);
}

bool ReadFrame(int fd, Frame* out) { return ReadFrameInternal(fd, out, nullptr) == FrameReadStatus::Ok; }

bool ColTypeToWire(int col_type, uint8_t* wire_type) {
    if (wire_type == nullptr) {
        return false;
    }
    // Must match defs.h: TYPE_INT=0, TYPE_FLOAT=1, TYPE_STRING=2
    switch (col_type) {
        case 0:
            *wire_type = kTypeInt32;
            return true;
        case 1:
            *wire_type = kTypeFloat32;
            return true;
        case 2:
            *wire_type = kTypeChar;
            return true;
        default:
            return false;
    }
}

bool TryEncodeMeta(const std::vector<ColumnDef>& columns, std::string* payload, std::string* diagnostic) {
    if (payload == nullptr) {
        SetDiagnostic(diagnostic, "null META output");
        return false;
    }
    payload->clear();
    if (columns.empty() || columns.size() > std::numeric_limits<uint16_t>::max()) {
        SetDiagnostic(diagnostic, "META column count is out of range");
        return false;
    }

    size_t encoded_size = sizeof(uint16_t);
    for (const auto& column : columns) {
        const size_t name_size = column.name.size();
        if (name_size == 0 || name_size > std::numeric_limits<uint16_t>::max()) {
            SetDiagnostic(diagnostic, "META column name is out of range");
            return false;
        }
        if (!IsKnownSqlType(column.sql_type)) {
            SetDiagnostic(diagnostic, "META contains an unknown column type");
            return false;
        }
        const size_t column_size = sizeof(uint16_t) + name_size + sizeof(uint8_t);
        if (column_size > kMaxPayloadBytes - encoded_size) {
            SetDiagnostic(diagnostic, "META payload exceeds 1 MiB");
            return false;
        }
        encoded_size += column_size;
    }

    payload->reserve(encoded_size);
    AppendU16(payload, static_cast<uint16_t>(columns.size()));
    for (const auto& column : columns) {
        AppendU16(payload, static_cast<uint16_t>(column.name.size()));
        payload->append(column.name);
        AppendU8(payload, column.sql_type);
    }
    return true;
}

std::string EncodeMeta(const std::vector<ColumnDef>& columns) {
    std::string payload;
    if (!TryEncodeMeta(columns, &payload)) {
        payload.clear();
    }
    return payload;
}

std::string EncodeMetaSingleCharColumn(const std::string& column_name) {
    ColumnDef column;
    column.name = column_name.empty() ? "output" : column_name;
    column.sql_type = kTypeChar;
    return EncodeMeta({column});
}

static void AppendCell(std::string* payload, const Cell& cell) {
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
    } else if (cell.sql_type == kTypeChar) {
        AppendU32(payload, static_cast<uint32_t>(cell.str_val.size()));
        payload->append(cell.str_val);
    }
}

bool TryEncodeRow(const std::vector<Cell>& cells, std::string* payload, std::string* diagnostic) {
    if (payload == nullptr) {
        SetDiagnostic(diagnostic, "null ROW output");
        return false;
    }
    payload->clear();
    if (cells.empty()) {
        SetDiagnostic(diagnostic, "ROW has no cells");
        return false;
    }

    size_t encoded_size = 0;
    for (const auto& cell : cells) {
        if (!IsKnownSqlType(cell.sql_type)) {
            SetDiagnostic(diagnostic, "ROW contains an unknown cell type");
            return false;
        }
        size_t cell_size = sizeof(uint8_t);
        if (!cell.is_null) {
            if (cell.sql_type == kTypeChar) {
                if (cell.str_val.size() > std::numeric_limits<uint32_t>::max()) {
                    SetDiagnostic(diagnostic, "ROW string cell is out of range");
                    return false;
                }
                cell_size += sizeof(uint32_t) + cell.str_val.size();
            } else {
                cell_size += sizeof(uint32_t);
            }
        }
        if (cell_size > kMaxPayloadBytes - encoded_size) {
            SetDiagnostic(diagnostic, "ROW payload exceeds 1 MiB");
            return false;
        }
        encoded_size += cell_size;
    }

    payload->reserve(encoded_size);
    for (const auto& cell : cells) {
        AppendCell(payload, cell);
    }
    return true;
}

std::string EncodeRow(const std::vector<Cell>& cells) {
    std::string payload;
    if (!TryEncodeRow(cells, &payload)) {
        payload.clear();
    }
    return payload;
}

std::string EncodeRowSingleChar(const std::string& value) {
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

static bool DecodeCell(const std::string& payload, size_t* off, uint8_t sql_type, Cell* cell) {
    if (cell == nullptr || !IsKnownSqlType(sql_type)) {
        return false;
    }
    *cell = Cell{};
    cell->sql_type = sql_type;

    uint8_t present = 0;
    if (!ReadU8(payload, off, &present) || present > 1) {
        return false;
    }
    if (present == 0) {
        cell->is_null = true;
        return true;
    }

    uint32_t bits = 0;
    if (sql_type == kTypeInt32) {
        if (!ReadU32(payload, off, &bits)) {
            return false;
        }
        std::memcpy(&cell->int_val, &bits, sizeof(cell->int_val));
        return true;
    }
    if (sql_type == kTypeFloat32) {
        if (!ReadU32(payload, off, &bits)) {
            return false;
        }
        std::memcpy(&cell->float_val, &bits, sizeof(cell->float_val));
        return true;
    }

    uint32_t size = 0;
    return ReadU32(payload, off, &size) && ReadBytes(payload, off, size, &cell->str_val);
}

static std::string CellText(const Cell& cell) {
    if (cell.is_null) {
        return "NULL";
    }
    if (cell.sql_type == kTypeInt32) {
        return std::to_string(cell.int_val);
    }
    if (cell.sql_type == kTypeFloat32) {
        return std::to_string(cell.float_val);
    }
    return cell.str_val;
}

// Match the historical fixed-width CLI layout so golden files stay stable.
static constexpr size_t kPrettyColWidth = 16;

static void AppendTableCell(std::string* out, std::string value) {
    if (value.size() > kPrettyColWidth) {
        size_t prefix_size = kPrettyColWidth - 3;
        while (prefix_size > 0 && prefix_size < value.size() &&
               (static_cast<unsigned char>(value[prefix_size]) & 0xc0u) == 0x80u) {
            --prefix_size;
        }
        value.resize(prefix_size);
        value += "...";
    }
    out->append("| ");
    if (value.size() < kPrettyColWidth) {
        out->append(kPrettyColWidth - value.size(), ' ');
    }
    out->append(value);
    out->push_back(' ');
}

static void AppendSeparator(std::string* out, size_t num_cols) {
    for (size_t i = 0; i < num_cols; ++i) {
        out->push_back('+');
        out->append(kPrettyColWidth + 2, '-');
    }
    out->append("+\n");
}

static void AppendTableHeader(std::string* out, const std::vector<ColumnDef>& columns) {
    AppendSeparator(out, columns.size());
    for (const auto& column : columns) {
        AppendTableCell(out, column.name);
    }
    out->append("|\n");
    AppendSeparator(out, columns.size());
}

static void AppendTableRow(std::string* out, const std::vector<Cell>& row) {
    for (const auto& cell : row) {
        AppendTableCell(out, CellText(cell));
    }
    out->append("|\n");
}

ExecuteResult ExecStreamResult(int fd, const std::string& sql, const ExecuteOptions& options) {
    ExecuteResult result;
    if (fd < 0) {
        result.status = ExecuteStatus::TransportError;
        result.diagnostic = "connection is not open";
        return result;
    }
    if (sql.empty() || sql.size() > kMaxPayloadBytes) {
        result.status = ExecuteStatus::InvalidRequest;
        result.diagnostic = "invalid SQL payload size";
        return result;
    }
    if (!WriteFrame(fd, kTagExecStream, 0, sql)) {
        result.status = ExecuteStatus::TransportError;
        result.diagnostic = std::string("failed to send EXEC_STREAM: ") + std::strerror(errno);
        return result;
    }

    enum class ResponseState { AwaitFirst, Rows };
    ResponseState state = ResponseState::AwaitFirst;
    uint64_t observed_rows = 0;
    bool consumer_stopped = false;
    std::string consumer_diagnostic;
    bool raw_text_result = false;

    while (true) {
        Frame frame;
        std::string read_diagnostic;
        const FrameReadStatus read_status = ReadFrameInternal(fd, &frame, &read_diagnostic);
        if (read_status != FrameReadStatus::Ok) {
            result.status = read_status == FrameReadStatus::ProtocolError ? ExecuteStatus::ProtocolError
                                                                          : ExecuteStatus::TransportError;
            result.diagnostic = std::move(read_diagnostic);
            return result;
        }
        switch (frame.tag) {
            case kTagCommandOk:
                if (frame.flags != 0 || state != ResponseState::AwaitFirst || !frame.payload.empty()) {
                    result.status = ExecuteStatus::ProtocolError;
                    result.diagnostic = "invalid COMMAND_OK";
                    return result;
                }
                result.status = ExecuteStatus::CommandOk;
                return result;

            case kTagError:
            case kTagTransactionAbort:
                if (frame.flags != 0) {
                    result.status = ExecuteStatus::ProtocolError;
                    result.diagnostic = "invalid error response flags";
                    return result;
                }
                result.status = frame.tag == kTagError ? ExecuteStatus::SqlError : ExecuteStatus::TransactionAbort;
                result.text.clear();
                result.diagnostic = std::move(frame.payload);
                return result;

            case kTagMeta: {
                if ((static_cast<unsigned int>(frame.flags) & ~static_cast<unsigned int>(kFlagRawText)) != 0U) {
                    result.status = ExecuteStatus::ProtocolError;
                    result.diagnostic = "invalid META flags";
                    return result;
                }
                if (state != ResponseState::AwaitFirst) {
                    result.status = ExecuteStatus::ProtocolError;
                    result.diagnostic = "duplicate META";
                    return result;
                }
                size_t off = 0;
                uint16_t column_count = 0;
                if (!ReadU16(frame.payload, &off, &column_count) || column_count == 0) {
                    result.status = ExecuteStatus::ProtocolError;
                    result.diagnostic = "invalid META";
                    return result;
                }

                result.columns.clear();
                result.columns.reserve(column_count);
                for (uint16_t i = 0; i < column_count; ++i) {
                    uint16_t name_size = 0;
                    if (!ReadU16(frame.payload, &off, &name_size) || name_size == 0) {
                        result.status = ExecuteStatus::ProtocolError;
                        result.diagnostic = "invalid META column name";
                        return result;
                    }
                    ColumnDef column;
                    if (!ReadBytes(frame.payload, &off, name_size, &column.name)) {
                        result.status = ExecuteStatus::ProtocolError;
                        result.diagnostic = "invalid META column name bytes";
                        return result;
                    }
                    if (!ReadU8(frame.payload, &off, &column.sql_type) || !IsKnownSqlType(column.sql_type)) {
                        result.status = ExecuteStatus::ProtocolError;
                        result.diagnostic = "unknown META column type";
                        return result;
                    }
                    result.columns.push_back(std::move(column));
                }
                if (off != frame.payload.size()) {
                    result.status = ExecuteStatus::ProtocolError;
                    result.diagnostic = "trailing bytes in META";
                    return result;
                }

                state = ResponseState::Rows;
                raw_text_result = (frame.flags & kFlagRawText) != 0;
                if (raw_text_result &&
                    (result.columns.size() != 1 || result.columns[0].sql_type != kTypeChar)) {
                    result.status = ExecuteStatus::ProtocolError;
                    result.diagnostic = "raw text META must contain one CHAR column";
                    return result;
                }
                if (options.on_meta) {
                    try {
                        if (!options.on_meta(result.columns)) {
                            consumer_stopped = true;
                            consumer_diagnostic = "metadata consumer requested stop";
                        }
                    } catch (const std::exception& error) {
                        consumer_stopped = true;
                        consumer_diagnostic = std::string("metadata consumer failed: ") + error.what();
                    } catch (...) {
                        consumer_stopped = true;
                        consumer_diagnostic = "metadata consumer failed";
                    }
                }
                if (options.format_text && !consumer_stopped && !raw_text_result) {
                    AppendTableHeader(&result.text, result.columns);
                }
                break;
            }

            case kTagRow: {
                if (frame.flags != 0 || state != ResponseState::Rows || result.columns.empty()) {
                    result.status = ExecuteStatus::ProtocolError;
                    result.diagnostic = "ROW before META";
                    return result;
                }
                size_t off = 0;
                std::vector<Cell> row;
                row.reserve(result.columns.size());
                for (const auto& column : result.columns) {
                    Cell cell;
                    if (!DecodeCell(frame.payload, &off, column.sql_type, &cell)) {
                        result.status = ExecuteStatus::ProtocolError;
                        result.diagnostic = "invalid ROW cell";
                        return result;
                    }
                    row.push_back(std::move(cell));
                }
                if (off != frame.payload.size()) {
                    result.status = ExecuteStatus::ProtocolError;
                    result.diagnostic = "trailing bytes in ROW";
                    return result;
                }

                ++observed_rows;
                if (!consumer_stopped && options.on_row) {
                    try {
                        if (!options.on_row(row)) {
                            consumer_stopped = true;
                            consumer_diagnostic = "row consumer requested stop";
                        }
                    } catch (const std::exception& error) {
                        consumer_stopped = true;
                        consumer_diagnostic = std::string("row consumer failed: ") + error.what();
                    } catch (...) {
                        consumer_stopped = true;
                        consumer_diagnostic = "row consumer failed";
                    }
                }
                if (options.format_text && !consumer_stopped) {
                    if (raw_text_result) {
                        if (!row[0].is_null) {
                            result.text.append(row[0].str_val);
                        }
                    } else {
                        AppendTableRow(&result.text, row);
                    }
                }
                break;
            }

            case kTagResultEnd: {
                if (frame.flags != 0 || state != ResponseState::Rows) {
                    result.status = ExecuteStatus::ProtocolError;
                    result.diagnostic = "RESULT_END before META";
                    return result;
                }
                size_t off = 0;
                uint64_t declared_rows = 0;
                if (!ReadU64(frame.payload, &off, &declared_rows) || off != frame.payload.size()) {
                    result.status = ExecuteStatus::ProtocolError;
                    result.diagnostic = "invalid RESULT_END";
                    return result;
                }
                if (declared_rows != observed_rows) {
                    result.status = ExecuteStatus::ProtocolError;
                    result.diagnostic = "RESULT_END row count mismatch";
                    return result;
                }

                result.row_count = declared_rows;
                if (options.format_text && !consumer_stopped && !raw_text_result) {
                    AppendSeparator(&result.text, result.columns.size());
                    result.text += "Total record(s): " + std::to_string(declared_rows) + "\n";
                }
                if (consumer_stopped) {
                    result.status = ExecuteStatus::ConsumerStopped;
                    result.diagnostic = std::move(consumer_diagnostic);
                } else {
                    result.status = ExecuteStatus::ResultSet;
                }
                return result;
            }

            default:
                result.status = ExecuteStatus::ProtocolError;
                result.diagnostic = "unknown response tag";
                return result;
        }
    }
}

bool ExecStream(int fd, const std::string& sql, std::string* response_text, std::string* diagnostic) {
    ExecuteOptions options;
    options.format_text = response_text != nullptr;
    ExecuteResult result = ExecStreamResult(fd, sql, options);
    if (response_text != nullptr) {
        *response_text = std::move(result.text);
    }
    if (diagnostic != nullptr) {
        *diagnostic = std::move(result.diagnostic);
    }
    return result.ok();
}

}  // namespace rucbase::wire
