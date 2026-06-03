#!/bin/bash
# ============================================================
# Qt 5.15.2 ARM 交叉编译脚本
# 在 Ubuntu 虚拟机上运行，编译 ARM 版本的 Qt5 库
# ============================================================

set -e

# 配置参数
QT_VERSION="5.15.2"
QT_SRC_URL="https://download.qt.io/archive/qt/5.15/${QT_VERSION}/single/qt-everywhere-src-${QT_VERSION}.tar.xz"
INSTALL_PREFIX="/opt/Qt5.15.2-arm"
BUILD_DIR="$HOME/qt5-arm-build"
JOBS=$(nproc)

# ARM 编译器前缀
ARM_CROSS=arm-linux-gnueabihf

echo "===================================================="
echo "  Qt ${QT_VERSION} ARM 交叉编译"
echo "  安装路径: ${INSTALL_PREFIX}"
echo "  并行任务: ${JOBS}"
echo "===================================================="
echo ""

# 检查工具链
echo "[步骤 1/5] 检查 ARM 交叉编译工具链..."

if ! command -v ${ARM_CROSS}-gcc &> /dev/null; then
    echo "安装 ARM 交叉编译工具链..."
    sudo apt-get update
    sudo apt-get install -y \
        ${ARM_CROSS}-gcc \
        ${ARM_CROSS}-g++ \
        ${ARM_CROSS}-libc-dev \
        build-essential \
        libfontconfig1-dev \
        libfreetype6-dev \
        libxcb1-dev \
        libxcb-util0-dev \
        libxcb-shm0-dev \
        libxcb-render0-dev \
        libxcb-keysyms1-dev \
        libxcb-image0-dev \
        libxcb-icccm4-dev \
        libxcb-sync1-dev \
        libxcb-xfixes0-dev \
        libxcb-shape0-dev \
        libxcb-randr0-dev \
        libxcb-glx0-dev \
        libxcb-xinerama0-dev \
        libxcb-xkb-dev \
        libxkbcommon-dev \
        libxkbcommon-x11-dev \
        python3 \
        perl \
        flex \
        bison
fi

echo "  ✓ 工具链就绪"
echo ""

# 下载源码
echo "[步骤 2/5] 下载 Qt ${QT_VERSION} 源码..."

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

if [ ! -f "qt-everywhere-src-${QT_VERSION}.tar.xz" ]; then
    wget -c "${QT_SRC_URL}"
fi

if [ ! -d "qt-everywhere-src-${QT_VERSION}" ]; then
    echo "解压源码..."
    tar xf "qt-everywhere-src-${QT_VERSION}.tar.xz"
fi

echo "  ✓ 源码就绪"
echo ""

# 创建 mkspec 配置（如果不存在）
echo "[步骤 3/5] 配置 ARM mkspec..."

QT_SRC_DIR="${BUILD_DIR}/qt-everywhere-src-${QT_VERSION}"
ARM_MKSPEC="${QT_SRC_DIR}/qtbase/mkspecs/linux-arm-gnueabihf-g++"

if [ ! -d "${ARM_MKSPEC}" ]; then
    echo "创建 ARM mkspec 配置..."
    cp -r "${QT_SRC_DIR}/qtbase/mkspecs/linux-arm-gnueabi-g++" "${ARM_MKSPEC}"

    # 修改 qmake.conf
    cat > "${ARM_MKSPEC}/qmake.conf" << 'QMAKEEOF'
#
# qmake configuration for arm-linux-gnueabihf
#

MAKEFILE_GENERATOR      = UNIX
CONFIG                 += incremental
QMAKE_INCREMENTAL_STYLE = sublib

include(../common/linux.conf)
include(../common/gcc-base-unix.conf)
include(../common/g++-unix.conf)

# 交叉编译器
QMAKE_CC                = arm-linux-gnueabihf-gcc
QMAKE_CXX               = arm-linux-gnueabihf-g++
QMAKE_LINK              = arm-linux-gnueabihf-g++
QMAKE_LINK_SHLIB        = arm-linux-gnueabihf-g++
QMAKE_AR                = arm-linux-gnueabihf-ar cqs
QMAKE_OBJCOPY           = arm-linux-gnueabihf-objcopy
QMAKE_NM                = arm-linux-gnueabihf-nm -P
QMAKE_STRIP             = arm-linux-gnueabihf-strip

# ARM 架构标志
QMAKE_CFLAGS           += -march=armv7-a -mfloat-abi=hard -mfpu=neon
QMAKE_CXXFLAGS         += -march=armv7-a -mfloat-abi=hard -mfpu=neon

# 链接标志
QMAKE_LFLAGS           += -march=armv7-a -mfloat-abi=hard -mfpu=neon

# 加载设备配置
load(qt_config)
QMAKEEOF
fi

echo "  ✓ mkspec 配置就绪"
echo ""

# 配置 Qt
echo "[步骤 4/5] 配置 Qt 交叉编译..."

cd "${QT_SRC_DIR}"

# 清理之前配置
if [ -f "config.summary" ]; then
    make distclean 2>/dev/null || true
fi

./configure \
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
    -v 2>&1 | tee configure.log

echo ""
echo "  ✓ 配置完成"
echo ""

# 编译和安装
echo "[步骤 5/5] 编译并安装 Qt (此步骤可能需要 30-60 分钟)..."

make -j${JOBS} 2>&1 | tee make.log

echo ""
echo "安装到 ${INSTALL_PREFIX}..."
sudo make install 2>&1 | tee install.log

echo ""
echo "===================================================="
echo "  Qt ${QT_VERSION} ARM 交叉编译完成！"
echo "  安装路径: ${INSTALL_PREFIX}"
echo ""
echo "  验证:"
echo "  ls ${INSTALL_PREFIX}/lib/"
echo "  ls ${INSTALL_PREFIX}/bin/qmake"
echo ""
echo "  编译本项目的 ARM 版本:"
echo "  ./build_arm.sh"
echo "===================================================="
