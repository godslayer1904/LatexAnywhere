@echo off
rem 用法：build.cmd [arm64|x64] [Release|Debug]
setlocal
set ARCH=%1
if "%ARCH%"=="" set ARCH=arm64
set CFG=%2
if "%CFG%"=="" set CFG=Release
set VSDIR=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools
if not exist "%VSDIR%" set VSDIR=C:\Program Files\Microsoft Visual Studio\2022\Community
if /i "%PROCESSOR_ARCHITECTURE%"=="ARM64" (set HOST=arm64) else (set HOST=amd64)
if /i "%HOST%"=="%ARCH%" (set VCARCH=%ARCH%) else (set VCARCH=%HOST%_%ARCH%)
if /i "%HOST%"=="amd64" if /i "%ARCH%"=="x64" set VCARCH=amd64
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" %VCARCH% >nul || exit /b 1
cmake -S "%~dp0." -B "%~dp0build\%ARCH%" -G Ninja -DCMAKE_BUILD_TYPE=%CFG% || exit /b 1
cmake --build "%~dp0build\%ARCH%" || exit /b 1
