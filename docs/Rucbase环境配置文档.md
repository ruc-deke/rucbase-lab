<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

- [Rucbase环境配置文档](#rucbase%E7%8E%AF%E5%A2%83%E9%85%8D%E7%BD%AE%E6%96%87%E6%A1%A3)
  - [虚拟机软件VMware/VirtualBox](#%E8%99%9A%E6%8B%9F%E6%9C%BA%E8%BD%AF%E4%BB%B6vmwarevirtualbox)
    - [VMware Workstation / Fusion（推荐）](#vmware-workstation--fusion%E6%8E%A8%E8%8D%90)
    - [VirtualBox（开源备选）](#virtualbox%E5%BC%80%E6%BA%90%E5%A4%87%E9%80%89)
  - [WSL 2.0 (Windows Subsystem for Linux)](#wsl-20-windows-subsystem-for-linux)
    - [WSL2.0相关注意事项](#wsl20%E7%9B%B8%E5%85%B3%E6%B3%A8%E6%84%8F%E4%BA%8B%E9%A1%B9)
  - [Ubuntu操作系统](#ubuntu%E6%93%8D%E4%BD%9C%E7%B3%BB%E7%BB%9F)
  - [使用Docker进行环境配置](#%E4%BD%BF%E7%94%A8docker%E8%BF%9B%E8%A1%8C%E7%8E%AF%E5%A2%83%E9%85%8D%E7%BD%AE)
    - [拉取镜像](#%E6%8B%89%E5%8F%96%E9%95%9C%E5%83%8F)
    - [首次创建并进入容器](#%E9%A6%96%E6%AC%A1%E5%88%9B%E5%BB%BA%E5%B9%B6%E8%BF%9B%E5%85%A5%E5%AE%B9%E5%99%A8)
    - [以后重新进入容器](#%E4%BB%A5%E5%90%8E%E9%87%8D%E6%96%B0%E8%BF%9B%E5%85%A5%E5%AE%B9%E5%99%A8)
    - [视频教程](#%E8%A7%86%E9%A2%91%E6%95%99%E7%A8%8B)
  - [代码编辑器VS Code](#%E4%BB%A3%E7%A0%81%E7%BC%96%E8%BE%91%E5%99%A8vs-code)
    - [安装VS Code](#%E5%AE%89%E8%A3%85vs-code)
    - [在VS Code中使用SSH连接到虚拟机](#%E5%9C%A8vs-code%E4%B8%AD%E4%BD%BF%E7%94%A8ssh%E8%BF%9E%E6%8E%A5%E5%88%B0%E8%99%9A%E6%8B%9F%E6%9C%BA)
  - [代码托管平台GitHub](#%E4%BB%A3%E7%A0%81%E6%89%98%E7%AE%A1%E5%B9%B3%E5%8F%B0github)
  - [Git使用](#git%E4%BD%BF%E7%94%A8)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# Rucbase环境配置文档

巧妇难为无米之炊，我们需要先对实验进行一些准备工作。配置好开发环境后，才能在Linux系统上运行Rucbase。

在本实验中，我们统一使用Ubuntu操作系统（Linux的一个主流发行版）。课程Docker镜像和手工安装均以Ubuntu 24.04 LTS为基线。推荐在你的主机（Windows/macOS）中使用VS Code作为代码编辑器。使用虚拟机的同学，推荐使用SSH连接到虚拟机进行实验。

请同学们按照我们的要求进行环境配置。如果你没有按照文档要求进行实验（比如使用了其他的Linux发行版），可能会出现一些奇怪的bug，进而影响实验进度和分数。

如果配置环境的过程中遇到了文档中没提及的问题，你可以向助教求助。



## 虚拟机软件VMware/VirtualBox

我们使用虚拟机软件安装Linux操作系统。虚拟机（Virtual Machine）指通过软件模拟的具有完整硬件系统功能的、运行在一个完全隔离环境中的完整计算机系统。

### VMware Workstation / Fusion（推荐）

VMware 是目前流行的桌面虚拟机软件。Windows / Linux 主机使用 **VMware Workstation Pro**，macOS 主机使用 **VMware Fusion Pro**，功能与使用方式基本一致，请按主机操作系统选择对应产品。

**许可说明（重要更新）：** 自 2024 年 11 月起，Broadcom 将 VMware Workstation Pro 与 Fusion Pro 改为**免费**，适用于个人、教育与商业用途，**无需购买许可证、无需填写激活密钥**。安装时若出现 License Key 提示，选择面向免费使用的选项（如 *Personal Use* / *Finish for free version*）即可继续。

> 旧版 **Workstation Player / Fusion Player** 已停更，请直接下载 **Pro** 版本（免费版 Pro 从 Workstation Pro 17.5.2、Fusion Pro 13.5.2 起提供）。请勿从非官方渠道下载“破解版”或来路不明的密钥，以免安全风险。

**下载方式：**

1. 打开官方产品页了解概况：[VMware Workstation & Fusion](https://www.vmware.com/products/desktop-hypervisor/workstation-and-fusion)
2. 实际安装包需在 Broadcom 支持门户获取（通常需免费注册账号）：
   - [Broadcom 下载中心](https://support.broadcom.com/group/ecx/downloads)
   - 在产品列表中搜索 **VMware Workstation Pro**（Windows / Linux）或 **VMware Fusion Pro**（macOS），选择较新的稳定版下载
3. 安装完成后即可新建虚拟机，无需额外付费激活

**平台对照：**

| 主机系统 | 产品 | 说明 |
| --- | --- | --- |
| Windows | VMware Workstation Pro | 推荐 17.5.2 及以上免费版 |
| Linux | VMware Workstation Pro | 同上 |
| macOS（Intel / Apple Silicon） | VMware Fusion Pro | 推荐 13.5.2 及以上免费版；Apple Silicon 请选对应 ARM 安装包 |

若官网入口或下载流程有调整，以 [Broadcom / VMware 官方说明](https://blogs.vmware.com/cloud-foundation/2024/11/11/vmware-fusion-and-workstation-are-now-free-for-all-users/) 为准。

### VirtualBox（开源备选）

若更倾向完全开源、或 VMware 下载不便，可使用 [Oracle VM VirtualBox](https://www.virtualbox.org/)。它是跨平台的开源虚拟化软件，对本实验同样适用。

## WSL 2.0 (Windows Subsystem for Linux)

对于使用Windows10,11操作系统的同学，你也可以使用WSL2.0作为自己的Linux开发环境。请注意，WSL开发环境默认安装为1.0版本，1.0并不能提供完整的Linux内核功能，**你必须升级为WSL2.0**，对此有疑问的同学，可在实验课上联系助教获得支持。

你也可以阅读Microsoft提供的官方文档：[适用于 Linux 的 Windows 子系统文档 | Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/wsl/)

### WSL2.0相关注意事项

1. 开启WSL2.0后，会开启hyper-v虚拟层，这样可能会导致你的pc性能略有下降(10%以内，包括磁盘IO和CPU/GPU处理速度)。
2. 部分使用低版本VMware，VirtualBox等虚拟化软件的同学可能无法正常启动虚拟化软件，需要进行升级
3. 对**国内手游/安卓模拟器有需求的同学**请注意，开启WSL2.0后可能你安装的模拟器将无法使用，你需要改用`BlueStack`等模拟器
4. PC机性能较为孱弱且对于日常使用性能有较高要求的同学不建议部署WSL2.0

## Ubuntu操作系统

安装好虚拟机软件后，我们就要在虚拟机中安装Linux操作系统。

Linux是开源的操作系统，拥有数百个发行版，其中Ubuntu是一个非常流行的桌面发行版。我们本次实验中就采用Ubuntu操作系统作为Rucbase的运行环境。

**我们强烈建议你使用Ubuntu系统，虽然在内部测试中，项目在原生Debian和CentOS上也可以通过编译，但是仍然可能遇到一些始料未及的问题**

[从清华大学开源软件镜像站下载Ubuntu 24.04 LTS](https://mirrors.tuna.tsinghua.edu.cn/ubuntu-releases/24.04/)

下载好Ubuntu后，在VMware中选择文件-新建虚拟机，选择“安装程序光盘映像文件”，即刚才下载的iso文件，然后按提示安装即可。

安装好系统之后，建议进行换源，以提高下载速度。Ubuntu官方的服务器在国外，速度较慢且时常断连。为了提高软件安装/更新速度，Ubuntu提供了选择服务器的功能，可以帮助我们使用位于国内的快速稳定的镜像服务器。先进入系统设置，打开软件和更新，在服务器列表中选择较近的服务器，我们建议使用清华或者阿里源。

我们还需要安装Rucbase的依赖环境，如gcc、cmake等。具体请参阅[Rucbase使用文档](Rucbase使用文档.md)

关于Linux的学习，还可以参考以下资料：

[Linux教程](https://www.runoob.com/linux)

[学习Linux命令行](https://nju-projectn.github.io/ics-pa-gitbook/ics2021/linux.html)

## 使用Docker进行环境配置

我们为Rucbase提供了国内Docker镜像。请先安装并启动Docker Desktop；Windows用户需要确认Docker Desktop运行在Linux容器模式。

### 拉取镜像

首次配置环境时，请根据主机处理器架构执行对应命令拉取最新镜像。

> 下列ARM64命令从Ubuntu 24.04多架构镜像发布后生效。在新版尚未推送期间，Apple Silicon请临时使用`--platform linux/amd64`运行现有镜像。

#### x64（Windows、Intel Mac）

```bash
docker pull --platform linux/amd64 crpi-i42psj2r9mqzm5eq.cn-wulanchabu.personal.cr.aliyuncs.com/daojiagban2026/rucbase-dev:latest
```

#### ARM64（Apple Silicon：M1/M2/M3/M4/M5）

Apple Silicon使用原生`linux/arm64`镜像：

```bash
docker pull --platform linux/arm64 crpi-i42psj2r9mqzm5eq.cn-wulanchabu.personal.cr.aliyuncs.com/daojiagban2026/rucbase-dev:latest
```

### 首次创建并进入容器

#### Windows（Docker Desktop，Linux容器模式）

```bash
docker run --name rucbase-dev --platform linux/amd64 -it crpi-i42psj2r9mqzm5eq.cn-wulanchabu.personal.cr.aliyuncs.com/daojiagban2026/rucbase-dev:latest
```

#### macOS（Apple Silicon：M1/M2/M3/M4/M5）

Apple Silicon使用原生`linux/arm64`平台：

```bash
docker run --name rucbase-dev --platform linux/arm64 -it crpi-i42psj2r9mqzm5eq.cn-wulanchabu.personal.cr.aliyuncs.com/daojiagban2026/rucbase-dev:latest
```

#### macOS（Intel）

```bash
docker run --name rucbase-dev --platform linux/amd64 -it crpi-i42psj2r9mqzm5eq.cn-wulanchabu.personal.cr.aliyuncs.com/daojiagban2026/rucbase-dev:latest
```

### 以后重新进入容器

容器创建完成后，Windows和macOS用户均使用以下命令重新启动并进入容器，无需再次执行`docker run`：

```bash
docker start -ai rucbase-dev
```

### 视频教程

我们分别录制了Windows和macOS系统的Rucbase实验环境搭建演示视频：

- [Windows系统实验环境搭建](https://www.bilibili.com/video/BV1ruuA6KE58/)
- [macOS系统实验环境搭建](https://www.bilibili.com/video/BV1U9uw6jEfW/)

## 代码编辑器VS Code

### 安装VS Code

VS Code（全称：Visual Studio Code）是一款由微软开发且跨平台的免费源代码编辑器。该软件支持语法高亮、代码自动补全、代码重构、查看定义功能，并且内置了命令行工具和 Git 版本控制系统。用户可以更改主题和键盘快捷方式实现个性化设置，也可以通过内置的扩展程序商店安装扩展以拓展软件功能。

[下载VS Code](https://code.visualstudio.com/Download)

在主机上下载并安装VS Code后，可以进行少量的配置，如安装C++扩展包、SSH相关扩展等。

**我们推荐你使用VScode，你也可以使用其他IDE或代码编辑工具，如Clion, Vim, Emacs等，但是请注意，使用其他工具请确保自己对其有一定的熟练度，在遇到部分配置问题时助教可能无法提供充足的支持**

### 在VS Code中使用SSH连接到虚拟机

需要在VS Code中安装SSH的相关扩展。

[官方教程](https://code.visualstudio.com/docs/remote/ssh-tutorial)

在VS Code扩展商店中搜索并安装Remote SSH，安装成功后进入配置页。在配置文件中填入虚拟机的用户名等相关信息，就可以连接虚拟机。



## 代码托管平台GitHub

Git 是一个版本控制系统，可以让你跟踪你对文件所做的修改。GitHub是世界上最大的软件远程仓库，是一个面向开源和私有软件项目的托管平台，使用Git做分布式版本控制。

你需要在Ubuntu中安装Git来下载实验代码。具体请参阅[Rucbase使用文档](Rucbase使用文档.md)。

[下载Git](https://git-scm.com/download)

## Git使用

对于不熟悉Git使用的同学，我们推荐你阅读该博客教程[Git教程 - 廖雪峰的官方网站 (liaoxuefeng.com)](https://www.liaoxuefeng.com/wiki/896043488029600)

