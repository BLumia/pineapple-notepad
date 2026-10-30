@echo off
setlocal enabledelayedexpansion

REM Check if executable exists
if not exist "build\pnotepad.exe" (
    echo Error: Executable not found. Please build the project first using build.bat
    echo.
    echo To build the project:
    echo   1. Ensure MSYS2 UCRT64 is installed
    echo   2. Run build.ps1
    echo.
    pause
    exit /b 1
)

REM Set MSYS2 paths for runtime dependencies
set MSYS2_ROOT=C:\msys64
set UCRT64_ROOT=%MSYS2_ROOT%\ucrt64
set PATH=%UCRT64_ROOT%\bin;%MSYS2_ROOT%\usr\bin;%PATH%
set QT_WIN_DEBUG_CONSOLE=attach

REM Check for required Qt6 DLLs
echo Checking runtime dependencies...
if not exist "%UCRT64_ROOT%\bin\Qt6Core.dll" (
    echo Warning: Qt6 runtime libraries not found in PATH
    echo Please ensure MSYS2 UCRT64 Qt6 packages are installed.
    echo.
)

echo Launching application...
echo.

REM Launch the application and keep console open
cd /d "%~dp0"
"build\pnotepad.exe" %*

if %ERRORLEVEL% neq 0 (
    echo App Crashed
    exit /b 1
)

echo Application started successfully!
