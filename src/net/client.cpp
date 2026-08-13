// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "net/client.h"

#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <limits>
#include <mutex>
#include <utility>

namespace rucbase::wire {
namespace {

class UniqueFd {
public:
    UniqueFd() = default;
    explicit UniqueFd(int fd) : fd_(fd) {}
    ~UniqueFd() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    UniqueFd(UniqueFd&& other) noexcept : fd_(other.Release()) {}
    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            Reset(other.Release());
        }
        return *this;
    }

    int get() const noexcept { return fd_; }

    int Release() noexcept {
        const int fd = fd_;
        fd_ = -1;
        return fd;
    }

    void Reset(int fd = -1) noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        fd_ = fd;
    }

private:
    int fd_ = -1;
};

ClientStatus Error(ClientError code, std::string message) {
    ClientStatus status;
    status.code = code;
    status.message = std::move(message);
    return status;
}

std::string ErrnoMessage(const std::string& prefix, int error_number) {
    return prefix + ": " + std::strerror(error_number);
}

ClientStatus ConnectAddress(const sockaddr* address,
                            socklen_t address_length,
                            std::chrono::steady_clock::time_point deadline,
                            UniqueFd* connected) {
    UniqueFd candidate(::socket(address->sa_family, SOCK_STREAM, 0));
    if (candidate.get() < 0) {
        return Error(ClientError::ConnectFailed, ErrnoMessage("socket creation failed", errno));
    }

    const int original_flags = ::fcntl(candidate.get(), F_GETFL);
    if (original_flags < 0) {
        return Error(ClientError::ConnectFailed, ErrnoMessage("failed to read socket flags", errno));
    }
    const int nonblocking_flags =
        static_cast<int>(static_cast<unsigned int>(original_flags) | static_cast<unsigned int>(O_NONBLOCK));
    if (::fcntl(candidate.get(), F_SETFL, nonblocking_flags) < 0) {
        return Error(ClientError::ConnectFailed, ErrnoMessage("failed to configure nonblocking connect", errno));
    }

    if (::connect(candidate.get(), address, address_length) != 0) {
        if (errno != EINPROGRESS) {
            return Error(ClientError::ConnectFailed, ErrnoMessage("connect failed", errno));
        }

        while (true) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                return Error(ClientError::ConnectTimeout, "connect timed out");
            }
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
            const int timeout_ms = static_cast<int>(std::max<int64_t>(1, remaining.count()));
            pollfd descriptor{.fd = candidate.get(), .events = POLLOUT, .revents = 0};
            const int poll_result = ::poll(&descriptor, 1, timeout_ms);
            if (poll_result < 0 && errno == EINTR) {
                continue;
            }
            if (poll_result == 0) {
                return Error(ClientError::ConnectTimeout, "connect timed out");
            }
            if (poll_result < 0) {
                return Error(ClientError::ConnectFailed, ErrnoMessage("connect poll failed", errno));
            }

            int socket_error = 0;
            socklen_t error_size = sizeof(socket_error);
            if (::getsockopt(candidate.get(), SOL_SOCKET, SO_ERROR, &socket_error, &error_size) != 0) {
                return Error(ClientError::ConnectFailed, ErrnoMessage("connect status failed", errno));
            }
            if (socket_error != 0) {
                return Error(ClientError::ConnectFailed, ErrnoMessage("connect failed", socket_error));
            }
            break;
        }
    }

    if (::fcntl(candidate.get(), F_SETFL, original_flags) < 0) {
        return Error(ClientError::ConnectFailed, ErrnoMessage("failed to restore socket flags", errno));
    }
    *connected = std::move(candidate);
    return {};
}

ClientStatus ConnectTcpSocket(const Endpoint& endpoint, const ClientOptions& options, UniqueFd* connected) {
    if (endpoint.host.empty() || endpoint.port == 0) {
        return Error(ClientError::InvalidEndpoint, "TCP host and port must be non-empty");
    }

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* addresses = nullptr;
    const std::string service = std::to_string(endpoint.port);
    const int resolve_result = ::getaddrinfo(endpoint.host.c_str(), service.c_str(), &hints, &addresses);
    if (resolve_result != 0) {
        return Error(ClientError::ResolveFailed,
                     "failed to resolve " + endpoint.host + ":" + service + ": " + ::gai_strerror(resolve_result));
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(options.connect_timeout_ms);
    ClientStatus last_error = Error(ClientError::ConnectFailed, "no usable address");
    for (addrinfo* address = addresses; address != nullptr; address = address->ai_next) {
        last_error = ConnectAddress(address->ai_addr, static_cast<socklen_t>(address->ai_addrlen), deadline, connected);
        if (last_error.ok() || last_error.code == ClientError::ConnectTimeout) {
            break;
        }
    }
    ::freeaddrinfo(addresses);
    return last_error;
}

ClientStatus ConnectUnixSocket(const Endpoint& endpoint, const ClientOptions& options, UniqueFd* connected) {
    if (endpoint.path.empty()) {
        return Error(ClientError::InvalidEndpoint, "Unix socket path must be non-empty");
    }

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    if (endpoint.path.size() >= sizeof(address.sun_path)) {
        return Error(ClientError::InvalidEndpoint, "Unix socket path is too long");
    }
    std::memcpy(address.sun_path, endpoint.path.c_str(), endpoint.path.size() + 1);
    const auto address_length = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + endpoint.path.size() + 1);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(options.connect_timeout_ms);
    return ConnectAddress(reinterpret_cast<const sockaddr*>(&address), address_length, deadline, connected);
}

}  // namespace

struct Client::Impl {
    ~Impl() {
        if (fd >= 0) {
            ::close(fd);
        }
    }

    mutable std::mutex mutex;
    int fd = -1;
};

Endpoint Endpoint::Tcp(std::string host, uint16_t port) {
    Endpoint endpoint;
    endpoint.kind = EndpointKind::Tcp;
    endpoint.host = std::move(host);
    endpoint.port = port;
    endpoint.path.clear();
    return endpoint;
}

Endpoint Endpoint::Unix(std::string path) {
    Endpoint endpoint;
    endpoint.kind = EndpointKind::Unix;
    endpoint.host.clear();
    endpoint.port = 0;
    endpoint.path = std::move(path);
    return endpoint;
}

Client::Client() : impl_(std::make_unique<Impl>()) {}

Client::~Client() { Close(); }

Client::Client(Client&& other) noexcept = default;

Client& Client::operator=(Client&& other) noexcept = default;

ClientStatus Client::Connect(const Endpoint& endpoint, const ClientOptions& options) {
    if (options.connect_timeout_ms == 0 ||
        options.connect_timeout_ms > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
        options.io_timeout_ms == 0) {
        return Error(ClientError::InvalidEndpoint, "connect and I/O timeouts are out of range");
    }
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }

    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->fd >= 0) {
        ::shutdown(impl_->fd, SHUT_RDWR);
        ::close(impl_->fd);
        impl_->fd = -1;
    }

    UniqueFd connected;
    ClientStatus status = endpoint.kind == EndpointKind::Tcp ? ConnectTcpSocket(endpoint, options, &connected)
                                                             : ConnectUnixSocket(endpoint, options, &connected);
    if (!status.ok()) {
        return status;
    }
    if (!ConfigureConnectedSocket(connected.get(), options.io_timeout_ms)) {
        return Error(ClientError::ConfigureFailed, ErrnoMessage("failed to configure connected socket", errno));
    }
    if (!ClientHandshake(connected.get())) {
        return Error(ClientError::HandshakeFailed, "wire handshake failed; server must speak RMDB wire protocol v3.1");
    }

    impl_->fd = connected.Release();
    return {};
}

ClientStatus Client::ConnectTcp(const std::string& host, uint16_t port, const ClientOptions& options) {
    return Connect(Endpoint::Tcp(host, port), options);
}

ClientStatus Client::ConnectUnix(const std::string& path, const ClientOptions& options) {
    return Connect(Endpoint::Unix(path), options);
}

ExecuteResult Client::Execute(const std::string& sql, const ExecuteOptions& options) {
    if (!impl_) {
        ExecuteResult result;
        result.status = ExecuteStatus::TransportError;
        result.diagnostic = "connection is not open";
        return result;
    }

    std::lock_guard<std::mutex> lock(impl_->mutex);
    ExecuteResult result = ExecStreamResult(impl_->fd, sql, options);
    if (!result.connection_reusable() && impl_->fd >= 0) {
        ::shutdown(impl_->fd, SHUT_RDWR);
        ::close(impl_->fd);
        impl_->fd = -1;
    }
    return result;
}

void Client::Close() noexcept {
    if (!impl_) {
        return;
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->fd >= 0) {
        ::shutdown(impl_->fd, SHUT_RDWR);
        ::close(impl_->fd);
        impl_->fd = -1;
    }
}

bool Client::is_open() const noexcept {
    if (!impl_) {
        return false;
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->fd >= 0;
}

}  // namespace rucbase::wire
