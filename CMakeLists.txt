cmake_minimum_required(VERSION 3.14)
project(FileTransfer VERSION 1.0.0 LANGUAGES CXX)

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 自动处理
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找 Qt5 组件
find_package(Qt5 REQUIRED COMPONENTS Core Network Widgets)

# 源文件
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/transfer.cpp
    src/fileutil.cpp
)

set(HEADERS
    src/mainwindow.h
    src/transfer.h
    src/protocol.h
    src/fileutil.h
)

# 可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# 链接 Qt 库
target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    Qt5::Network
    Qt5::Widgets
)

# 包含路径
target_include_directories(${PROJECT_NAME} PRIVATE src)

# 安装规则
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# 打印配置信息
message(STATUS "项目名称: ${PROJECT_NAME}")
message(STATUS "项目版本: ${PROJECT_VERSION}")
message(STATUS "Qt5 路径: ${Qt5_DIR}")
message(STATUS "生成目标: ${PROJECT_NAME}")