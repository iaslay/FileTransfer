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
TOOLCHAIN_FILE="${SCRIPT_DIR}/arm-toolchain.cmake"

echo "===================================================="
echo "  文件传输工具 - ARM 交叉编译"
echo "===================================================="
echo ""

# 检查工具链
echo "[检查] ARM 交叉编译工具链..."

if ! command -v arm-linux-gnueabihf-gcc >/dev/null 2>&1; then
    echo "[错误] 未找到 ARM 交叉编译器"
    echo "请安装：sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf"
    exit 1
fi
echo "  ✓ arm-linux-gnueabihf-gcc"
echo "  ✓ arm-linux-gnueabihf-g++"

# 检查工具链文件
if [ ! -f "$TOOLCHAIN_FILE" ]; then
    echo "[错误] 未找到 ARM 交叉编译工具链文件: $TOOLCHAIN_FILE"
    echo "请确保 arm-toolchain.cmake 存在于项目根目录"
    exit 1
fi
echo "  ✓ toolchain: ${TOOLCHAIN_FILE}"

# 检查 Qt5 ARM 库
QT5_ARM_PATH="/opt/Qt5.15.2-arm"
if [ ! -d "$QT5_ARM_PATH" ]; then
    echo "[错误] 未找到 ARM Qt5 库: $QT5_ARM_PATH"
    echo "请先运行 ./build_qt5_arm.sh 编译 Qt5 ARM 库"
    exit 1
fi
echo "  ✓ Qt5 ARM: ${QT5_ARM_PATH}"

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

# 确认 Qt5 的 CMake 配置目录确实存在
QT5_CMAKE_DIR="${QT5_ARM_PATH}/lib/cmake/Qt5"
if [ ! -d "$QT5_CMAKE_DIR" ]; then
    echo "[致命错误] 找不到 Qt5 CMake 配置目录: $QT5_CMAKE_DIR"
    echo "请检查 Qt 是否成功安装到了 /opt/Qt5.15.2-arm"
    exit 1
fi
echo "  ✓ Qt5 CMake 配置: ${QT5_CMAKE_DIR}"

echo ""
echo "[信息] 配置 CMake (ARM 交叉编译)..."

# 完美联动 RPATH 和 BOTH 搜索模式
cmake "$SCRIPT_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
    -DCMAKE_PREFIX_PATH="$QT5_ARM_PATH" \
    -DQt5_DIR="${QT5_ARM_PATH}/lib/cmake/Qt5" \
    -DCMAKE_FIND_ROOT_PATH="${QT5_ARM_PATH}" \
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
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
