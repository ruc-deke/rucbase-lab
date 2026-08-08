ARG UBUNTU_IMAGE=docker.m.daocloud.io/library/ubuntu:24.04@sha256:561618e2c15bf2397621dd04f96926663a3b5616c189cf7e38db7e82f5c538ea

FROM ${UBUNTU_IMAGE}

ARG NODE_MAJOR=22
ARG UBUNTU_MIRROR=http://mirrors.tuna.tsinghua.edu.cn
ARG RUCBASE_REPOSITORY=https://github.com/ruc-deke/rucbase-lab.git
ARG RUCBASE_REF=main
ARG BUILD_DATE
ARG VCS_REF

LABEL org.opencontainers.image.title="Rucbase development environment" \
      org.opencontainers.image.description="Ubuntu 24.04 teaching environment for Rucbase" \
      org.opencontainers.image.source="${RUCBASE_REPOSITORY}" \
      org.opencontainers.image.revision="${VCS_REF}" \
      org.opencontainers.image.created="${BUILD_DATE}" \
      org.opencontainers.image.version="24.04"

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8

# Use architecture-specific Ubuntu mirrors so builds remain practical on
# domestic networks. Package signatures are still checked by apt.
RUN architecture="$(dpkg --print-architecture)" \
    && case "${architecture}" in \
        amd64) repository="${UBUNTU_MIRROR}/ubuntu/" ;; \
        arm64) repository="${UBUNTU_MIRROR}/ubuntu-ports/" ;; \
        *) echo "Unsupported architecture: ${architecture}" >&2; exit 1 ;; \
    esac \
    && sed -i -E \
        "s#https?://(archive|security).ubuntu.com/ubuntu/?#${repository}#g; s#https?://ports.ubuntu.com/ubuntu-ports/?#${repository}#g" \
        /etc/apt/sources.list.d/ubuntu.sources

# Rucbase toolchain plus common debugging and command-line tools used in labs.
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        bison \
        build-essential \
        ca-certificates \
        ccache \
        clang \
        clang-format \
        clang-tidy \
        cmake \
        curl \
        file \
        flex \
        gdb \
        git \
        gnupg \
        jq \
        less \
        lsb-release \
        nano \
        ninja-build \
        openssh-client \
        procps \
        python-is-python3 \
        python3 \
        python3-pip \
        python3-pytest \
        python3-venv \
        ripgrep \
        sudo \
        valgrind \
        vim \
        wget \
    && rm -rf /var/lib/apt/lists/*

# Node.js 22 is required by current Claude Code npm releases and is also used by
# the Codex npm package. NodeSource publishes packages for amd64 and arm64.
RUN install -d -m 0755 /etc/apt/keyrings \
    && curl -fsSL https://deb.nodesource.com/gpgkey/nodesource-repo.gpg.key \
        -o /tmp/nodesource-repo.gpg.key \
    && gpg --dearmor --yes \
        --output /etc/apt/keyrings/nodesource.gpg \
        /tmp/nodesource-repo.gpg.key \
    && echo "deb [signed-by=/etc/apt/keyrings/nodesource.gpg] https://deb.nodesource.com/node_${NODE_MAJOR}.x nodistro main" \
        > /etc/apt/sources.list.d/nodesource.list \
    && apt-get -o Acquire::Retries=5 update \
    && apt-get -o Acquire::Retries=10 install -y --no-install-recommends nodejs \
    && rm -f /tmp/nodesource-repo.gpg.key \
    && rm -rf /var/lib/apt/lists/*

# Install the newest CLI releases available when the image is built. Never bake
# account credentials or API keys into this image.
#
# Codex ships platform binaries as npm optionalDependencies. Multi-arch buildx
# (especially amd64 under QEMU on Apple Silicon) may skip them when the host
# arch differs, so force linux + the target cpu so the correct package lands.
RUN architecture="$(dpkg --print-architecture)" \
    && case "${architecture}" in \
        amd64) npm_cpu=x64 ;; \
        arm64) npm_cpu=arm64 ;; \
        *) echo "Unsupported architecture for Codex: ${architecture}" >&2; exit 1 ;; \
    esac \
    && npm config set fetch-retries 5 \
    && npm config set fetch-retry-mintimeout 20000 \
    && npm config set fetch-retry-maxtimeout 120000 \
    && npm install --global --no-audit --no-fund --os=linux --cpu="${npm_cpu}" \
        "@openai/codex@latest" \
        "@anthropic-ai/claude-code@latest" \
    && npm cache clean --force \
    && codex --version \
    && claude --version

WORKDIR /workspace

# Keep the existing classroom experience: the container opens directly in a
# complete Rucbase checkout, including the GoogleTest submodule.
RUN git -c http.version=HTTP/1.1 clone --branch "${RUCBASE_REF}" --recursive \
        "${RUCBASE_REPOSITORY}" rucbase-lab

WORKDIR /workspace/rucbase-lab

CMD ["/bin/bash"]
