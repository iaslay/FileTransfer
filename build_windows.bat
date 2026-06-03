@echo off
chcp 65001 >nul
cls

:: ============================================================
:: Windows 平台构建脚本
:: 依赖：安装 Qt 5.15.x + MinGW 或 MSVC 工具链
:: ============================================================

echo ===================================================
echo  文件传输工具 - Windows 构建脚本
echo ===================================================
echo.

:: 检查 CMake
where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [错误] 未找到 CMake，请先安装 CMake
    echo 下载地址: https://cmake.org/download/
    pause
    exit /b 1
)
echo [信息] 已找到 CMake

:: 检查 Qt5
if not defined Qt5_DIR (
    if exist "C:\Qt\Qt5.15.2\5.15.2\mingw81_64\lib\cmake\Qt5" (
        set Qt5_DIR=C:\Qt\Qt5.15.2\5.15.2\mingw81_64\lib\cmake\Qt5
    ) else if exist "C:\Qt\5.15.2\msvc2019_64\lib\cmake\Qt5" (
        set Qt5_DIR=C:\Qt\5.15.2\msvc2019_64\lib\cmake\Qt5
    ) else (
        echo [警告] 未设置 Qt5_DIR 环境变量
        echo 请确保已安装 Qt 5.15.x 并设置 Qt5_DIR
    )
)

:: 创建构建目录
if not exist "build_windows" mkdir build_windows
cd build_windows

echo [信息] 配置 CMake ...
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release 2>&1

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [错误] CMake 配置失败
    echo 可能原因：
    echo 1. 未安装 Qt 5.15.x
    echo 2. Qt5_DIR 路径配置不正确
    echo 3. MinGW 编译器未安装
    pause
    exit /b 1
)

echo [信息] 开始编译...
cmake --build . --config Release 2>&1

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ===================================================
    echo  编译成功！
    echo  输出文件: %CD%\FileTransfer.exe
    echo ===================================================
    echo.
    echo 运行方式：
    echo   FileTransfer.exe --mode server   (服务端模式)
    echo   FileTransfer.exe --mode client   (客户端模式)
    echo.
) else (
    echo.
    echo [错误] 编译失败
    pause
    exit /b 1
)

pause
