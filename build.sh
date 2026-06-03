#!/bin/bash
# Linux / macOS 下用 PyInstaller 打包 ftransfer 为单文件
# 使用方法: chmod +x build.sh && ./build.sh

pip install pyinstaller cryptography
pyinstaller --onefile --name ftransfer ftransfer.py

echo ""
echo "打包完成！可执行文件在 dist/ftransfer"
