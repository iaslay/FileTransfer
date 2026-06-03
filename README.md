# 网络文件传输工具 FileTransfer

## 概述

基于 **Qt 5 + TCP** 的跨平台文件传输工具，支持 **Windows** 与 **树莓派 ARM** 平台之间进行文件和目录的双向传输。

### 主要特性

- ✅ **双向文件传输**：Windows ↔ 树莓派 互传文件
- ✅ **大文件分块传输**：支持将大文件切割为多个数据块传输，接收端自动合并
- ✅ **目录传输**：递归传输整个目录结构
- ✅ **CRC32 校验**：每块数据和完整文件都有 CRC32 校验，确保传输完整性
- ✅ **断点续传**：暂停/恢复传输
- ✅ **图形界面**：统一的 Qt GUI 界面，一键操作
- ✅ **命令行参数**：支持自动连接和传输
- ✅ **统一程序**：同一份代码，通过 `--mode` 参数切换服务端/客户端

## 项目结构

```
ftransfer/
├── src/                  # 源代码
│   ├── main.cpp          # 程序入口
│   ├── protocol.h        # 传输协议定义
│   ├── fileutil.h        # 文件工具类（头文件）
│   ├── fileutil.cpp      # 文件工具类（分块、合并、校验）
│   ├── transfer.h        # 传输引擎（头文件）
│   ├── transfer.cpp      # 传输引擎（网络通信核心）
│   ├── mainwindow.h      # 主窗口（头文件）
│   └── mainwindow.cpp    # 主窗口（界面实现）
├── CMakeLists.txt        # CMake 构建配置
├── arm-toolchain.cmake   # ARM 交叉编译工具链
├── build_windows.bat     # Windows 构建脚本
├── build_arm.sh          # ARM 交叉编译脚本
├── build_qt5_arm.sh      # Qt5 ARM 交叉编译脚本
└── README.md             # 本文件
```

## 传输协议

基于 TCP 的自定义协议，消息格式为：

```
┌─────────────────────────────────────┐
│  消息头 (20 字节)                    │
│  - magic:     0x46544652 ("FTFR")  │
│  - type:      消息类型               │
│  - body_len:  消息体长度             │
│  - sequence:  序列号                 │
│  - checksum:  头部校验              │
├─────────────────────────────────────┤
│  消息体 (可变长度)                   │
│  - 握手 / 文件信息 / 数据块 / ...    │
└─────────────────────────────────────┘
```

### 支持的消息类型

| 类型 | 说明 |
|------|------|
| HANDSHAKE_REQ/ACK/NAK | 握手协商 |
| FILE_INFO/BLOCK/END/ACK/NAK | 文件传输 |
| DIR_START/ENTRY/END | 目录传输 |
| CANCEL/PAUSE/RESUME | 传输控制 |
| HEARTBEAT/ACK | 心跳检测 |

### 分块传输

- 默认分块大小：**4 MB**（可在界面中 1-64 MB 调节）
- 每块独立进行 **CRC32 校验**
- 接收端完成所有块后自动合并为完整文件
- 最终验证文件的整体 CRC32 校验值

## 编译指南

### 方案一：Windows 编译（原生编译）

**前置条件：**
- 安装 Qt 5.15.x（含 Qt Network 和 Qt Widgets 组件）
- 安装 CMake 3.14+
- MinGW 或 MSVC 编译器

**步骤：**

```bash
# 方法 1：使用构建脚本
cd ftransfer
build_windows.bat

# 方法 2：手动构建
cd ftransfer
mkdir build_windows && cd build_windows
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

**输出：** `build_windows/FileTransfer.exe`

### 方案二：树莓派直接编译（最简单）

在树莓派上直接安装 Qt5 并编译：

```bash
# 安装 Qt5
sudo apt-get install qtbase5-dev qt5-qmake cmake build-essential

# 编译
cd ftransfer
mkdir build && cd build
cmake ..
make -j4
```

**输出：** `build/FileTransfer`

### 方案三：Ubuntu 交叉编译 ARM 版本（推荐）

在 X86 Ubuntu 虚拟机上交叉编译：

#### 步骤 1：编译 Qt5 for ARM

```bash
# 安装 ARM 交叉编译器
sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf

# 使用提供的脚本编译 Qt5（需要 30-60 分钟）
chmod +x build_qt5_arm.sh
./build_qt5_arm.sh
```

#### 步骤 2：交叉编译本项目

```bash
# 确保 Qt5 ARM 库已安装到 /opt/Qt5.15.2-arm
chmod +x build_arm.sh
./build_arm.sh
```

**输出：** `build_arm/FileTransfer`（ARM 可执行文件）

#### 部署到树莓派

```bash
scp build_arm/FileTransfer pi@192.168.1.xxx:~/ftransfer
```

## 使用方法

### 启动方式

```bash
# 服务端模式（通常在 Windows 上运行，监听连接）
FileTransfer --mode server

# 客户端模式（通常在树莓派上运行，主动连接）
FileTransfer --mode client

# 指定端口
FileTransfer --mode server --port 8888

# 客户端模式自动连接
FileTransfer --mode client --host 192.168.1.100 --port 8888
```

### 图形界面操作

**服务端（Windows）：**
1. 启动程序，服务端自动开始监听
2. 客户端连接后，状态显示 "已连接（空闲）"
3. 点击 "选择文件..." 或 "选择目录..." 添加要传输的内容
4. 点击 "开始传输" 发送文件

**客户端（树莓派）：**
1. 输入服务器 IP 地址
2. 点击 "连接服务器"
3. 连接成功后，可以选择文件上传
4. 或者等待服务器推送文件（自动接收）

### 传输流程

```
Windows (服务端)                   树莓派 (客户端)
     │                                  │
     │←────── 握手请求 ──────────│
     │─────── 握手应答 ──────────→│
     │                                  │
     │─────── 文件信息 ──────────→│
     │─────── 数据块 1 ─────────→│      (带 CRC32 校验)
     │←────── 块确认 ──────────│
     │─────── 数据块 2 ─────────→│
     │←────── 块确认 ──────────│
     │    ...                        │
     │─────── 传输结束 ────────→│      (自动合并文件)
```

## 配置说明

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| 端口 | 8888 | TCP 监听/连接端口 |
| 分块大小 | 4 MB | 大文件切割的块大小 |
| 分块传输 | 启用 | 可关闭以使用普通传输模式 |

## 依赖

- **运行时：** Qt 5 Core, Network, Widgets
- **构建时：** CMake 3.14+, C++17 编译器
- **交叉编译：** arm-linux-gnueabihf-gcc/g++

## 设计原理

### 为什么用统一程序？

Windows 端和树莓派端的 GUI 界面完全相同，仅标题和色调略有不同。通过命令行参数 `--mode` 区分运行模式，一份代码编译两次，减少重复维护。

### 为什么用分块传输？

- 大文件（>100MB）直接传输会占用大量内存
- 分块后每块独立校验，失败只重传该块
- 支持暂停/恢复，下次启动可继续传输
- 接收端边收边写临时文件，减少内存占用

### 传输可靠性

- **每块 CRC32 校验**：发现损坏立即请求重传
- **文件级 CRC32 校验**：合并后验证文件完整性
- **心跳检测**：每 10 秒检测连接状态

## 许可

MIT License
