@echo off
setlocal

REM 创建 build 目录
if not exist build (
    mkdir build
)

REM 生成 VS2022 x64 工程
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 
if errorlevel 1 (
    echo CMake generate failed
    exit /b 1
)

REM 编译 Release
cmake --build build --config Release
if errorlevel 1 (
    echo Build failed
    exit /b 1
)

echo Build Success!
pause
