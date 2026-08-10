// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cstring>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "net/client.h"

namespace {

class SocketPair {
   public:
    SocketPair() { EXPECT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds_.data()), 0); }

    ~SocketPair() {
        for (int fd : fds_) {
            if (fd >= 0) {
                close(fd);
            }
        }
    }

    int client() const { return fds_[0]; }
    int server() const { return fds_[1]; }

    void CloseClient() {
        close(fds_[0]);
        fds_[0] = -1;
    }

   private:
    std::array<int, 2> fds_{{-1, -1}};
};

struct ResponseFrame {
    ResponseFrame(const uint8_t tag_value, std::string payload_value, const uint8_t flag_value = 0)
        : tag(tag_value), payload(std::move(payload_value)), flags(flag_value) {}

    uint8_t tag;
    std::string payload;
    uint8_t flags = 0;
};

bool RunExecStream(const std::vector<ResponseFrame>& responses, std::string* text, std::string* diagnostic) {
    SocketPair sockets;
    for (const auto& response : responses) {
        if (!rucbase::wire::WriteFrame(sockets.server(), response.tag, response.flags, response.payload)) {
            return false;
        }
    }
    return rucbase::wire::ExecStream(sockets.client(), "select 1;", text, diagnostic);
}

rucbase::wire::ExecuteResult RunExecStreamResult(
    const std::vector<ResponseFrame>& responses,
    const rucbase::wire::ExecuteOptions& options = rucbase::wire::ExecuteOptions{}) {
    SocketPair sockets;
    for (const auto& response : responses) {
        if (!rucbase::wire::WriteFrame(sockets.server(), response.tag, response.flags, response.payload)) {
            rucbase::wire::ExecuteResult result;
            result.status = rucbase::wire::ExecuteStatus::TransportError;
            result.diagnostic = "test response write failed";
            return result;
        }
    }
    return rucbase::wire::ExecStreamResult(sockets.client(), "select 1;", options);
}

std::string OneCharMeta() { return rucbase::wire::EncodeMetaSingleCharColumn("value"); }

}  // namespace

TEST(WireHandshakeTest, WritesStableVersionBytes) {
    SocketPair sockets;
    ASSERT_TRUE(rucbase::wire::WriteHandshake(sockets.client()));

    std::array<uint8_t, 8> actual{};
    ASSERT_TRUE(rucbase::wire::ReadExact(sockets.server(), actual.data(), actual.size()));
    const std::array<uint8_t, 8> expected{{'R', 'U', 'C', 'B', 0, 3, 0, 1}};
    EXPECT_EQ(actual, expected);
}

TEST(WireHandshakeTest, ClientAndServerCompleteHandshake) {
    SocketPair sockets;
    std::atomic<bool> server_ok{false};
    std::thread server([&] { server_ok = rucbase::wire::ReadAndCheckHandshake(sockets.server()); });
    const bool client_ok = rucbase::wire::ClientHandshake(sockets.client());
    server.join();

    EXPECT_TRUE(client_ok);
    EXPECT_TRUE(server_ok.load());
}

TEST(WireFrameTest, RoundTripsBinaryPayload) {
    SocketPair sockets;
    const std::string payload("a\0b", 3);
    ASSERT_TRUE(rucbase::wire::WriteFrame(sockets.client(), rucbase::wire::kTagExecStream, 0, payload));

    rucbase::wire::Frame frame;
    ASSERT_TRUE(rucbase::wire::ReadFrame(sockets.server(), &frame));
    EXPECT_EQ(frame.tag, rucbase::wire::kTagExecStream);
    EXPECT_EQ(frame.flags, 0);
    EXPECT_EQ(frame.payload, payload);
}

TEST(WireFrameTest, WritesStableHeaderBytes) {
    SocketPair sockets;
    ASSERT_TRUE(rucbase::wire::WriteFrame(sockets.client(), rucbase::wire::kTagExecStream, 0, "abc"));

    std::array<uint8_t, 11> actual{};
    ASSERT_TRUE(rucbase::wire::ReadExact(sockets.server(), actual.data(), actual.size()));
    const std::array<uint8_t, 11> expected{{0, 0, 0, 3, rucbase::wire::kTagExecStream, 0, 0, 0, 'a', 'b', 'c'}};
    EXPECT_EQ(actual, expected);
}

TEST(WireFrameTest, RejectsNonZeroReservedField) {
    SocketPair sockets;
    const std::array<uint8_t, 8> header{{0, 0, 0, 0, rucbase::wire::kTagExecStream, 0, 0, 1}};
    ASSERT_TRUE(rucbase::wire::WriteAll(sockets.client(), header.data(), header.size()));

    rucbase::wire::Frame frame;
    EXPECT_FALSE(rucbase::wire::ReadFrame(sockets.server(), &frame));
}

TEST(WireFrameTest, RejectsOversizedPayloadBeforeAllocation) {
    SocketPair sockets;
    std::array<uint8_t, 8> header{};
    const uint32_t oversized = htonl(rucbase::wire::kMaxPayloadBytes + 1);
    std::memcpy(header.data(), &oversized, sizeof(oversized));
    header[4] = rucbase::wire::kTagExecStream;
    ASSERT_TRUE(rucbase::wire::WriteAll(sockets.client(), header.data(), header.size()));

    rucbase::wire::Frame frame;
    EXPECT_FALSE(rucbase::wire::ReadFrame(sockets.server(), &frame));
}

TEST(WireFrameTest, ClosedDescriptorIsAWriteError) {
    SocketPair sockets;
    // A peer close is observed asynchronously on stream sockets: Windows /
    // MSYS2 may accept one small write after the peer has sent FIN.  Use a
    // closed descriptor to exercise the same WriteAll error path
    // deterministically on every supported platform.
    sockets.CloseClient();
    const char byte = 'x';
    EXPECT_FALSE(rucbase::wire::WriteAll(sockets.client(), &byte, 1));
}

TEST(WireFrameTest, RejectsOversizedDiagnostic) {
    SocketPair sockets;
    const std::string diagnostic(rucbase::wire::kMaxDiagnosticBytes + 1, 'x');
    EXPECT_FALSE(rucbase::wire::WriteFrame(sockets.client(), rucbase::wire::kTagError, 0, diagnostic));
}

TEST(WireFrameTest, ReadHonorsConfiguredTimeout) {
    SocketPair sockets;
    EXPECT_FALSE(rucbase::wire::ConfigureConnectedSocket(sockets.client(), 0));
    ASSERT_TRUE(rucbase::wire::ConfigureConnectedSocket(sockets.client(), 20));

    rucbase::wire::Frame frame;
    EXPECT_FALSE(rucbase::wire::ReadFrame(sockets.client(), &frame));
}

TEST(WireEncodingTest, MetaHasStableBytes) {
    rucbase::wire::ColumnDef column;
    column.name = "value";
    column.sql_type = rucbase::wire::kTypeChar;

    std::string payload;
    std::string diagnostic;
    ASSERT_TRUE(rucbase::wire::TryEncodeMeta({column}, &payload, &diagnostic));
    const std::string expected("\x00\x01\x00\x05value\x03", 10);
    EXPECT_EQ(payload, expected);
    EXPECT_TRUE(diagnostic.empty());
}

TEST(WireEncodingTest, RowHasStableTypedBytes) {
    rucbase::wire::Cell integer;
    integer.sql_type = rucbase::wire::kTypeInt32;
    integer.int_val = -1;

    rucbase::wire::Cell floating;
    floating.sql_type = rucbase::wire::kTypeFloat32;
    floating.float_val = 1.0f;

    rucbase::wire::Cell text;
    text.sql_type = rucbase::wire::kTypeChar;
    text.str_val = "x";

    std::string payload;
    ASSERT_TRUE(rucbase::wire::TryEncodeRow({integer, floating, text}, &payload));
    const std::string expected("\x01\xff\xff\xff\xff\x01\x3f\x80\x00\x00\x01\x00\x00\x00\x01x", 16);
    EXPECT_EQ(payload, expected);
}

TEST(WireEncodingTest, NullCellOnlyCarriesPresenceByte) {
    rucbase::wire::Cell cell;
    cell.is_null = true;
    cell.sql_type = rucbase::wire::kTypeInt32;
    cell.int_val = 123;

    std::string payload;
    ASSERT_TRUE(rucbase::wire::TryEncodeRow({cell}, &payload));
    EXPECT_EQ(payload, std::string("\0", 1));
}

TEST(WireEncodingTest, RejectsUnknownTypesAndOversizedNames) {
    rucbase::wire::ColumnDef unknown;
    unknown.name = "value";
    unknown.sql_type = 0xff;
    std::string payload;
    std::string diagnostic;
    EXPECT_FALSE(rucbase::wire::TryEncodeMeta({unknown}, &payload, &diagnostic));
    EXPECT_FALSE(diagnostic.empty());

    rucbase::wire::ColumnDef oversized;
    oversized.name.assign(65536, 'x');
    oversized.sql_type = rucbase::wire::kTypeChar;
    EXPECT_FALSE(rucbase::wire::TryEncodeMeta({oversized}, &payload, &diagnostic));

    uint8_t wire_type = 0;
    EXPECT_FALSE(rucbase::wire::ColTypeToWire(99, &wire_type));
}

TEST(WireExecStreamTest, AcceptsValidTypedResponse) {
    rucbase::wire::Cell cell;
    cell.sql_type = rucbase::wire::kTypeChar;
    cell.str_val = "hello";

    std::string text;
    std::string diagnostic;
    EXPECT_TRUE(RunExecStream({{rucbase::wire::kTagMeta, OneCharMeta()},
                               {rucbase::wire::kTagRow, rucbase::wire::EncodeRow({cell})},
                               {rucbase::wire::kTagResultEnd, rucbase::wire::EncodeResultEnd(1)}},
                              &text, &diagnostic));
    EXPECT_TRUE(diagnostic.empty());
    EXPECT_NE(text.find("hello"), std::string::npos);
}

TEST(WireExecStreamTest, RawTextRequiresExplicitMetaFlag) {
    rucbase::wire::Cell cell;
    cell.sql_type = rucbase::wire::kTypeChar;
    cell.str_val = "hello";

    const auto ordinary = RunExecStreamResult(
        {{rucbase::wire::kTagMeta, rucbase::wire::EncodeMetaSingleCharColumn("output")},
         {rucbase::wire::kTagRow, rucbase::wire::EncodeRow({cell})},
         {rucbase::wire::kTagResultEnd, rucbase::wire::EncodeResultEnd(1)}});
    EXPECT_TRUE(ordinary.ok());
    EXPECT_NE(ordinary.text.find("Total record(s): 1"), std::string::npos);

    const auto raw = RunExecStreamResult(
        {{rucbase::wire::kTagMeta, rucbase::wire::EncodeMetaSingleCharColumn("output"),
          rucbase::wire::kFlagRawText},
         {rucbase::wire::kTagRow, rucbase::wire::EncodeRow({cell})},
         {rucbase::wire::kTagResultEnd, rucbase::wire::EncodeResultEnd(1)}});
    EXPECT_TRUE(raw.ok());
    EXPECT_EQ(raw.text, "hello");
}

TEST(WireExecStreamTest, RejectsInvalidRawTextFlags) {
    const auto unknown_flag = RunExecStreamResult(
        {{rucbase::wire::kTagMeta, rucbase::wire::EncodeMetaSingleCharColumn("output"), 0x80}});
    EXPECT_EQ(unknown_flag.status, rucbase::wire::ExecuteStatus::ProtocolError);

    const auto non_char_schema = RunExecStreamResult(
        {{rucbase::wire::kTagMeta,
          rucbase::wire::EncodeMeta(
              {{.name = "id", .sql_type = rucbase::wire::kTypeInt32}}),
          rucbase::wire::kFlagRawText}});
    EXPECT_EQ(non_char_schema.status, rucbase::wire::ExecuteStatus::ProtocolError);
}

TEST(WireExecStreamTest, StreamsRowsWithoutCollectingText) {
    rucbase::wire::Cell first;
    first.sql_type = rucbase::wire::kTypeChar;
    first.str_val = "first";
    rucbase::wire::Cell second = first;
    second.str_val = "second";

    size_t meta_calls = 0;
    std::vector<std::string> values;
    rucbase::wire::ExecuteOptions options;
    options.format_text = false;
    options.on_meta = [&](const std::vector<rucbase::wire::ColumnDef>& columns) {
        ++meta_calls;
        return columns.size() == 1 && columns[0].name == "value";
    };
    options.on_row = [&](const std::vector<rucbase::wire::Cell>& row) {
        values.push_back(row.at(0).str_val);
        return true;
    };

    const auto result = RunExecStreamResult({{rucbase::wire::kTagMeta, OneCharMeta()},
                                             {rucbase::wire::kTagRow, rucbase::wire::EncodeRow({first})},
                                             {rucbase::wire::kTagRow, rucbase::wire::EncodeRow({second})},
                                             {rucbase::wire::kTagResultEnd, rucbase::wire::EncodeResultEnd(2)}},
                                            options);
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.row_count, 2);
    EXPECT_TRUE(result.text.empty());
    EXPECT_EQ(meta_calls, 1);
    EXPECT_EQ(values, (std::vector<std::string>{"first", "second"}));
}

TEST(WireExecStreamTest, StoppedConsumerIsDrainedAndConnectionRemainsReusable) {
    SocketPair sockets;
    rucbase::wire::Cell cell;
    cell.sql_type = rucbase::wire::kTypeChar;
    cell.str_val = "unformatted-row";
    ASSERT_TRUE(rucbase::wire::WriteFrame(sockets.server(), rucbase::wire::kTagMeta, 0, OneCharMeta()));
    ASSERT_TRUE(
        rucbase::wire::WriteFrame(sockets.server(), rucbase::wire::kTagRow, 0, rucbase::wire::EncodeRow({cell})));
    ASSERT_TRUE(rucbase::wire::WriteFrame(sockets.server(), rucbase::wire::kTagResultEnd, 0,
                                          rucbase::wire::EncodeResultEnd(1)));
    ASSERT_TRUE(rucbase::wire::WriteFrame(sockets.server(), rucbase::wire::kTagCommandOk, 0, ""));

    rucbase::wire::ExecuteOptions options;
    options.format_text = true;
    options.on_row = [](const std::vector<rucbase::wire::Cell>&) { return false; };
    const auto stopped = rucbase::wire::ExecStreamResult(sockets.client(), "select 1;", options);
    EXPECT_EQ(stopped.status, rucbase::wire::ExecuteStatus::ConsumerStopped);
    EXPECT_TRUE(stopped.connection_reusable());
    EXPECT_EQ(stopped.text.find("unformatted-row"), std::string::npos);

    const auto next = rucbase::wire::ExecStreamResult(sockets.client(), "begin;");
    EXPECT_TRUE(next.ok());
    EXPECT_EQ(next.status, rucbase::wire::ExecuteStatus::CommandOk);
}

TEST(WireExecStreamTest, ServerErrorLeavesConnectionSynchronized) {
    SocketPair sockets;
    ASSERT_TRUE(rucbase::wire::WriteFrame(sockets.server(), rucbase::wire::kTagError, 0, "bad SQL"));
    ASSERT_TRUE(rucbase::wire::WriteFrame(sockets.server(), rucbase::wire::kTagCommandOk, 0, ""));

    const auto error_result = rucbase::wire::ExecStreamResult(sockets.client(), "bad;");
    EXPECT_EQ(error_result.status, rucbase::wire::ExecuteStatus::SqlError);
    EXPECT_TRUE(error_result.connection_reusable());
    EXPECT_EQ(error_result.diagnostic, "bad SQL");

    const auto next_result = rucbase::wire::ExecStreamResult(sockets.client(), "begin;");
    EXPECT_EQ(next_result.status, rucbase::wire::ExecuteStatus::CommandOk);
    EXPECT_TRUE(next_result.ok());
}

TEST(WireExecStreamTest, RejectsDuplicateMeta) {
    std::string text;
    std::string diagnostic;
    EXPECT_FALSE(RunExecStream({{rucbase::wire::kTagMeta, OneCharMeta()}, {rucbase::wire::kTagMeta, OneCharMeta()}},
                               &text, &diagnostic));
    EXPECT_EQ(diagnostic, "duplicate META");
}

TEST(WireExecStreamTest, RejectsTrailingPayloadBytes) {
    std::string meta = OneCharMeta();
    meta.push_back('\0');

    std::string text;
    std::string diagnostic;
    EXPECT_FALSE(RunExecStream({{rucbase::wire::kTagMeta, std::move(meta)}}, &text, &diagnostic));
    EXPECT_EQ(diagnostic, "trailing bytes in META");
}

TEST(WireExecStreamTest, RejectsResultEndBeforeMeta) {
    std::string text;
    std::string diagnostic;
    EXPECT_FALSE(
        RunExecStream({{rucbase::wire::kTagResultEnd, rucbase::wire::EncodeResultEnd(0)}}, &text, &diagnostic));
    EXPECT_EQ(diagnostic, "RESULT_END before META");
}

TEST(WireExecStreamTest, RejectsResultRowCountMismatch) {
    std::string text;
    std::string diagnostic;
    EXPECT_FALSE(RunExecStream(
        {{rucbase::wire::kTagMeta, OneCharMeta()}, {rucbase::wire::kTagResultEnd, rucbase::wire::EncodeResultEnd(1)}},
        &text, &diagnostic));
    EXPECT_EQ(diagnostic, "RESULT_END row count mismatch");
}

TEST(WireExecStreamTest, RejectsCommandOkAfterMeta) {
    std::string text;
    std::string diagnostic;
    EXPECT_FALSE(RunExecStream({{rucbase::wire::kTagMeta, OneCharMeta()}, {rucbase::wire::kTagCommandOk, ""}}, &text,
                               &diagnostic));
    EXPECT_EQ(diagnostic, "invalid COMMAND_OK");
}

TEST(WireClientTest, ConnectsHandshakesExecutesAndCloses) {
    const int listener = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(listener, 0);
    const int reuse_address = 1;
    ASSERT_EQ(::setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse_address, sizeof(reuse_address)), 0);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    ASSERT_EQ(::bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)), 0);
    ASSERT_EQ(::listen(listener, 1), 0);
    socklen_t address_size = sizeof(address);
    ASSERT_EQ(::getsockname(listener, reinterpret_cast<sockaddr*>(&address), &address_size), 0);
    const uint16_t port = ntohs(address.sin_port);

    std::atomic<bool> server_ok{false};
    std::thread server([&] {
        const int accepted = ::accept(listener, nullptr, nullptr);
        if (accepted >= 0 && rucbase::wire::ConfigureConnectedSocket(accepted, 1000) &&
            rucbase::wire::ReadAndCheckHandshake(accepted)) {
            rucbase::wire::Frame request;
            if (rucbase::wire::ReadFrame(accepted, &request) && request.tag == rucbase::wire::kTagExecStream &&
                request.payload == "begin;" &&
                rucbase::wire::WriteFrame(accepted, rucbase::wire::kTagCommandOk, 0, "")) {
                server_ok = true;
            }
        }
        if (accepted >= 0) {
            ::close(accepted);
        }
    });

    rucbase::wire::Client client;
    const auto connect_status = client.ConnectTcp("127.0.0.1", port);
    rucbase::wire::ExecuteResult execute_result;
    if (connect_status.ok()) {
        execute_result = client.Execute("begin;");
    }
    client.Close();
    server.join();
    ::close(listener);

    EXPECT_TRUE(connect_status.ok()) << connect_status.message;
    EXPECT_TRUE(execute_result.ok()) << execute_result.diagnostic;
    EXPECT_TRUE(server_ok.load());
    EXPECT_FALSE(client.is_open());
}
