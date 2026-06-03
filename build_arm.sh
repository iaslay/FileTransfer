#!/bin/bash
# ============================================================
# ARM 交叉编译脚本 - 在 Ubuntu 虚拟机上执行
# 用于编译树莓派 ARM 版本的可执行文件
#
# 前置条件：
#   1. 安装 ARM 交叉编译器：
#      sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
#
#   2. 编译 Qt5 for ARM（或使用预编译版本）：
#      参考下方 build_qt5_arm.sh 脚本
#
# 使用方法：
#   chmod +x build_arm.sh
#   ./build_arm.sh
# ============================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build_arm"

echo "===================================================="
echo "  文件传输工具 - ARM 交叉编译"
echo "===================================================="
echo ""

# 检查工具链
echo "[检查] ARM 交叉编译工具链..."

if ! command -v arm-linux-gnueabihf-gcc &> /dev/null; then
    echo "[错误] 未找到 ARM 交叉编译器"
    echo "请安装：sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf"
    exit 1
fi
echo "  ✓ arm-linux-gnueabihf-gcc"
echo "  ✓ arm-linux-gnueabihf-g++"

# 检查 Qt5 ARM 库
QT5_ARM_PATH="/opt/Qt5.15.2-arm"
if [ ! -d "$QT5_ARM_PATH" ]; then
    echo "[警告] 未找到 ARM Qt5 库: $QT5_ARM_PATH"
    echo "将尝试使用系统路径或环境变量 Qt5_DIR"
fi

# 清理旧的构建目录
if [ -d "$BUILD_DIR" ]; then
    echo "[信息] 清理旧的构建目录..."
    rm -rf "$BUILD_DIR"
fi

# 创建构建目录
echo "[信息] 创建构建目录: ${BUILD_DIR}"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo ""
echo "[信息] 配置 CMake (ARM 交叉编译)..."

cmake "$SCRIPT_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="${SCRIPT_DIR}/arm-toolchain.cmake" \
    -DCMAKE_PREFIX_PATH="$QT5_ARM_PATH" \
    -DCMAKE_BUILD_TYPE=Release \
    2>&1

echo ""
echo "[信息] 开始编译..."

cmake --build . --config Release -j$(nproc) 2>&1

echo ""
if [ -f "${BUILD_DIR}/FileTransfer" ]; then
    echo "===================================================="
    echo "  ARM 交叉编译成功！"
    echo "  输出文件: ${BUILD_DIR}/FileTransfer"
    echo ""
    echo "  检查目标架构:"
    file "${BUILD_DIR}/FileTransfer"
    echo ""
    echo "  部署到树莓派:"
    echo "  scp ${BUILD_DIR}/FileTransfer pi@树莓派IP:~/"
    echo "===================================================="
else
    echo "[错误] 编译失败，未生成可执行文件"
    exit 1
fi
