# Rucbase 教学开发镜像

镜像基于 Ubuntu 24.04 LTS，同时发布 `linux/amd64` 与 `linux/arm64` 两种架构。镜像包含 Rucbase 编译、调试和测试工具链，以及 Codex CLI 和 Claude Code。账号凭据不会写入镜像，首次运行 AI 工具时需要用户自行登录。

## 构建参数

| 参数 | 默认值 | 用途 |
| --- | --- | --- |
| `UBUNTU_IMAGE` | DaoCloud 代理的 Ubuntu 24.04 固定 digest | 多架构基础镜像；可覆盖为其他可信代理 |
| `UBUNTU_MIRROR` | `http://mirrors.tuna.tsinghua.edu.cn` | amd64/arm64 对应的 Ubuntu 软件源根地址；apt 会校验仓库签名 |
| `NODE_MAJOR` | `22` | Node.js 主版本 |
| `RUCBASE_REPOSITORY` | `https://github.com/ruc-deke/rucbase-lab.git` | 容器内源码仓库 |
| `RUCBASE_REF` | `main` | 容器内源码分支或标签 |

## 本地单架构验证

在仓库根目录执行：

```bash
docker buildx build \
  --platform linux/arm64 \
  --load \
  -f docker/Dockerfile \
  -t rucbase-dev:test .

docker run --rm rucbase-dev:test bash -lc \
  'gcc --version | head -n 1; cmake --version | head -n 1; codex --version; claude --version'
```

在 x64 主机上将 `linux/arm64` 改为 `linux/amd64`。Apple Silicon 也可以通过 Docker Desktop 的模拟能力验证 amd64 镜像。

## 发布多架构镜像

以下命令同时发布不可变日期标签、Ubuntu 基线标签和 `latest`。发布前应更新日期标签，并确认已登录阿里云容器镜像服务。

```bash
docker buildx build \
  --platform linux/amd64,linux/arm64 \
  --build-arg BUILD_DATE=2026-08-06T09:12:02Z \
  --build-arg VCS_REF=7eaa2418052553207b75d409b480082e80b6bc8e \
  -f docker/Dockerfile \
  -t crpi-i42psj2r9mqzm5eq.cn-wulanchabu.personal.cr.aliyuncs.com/daojiagban2026/rucbase-dev:ubuntu24.04-20260806 \
  -t crpi-i42psj2r9mqzm5eq.cn-wulanchabu.personal.cr.aliyuncs.com/daojiagban2026/rucbase-dev:24.04 \
  -t crpi-i42psj2r9mqzm5eq.cn-wulanchabu.personal.cr.aliyuncs.com/daojiagban2026/rucbase-dev:latest \
  --push .
```

发布后检查 manifest：

```bash
docker buildx imagetools inspect \
  crpi-i42psj2r9mqzm5eq.cn-wulanchabu.personal.cr.aliyuncs.com/daojiagban2026/rucbase-dev:latest
```

## 升级 AI 工具

镜像每次构建都会安装 npm 仓库当时提供的最新版 Codex CLI 和 Claude Code，不固定版本号。发布前应确认两种架构均可启动、Rucbase 可编译，并记录 `codex --version`、`claude --version` 的实际输出，再更新正式标签。
