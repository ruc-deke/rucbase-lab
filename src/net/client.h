// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "net/wire.h"

namespace rucbase::wire {

enum class EndpointKind { Tcp, Unix };

struct Endpoint {
    EndpointKind kind = EndpointKind::Tcp;
    std::string host = "127.0.0.1";
    uint16_t port = 8765;
    std::string path;

    static Endpoint Tcp(std::string host = "127.0.0.1", uint16_t port = 8765);
    static Endpoint Unix(std::string path);
};

struct ClientOptions {
    uint32_t connect_timeout_ms = 5000;
    uint32_t io_timeout_ms = kDefaultIoTimeoutMs;
};

enum class ClientError {
    None,
    InvalidEndpoint,
    ResolveFailed,
    ConnectFailed,
    ConnectTimeout,
    ConfigureFailed,
    HandshakeFailed,
};

struct ClientStatus {
    ClientError code = ClientError::None;
    std::string message;

    bool ok() const noexcept { return code == ClientError::None; }
    explicit operator bool() const noexcept { return ok(); }
};

// The stable student-facing API. Client owns the socket, performs the wire
// handshake, serializes concurrent Execute calls, and closes a poisoned
// connection after transport/protocol failures.
class Client {
public:
    Client();
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&& other) noexcept;
    Client& operator=(Client&& other) noexcept;

    ClientStatus Connect(const Endpoint& endpoint, const ClientOptions& options = ClientOptions{});
    ClientStatus ConnectTcp(const std::string& host, uint16_t port, const ClientOptions& options = ClientOptions{});
    ClientStatus ConnectUnix(const std::string& path, const ClientOptions& options = ClientOptions{});

    ExecuteResult Execute(const std::string& sql, const ExecuteOptions& options = ExecuteOptions{});

    void Close() noexcept;
    bool is_open() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rucbase::wire
