#!/bin/bash
# ============================================================
# Qt 5.15.2 ARM 交叉编译脚本 (方案一：Bootlin 低版本 GLIBC 工具链版)
# ============================================================

set -euo pipefail

# 配置参数
QT_VERSION="5.15.2"
QT_SRC_URL="https://download.qt.io/archive/qt/5.15/${QT_VERSION}/single/qt-everywhere-src-${QT_VERSION}.tar.xz"
INSTALL_PREFIX="/opt/Qt5.15.2-arm"
BUILD_DIR="$HOME/qt5-arm-build"
JOBS=$(nproc)

# Bootlin 工具链路径
TOOLCHAIN_PREFIX="/home/undef1ned/toolchains/armv7-eabihf--glibc--stable-2022.08-1/bin/arm-linux"

echo "===================================================="
echo "  Qt ${QT_VERSION} ARM 交叉编译 (低版本 GLIBC 方案)"
echo "===================================================="

# 1. 准备源码
echo "[步骤 1/4] 准备源码树..."
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

if [ ! -f "qt-everywhere-src-${QT_VERSION}.tar.xz" ]; then
    wget -c "${QT_SRC_URL}"
fi

if [ ! -d "qt-everywhere-src-${QT_VERSION}" ]; then
    echo "解压源码..."
    tar xf "qt-everywhere-src-${QT_VERSION}.tar.xz"
fi
QT_SRC_DIR="${BUILD_DIR}/qt-everywhere-src-${QT_VERSION}"

# 2. 自动打入所有历史兼容性补丁
echo "[步骤 2/4] 自动应用历史累积的所有源码补丁..."

# 补丁 1: GCC 11+ <limits> 补丁
sed -i '1s/^/#include <limits>\n/' "${QT_SRC_DIR}/qtbase/src/corelib/global/qfloat16.h" 2>/dev/null || true
sed -i '1s/^/#include <limits>\n/' "${QT_SRC_DIR}/qtbase/src/corelib/text/qbytearraymatcher.h" 2>/dev/null || true

# 补丁 2: zlib 大文件自毁宏补丁 (解决 features-time64 冲突)
if grep -q "undef _FILE_OFFSET_BITS" "${QT_SRC_DIR}/qtbase/src/3rdparty/zlib/src/gzguts.h" 2>/dev/null; then
    echo "  -> 正在切除 zlib 捣乱的 undef 宏..."
    sed -i '/undef _FILE_OFFSET_BITS/d' "${QT_SRC_DIR}/qtbase/src/3rdparty/zlib/src/gzguts.h"
fi

# 补丁 3: QML 引擎缺失 <cstdint> 补丁 (解决 uintptr_t 未定义错误)
if ! grep -q "include <cstdint>" "${QT_SRC_DIR}/qtdeclarative/src/qml/compiler/qv4compiler.cpp" 2>/dev/null; then
    echo "  -> 正在修复 QML 编译器缺失的 cstdint 头文件..."
    sed -i '1i #include <cstdint>' "${QT_SRC_DIR}/qtdeclarative/src/qml/compiler/qv4compiler.cpp"
fi
echo "  ✓ 所有补丁自动处理完毕"

# 3. 部署并重写目标架构 mkspec
echo "[步骤 3/4] 部署 Bootlin 工具链 mkspec 配置..."
ARM_MKSPEC="${QT_SRC_DIR}/qtbase/mkspecs/linux-arm-gnueabihf-g++"
rm -rf "${ARM_MKSPEC}"
cp -r "${QT_SRC_DIR}/qtbase/mkspecs/linux-arm-gnueabi-g++" "${ARM_MKSPEC}"

cat > "${ARM_MKSPEC}/qmake.conf" << QMAKEEOF
MAKEFILE_GENERATOR      = UNIX
CONFIG                 += incremental
QMAKE_INCREMENTAL_STYLE = sublib

include(../common/linux.conf)
include(../common/gcc-base-unix.conf)
include(../common/g++-unix.conf)

# 关键：完全换成 Bootlin 绝对路径编译器
QMAKE_CC                = ${TOOLCHAIN_PREFIX}-gcc
QMAKE_CXX               = ${TOOLCHAIN_PREFIX}-g++
QMAKE_LINK              = ${TOOLCHAIN_PREFIX}-g++
QMAKE_LINK_SHLIB        = ${TOOLCHAIN_PREFIX}-g++
QMAKE_AR                = ${TOOLCHAIN_PREFIX}-ar cqs
QMAKE_OBJCOPY           = ${TOOLCHAIN_PREFIX}-objcopy
QMAKE_NM                = ${TOOLCHAIN_PREFIX}-nm -P
QMAKE_STRIP             = ${TOOLCHAIN_PREFIX}-strip

# 注入大文件与时间戳宏保护
QMAKE_CFLAGS           += -march=armv7-a -mfloat-abi=hard -mfpu=neon -Wno-implicit-fallthrough -D_FILE_OFFSET_BITS=64
QMAKE_CXXFLAGS         += -march=armv7-a -mfloat-abi=hard -mfpu=neon -Wno-implicit-fallthrough -D_FILE_OFFSET_BITS=64
QMAKE_LFLAGS           += -march=armv7-a -mfloat-abi=hard -mfpu=neon

load(qt_config)
QMAKEEOF
echo "  ✓ mkspec 配置就绪"

# 4. 隔离安全编译 (Shadow Build)
echo "[步骤 4/4] 准备 Shadow Build 隔离构建环境..."
SHADOW_BUILD_DIR="${BUILD_DIR}/build"
rm -rf "${SHADOW_BUILD_DIR}"
mkdir -p "${SHADOW_BUILD_DIR}"
cd "${SHADOW_BUILD_DIR}"

echo "开始执行 Configure..."
../qt-everywhere-src-${QT_VERSION}/configure \
    -prefix ${INSTALL_PREFIX} \
    -xplatform linux-arm-gnueabihf-g++ \
    -opensource \
    -confirm-license \
    -nomake examples \
    -nomake tests \
    -no-opengl \
    -no-gtk \
    -no-dbus \
    -no-qml-debug \
    -no-compile-examples \
    -skip qtwebengine \
    -skip qtwayland \
    -skip qtlocation \
    -skip qtsensors \
    -skip qttools \
    -skip qtconnectivity \
    -skip qtserialport \
    -skip qtcharts \
    -skip qtdatavis3d \
    -skip qtgraphicaleffects \
    -skip qtquickcontrols \
    -skip qtquickcontrols2 \
    -feature-network \
    -no-icu \
    -no-use-gold-linker \
    -v 2>&1 | tee configure.log; test ${PIPESTATUS[0]} -eq 0

echo "开始全力并行编译..."
make -j${JOBS} 2>&1 | tee make.log; test ${PIPESTATUS[0]} -eq 0

echo "执行安装到系统目录: ${INSTALL_PREFIX}..."
sudo make install 2>&1 | tee install.log; test ${PIPESTATUS[0]} -eq 0

echo "===================================================="
echo " 🎉 配合低版本 GLIBC 的 Qt 核心库编译完成！"
echo "===================================================="