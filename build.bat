@echo off
REM Windows 下用 PyInstaller 打包 ftransfer 为单文件 exe
REM 使用方法：双击运行 或 build.bat

pip install pyinstaller cryptography
pyinstaller --onefile --name ftransfer ftransfer.py

echo.
echo 打包完成！可执行文件在 dist\ftransfer.exe
pause
