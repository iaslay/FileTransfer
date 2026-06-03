@echo off
chcp 65001 >nul
cls

:: ============================================================
:: Windows Build Script
:: ============================================================

:: Switch to project directory
cd /d "%~dp0"

:: Find CMake (Qt ships its own)
set CMAKE_PATH=C:\Qt\Tools\CMake_64\bin\cmake.exe
if not exist "%CMAKE_PATH%" (
    where cmake >nul 2>&1
    if %ERRORLEVEL% NEQ 0 (
        echo [ERROR] CMake not found. Install CMake first.
        pause
        exit /b 1
    )
    set CMAKE_PATH=cmake
)
echo [OK] CMake: %CMAKE_PATH%

:: Find MinGW
set MINGW_PATH=C:\Qt\Tools\mingw810_32\bin
if not exist "%MINGW_PATH%\mingw32-make.exe" (
    where mingw32-make >nul 2>&1
    if %ERRORLEVEL% NEQ 0 (
        echo [ERROR] MinGW not found.
        pause
        exit /b 1
    )
    for /f "delims=" %%i in ('where mingw32-make') do set MINGW_PATH=%%~dpi
)
set PATH=%MINGW_PATH%;%PATH%
echo [OK] MinGW: %MINGW_PATH%

:: Find Qt5
if not defined Qt5_DIR (
    if exist "C:\Qt\5.15.2\mingw81_32\lib\cmake\Qt5" (
        set Qt5_DIR=C:\Qt\5.15.2\mingw81_32\lib\cmake\Qt5
    ) else if exist "C:\Qt\Qt5.15.2\5.15.2\mingw81_64\lib\cmake\Qt5" (
        set Qt5_DIR=C:\Qt\Qt5.15.2\5.15.2\mingw81_64\lib\cmake\Qt5
    ) else (
        echo [WARN] Qt5_DIR not set. Ensure Qt 5.15.x is installed.
    )
)
echo [OK] Qt5_DIR: %Qt5_DIR%

:: Create build directory
if not exist "build_windows" mkdir build_windows
cd build_windows

echo [INFO] Configuring CMake ...
"%CMAKE_PATH%" .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] CMake configuration failed!
    pause
    exit /b 1
)

echo [INFO] Building ...
"%CMAKE_PATH%" --build . --config Release

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ====================================
    echo  BUILD SUCCESS!
    echo  Output: %CD%\FileTransfer.exe
    echo ====================================
    echo.
    echo Usage:
    echo   FileTransfer.exe --mode server
    echo   FileTransfer.exe --mode client
    echo.
) else (
    echo.
    echo [ERROR] Build failed!
    pause
    exit /b 1
)

pause