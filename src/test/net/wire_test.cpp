#include "net/wire.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

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

    void CloseServer() {
        close(fds_[1]);
        fds_[1] = -1;
    }

   private:
    std::array<int, 2> fds_{{-1, -1}};
};

struct ResponseFrame {
    uint8_t tag;
    std::string payload;
};

bool RunExecStream(const std::vector<ResponseFrame> &responses, std::string *text, std::string *diagnostic) {
    SocketPair sockets;
    for (const auto &response : responses) {
        if (!rucbase::wire::WriteFrame(sockets.server(), response.tag, 0, response.payload)) {
            return false;
        }
    }
    return rucbase::wire::ExecStream(sockets.client(), "select 1;", text, diagnostic);
}

std::string OneCharMeta() { return rucbase::wire::EncodeMetaSingleCharColumn("value"); }

}  // namespace

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

TEST(WireFrameTest, PeerDisconnectIsAWriteError) {
    SocketPair sockets;
    sockets.CloseServer();
    const char byte = 'x';
    EXPECT_FALSE(rucbase::wire::WriteAll(sockets.client(), &byte, 1));
}

TEST(WireFrameTest, RejectsOversizedDiagnostic) {
    SocketPair sockets;
    const std::string diagnostic(rucbase::wire::kMaxDiagnosticBytes + 1, 'x');
    EXPECT_FALSE(rucbase::wire::WriteFrame(sockets.client(), rucbase::wire::kTagError, 0, diagnostic));
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

TEST(WireExecStreamTest, RejectsDuplicateMeta) {
    std::string text;
    std::string diagnostic;
    EXPECT_FALSE(RunExecStream({{rucbase::wire::kTagMeta, OneCharMeta()},
                                {rucbase::wire::kTagMeta, OneCharMeta()}},
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
    EXPECT_FALSE(RunExecStream(
        {{rucbase::wire::kTagResultEnd, rucbase::wire::EncodeResultEnd(0)}}, &text, &diagnostic));
    EXPECT_EQ(diagnostic, "RESULT_END before META");
}

TEST(WireExecStreamTest, RejectsResultRowCountMismatch) {
    std::string text;
    std::string diagnostic;
    EXPECT_FALSE(RunExecStream({{rucbase::wire::kTagMeta, OneCharMeta()},
                                {rucbase::wire::kTagResultEnd, rucbase::wire::EncodeResultEnd(1)}},
                               &text, &diagnostic));
    EXPECT_EQ(diagnostic, "RESULT_END row count mismatch");
}

TEST(WireExecStreamTest, RejectsCommandOkAfterMeta) {
    std::string text;
    std::string diagnostic;
    EXPECT_FALSE(RunExecStream({{rucbase::wire::kTagMeta, OneCharMeta()},
                                {rucbase::wire::kTagCommandOk, ""}},
                               &text, &diagnostic));
    EXPECT_EQ(diagnostic, "invalid COMMAND_OK");
}
