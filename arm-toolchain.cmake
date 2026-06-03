# ============================================================
# ARM 交叉编译工具链文件
# 用于在 X86 Ubuntu 虚拟机上交叉编译树莓派 ARM 版本
# 安装工具链：sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
# ============================================================

# 目标系统
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 交叉编译器
set(CMAKE_C_COMPILER   arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

# 查找策略
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Qt5 ARM 库路径（编译 Qt5 时的安装路径）
set(QT5_ARM_ROOT "/opt/Qt5.15.2-arm")

# 设置 CMAKE_PREFIX_PATH 指向 ARM Qt5
if(EXISTS ${QT5_ARM_ROOT})
    set(CMAKE_PREFIX_PATH ${QT5_ARM_ROOT})
endif()

# ARM 编译标志
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv7-a -mfloat-abi=hard -mfpu=neon")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv7-a -mfloat-abi=hard -mfpu=neon")

message(STATUS "使用 ARM 交叉编译工具链")
message(STATUS "C 编译器: ${CMAKE_C_COMPILER}")
message(STATUS "C++ 编译器: ${CMAKE_CXX_COMPILER}")
message(STATUS "Qt5 ARM 路径: ${QT5_ARM_ROOT}")