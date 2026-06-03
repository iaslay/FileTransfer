# FileTransfer — 网络文件传输工具

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![Qt5](https://img.shields.io/badge/Qt-5.15-green.svg)](https://www.qt.io/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

> 基于 **Qt 5 + TCP** 的跨平台文件传输工具，支持 **Windows** 与 **树莓派 ARM** 平台之间进行文件和目录的双向传输。

---

## 目录

- [概述](#概述)
- [特性](#特性)
- [项目结构](#项目结构)
- [快速开始](#快速开始)
- [编译指南](#编译指南)
  - [Windows 原生编译](#windows-原生编译)
  - [树莓派直接编译](#树莓派直接编译)
  - [Ubuntu 交叉编译 ARM](#ubuntu-交叉编译-arm)
- [使用方法](#使用方法)
- [传输协议](#传输协议)
- [传输流程](#传输流程)
- [配置说明](#配置说明)
- [依赖项](#依赖项)
- [设计原理](#设计原理)


---

## 概述

FileTransfer 是一个基于 TCP 协议的跨平台文件传输工具，使用同一份代码编译出 Windows 和 ARM 两个版本，通过命令行参数切换服务端/客户端模式。

**典型场景：** 在 Windows 上启动服务端，树莓派（或其他 ARM 设备）作为客户端主动连接，实现文件的双向传输。

## 特性

| 特性 | 说明 |
|------|------|
| **双向传输** | Windows ↔ 树莓派 互传文件和目录 |
| **分块传输** | 大文件切割为 4 MB（可调）数据块传输，自动合并 |
| **CRC32 校验** | 每块数据及完整文件均有校验，确保传输完整性 |
| **断点续传** | 支持暂停/恢复传输 |
| **目录传输** | 递归传输整个目录结构 |
| **图形界面** | 统一 Qt GUI，一键操作 |
| **命令行参数** | 支持静默自动连接和传输 |
| **统一程序** | 同一份代码，通过 `--mode` 参数切换运行模式 |

## 项目结构

```
ftransfer/
├── src/                      # 源代码
│   ├── main.cpp              # 程序入口
│   ├── protocol.h            # 传输协议定义（消息类型、消息头结构）
│   ├── fileutil.h / .cpp     # 文件工具类（分块、合并、CRC32 校验）
│   ├── transfer.h / .cpp     # 传输引擎（网络通信核心）
│   ├── mainwindow.h / .cpp   # 主窗口（GUI 界面实现）
├── CMakeLists.txt            # CMake 构建配置（v3.14+, C++17）
├── arm-toolchain.cmake       # ARM 交叉编译工具链配置
├── build_windows.bat         # Windows 构建脚本
├── build_arm.sh              # ARM 交叉编译脚本
├── build_qt5_arm.sh          # Qt5 ARM 交叉编译脚本
└── README.md                 # 本文件
```

## 快速开始

```bash
# 1️⃣ 克隆或进入项目目录
cd ftransfer

# 2️⃣ 在 Windows 上编译服务端
build_windows.bat

# 3️⃣ 在树莓派上编译客户端（或交叉编译）
#    树莓派直接编译：
mkdir build && cd build
cmake .. && make -j4
```

## 编译指南

### Windows 原生编译

**前置条件：**
- Qt 5.15.x（含 Core、Network、Widgets 组件）
- CMake 3.14+
- MinGW 或 MSVC 编译器

**步骤：**

```bash
# 方式一：使用构建脚本
build_windows.bat

# 方式二：手动构建
mkdir build_windows && cd build_windows
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

**输出：** `build_windows/FileTransfer.exe`

### 树莓派直接编译

在树莓派上直接编译（最简单的方式）：

```bash
# 安装依赖
sudo apt-get install qtbase5-dev qt5-qmake cmake build-essential

# 编译
cd ftransfer
mkdir build && cd build
cmake ..
make -j4
```

**输出：** `build/FileTransfer`

### Ubuntu 交叉编译 ARM

在 x86 Ubuntu 虚拟机上交叉编译树莓派 ARM 版本：

#### 第 1 步：安装 ARM 交叉编译器

```bash
sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
```

#### 第 2 步：编译 Qt5 for ARM（约 30-60 分钟）

```bash
chmod +x build_qt5_arm.sh
./build_qt5_arm.sh
```

#### 第 3 步：交叉编译本项目

```bash
chmod +x build_arm.sh
./build_arm.sh
```

**输出：** `build_arm/FileTransfer`（ARM 可执行文件）

#### 部署到树莓派

```bash
scp build_arm/FileTransfer pi@192.168.1.xxx:~/ftransfer
```

## 使用方法

### 命令行启动

```bash
# 服务端模式（Windows，监听连接）
FileTransfer --mode server

# 客户端模式（树莓派，主动连接）
FileTransfer --mode client

# 指定端口
FileTransfer --mode server --port 8888

# 客户端自动连接
FileTransfer --mode client --host 192.168.1.100 --port 8888
```

### 图形界面操作

**服务端（Windows）：**
1. 启动程序，自动开始监听
2. 客户端连接后，状态栏显示「已连接（空闲）」
3. 点击「选择文件...」或「选择目录...」添加要传输的内容
4. 点击「开始传输」发送文件

**客户端（树莓派）：**
1. 输入服务器 IP 地址
2. 点击「连接服务器」
3. 连接成功后，可选择文件上传
4. 或等待服务器推送文件（自动接收）

## 传输协议

基于 TCP 的自定义协议，消息格式如下：

```
┌──────────────────────────────────────────────────┐
│                  消息头 (20 字节)                  │
│  ┌─────────┬──────────┬──────────┬──────────┐   │
│  │ magic   │  type    │ body_len │ sequence │   │
│  │ 4 字节  │  4 字节  │  4 字节  │  4 字节  │   │
│  ├─────────┴──────────┴──────────┴──────────┤   │
│  │              checksum (4 字节)             │   │
│  └───────────────────────────────────────────┘   │
├──────────────────────────────────────────────────┤
│              消息体 (可变长度)                     │
│     握手 / 文件信息 / 数据块 / 目录条目 ...       │
└──────────────────────────────────────────────────┘
```

**魔数：** `0x46544652`（即 ASCII `"FTFR"`）

### 消息类型

| 类型 | 方向 | 说明 |
|------|------|------|
| `HANDSHAKE_REQ / ACK / NAK` | 双向 | 握手协商 |
| `FILE_INFO / BLOCK / END` | 发送方→接收方 | 文件元信息、数据块、传输结束 |
| `FILE_ACK / NAK` | 接收方→发送方 | 文件块确认/拒绝 |
| `DIR_START / ENTRY / END` | 发送方→接收方 | 目录传输：开始、条目、结束 |
| `CANCEL / PAUSE / RESUME` | 双向 | 传输控制：取消、暂停、恢复 |
| `HEARTBEAT / ACK` | 双向 | 心跳检测 |

### 分块传输

- **默认分块大小：** 4 MB（可在界面中 1~64 MB 范围调节）
- 每块独立进行 **CRC32 校验**
- 失败只重传对应块，无需重新传输整个文件
- 接收端完成所有块后自动合并为完整文件
- 最终验证文件的整体 CRC32 校验值

## 传输流程

```
Windows (服务端)                      树莓派 (客户端)
     │                                      │
     │◄────── 握手请求 ──────────────────│
     │─────── 握手应答 ──────────────────►│
     │                                      │
     │─────── 文件信息 ──────────────────►│
     │─────── 数据块 1（含 CRC32）──────►│
     │◄────── 块确认 ──────────────────│
     │─────── 数据块 2（含 CRC32）──────►│
     │◄────── 块确认 ──────────────────│
     │         ...                          │
     │─────── 传输结束 ──────────────────►│
     │                                      │  ← 自动合并文件
     │                                      │  ← 验证整体 CRC32
```

## 配置说明

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| 端口 | `8888` | TCP 监听 / 连接端口 |
| 分块大小 | `4 MB` | 大文件切割的块大小（可调 1~64 MB） |
| 分块传输 | 启用 | 可关闭以使用普通传输模式 |
| 心跳间隔 | 10 秒 | 检测连接是否存活 |

## 依赖项

| 类别 | 依赖 |
|------|------|
| 运行时 | Qt 5 Core、Qt 5 Network、Qt 5 Widgets |
| 构建时 | CMake 3.14+、C++17 编译器 |
| 交叉编译 | `arm-linux-gnueabihf-gcc/g++` |

## 设计原理

### 为什么用统一程序？

Windows 端和树莓派端的 GUI 界面完全相同，仅标题和色调略有区别。通过 `--mode` 命令行参数区分运行模式，一份代码编译两次，减少重复维护。

### 为什么用分块传输？

- 大文件（>100 MB）直接传输会占用大量内存
- 分块后每块独立校验，失败只重传该块，效率更高
- 支持暂停/恢复，下次启动可继续传输
- 接收端边收边写临时文件，内存占用低

### 传输可靠性

- **逐块 CRC32 校验：** 发现损坏立即请求重传
- **文件级 CRC32 校验：** 合并后验证文件完整性
- **心跳检测：** 每 10 秒检测连接状态，及时发现断连

