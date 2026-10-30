@echo off
setlocal enabledelayedexpansion

if not exist "build\kshlexer_fold_test.exe" (
    echo Error: Tests not built. Please build the project first.
    echo.
    echo   pwsh -ExecutionPolicy Bypass -File build-msys2.ps1
    echo.
    pause
    exit /b 1
)

set MSYS2_ROOT=C:\msys64
set UCRT64_ROOT=%MSYS2_ROOT%\ucrt64
set PATH=%UCRT64_ROOT%\bin;%MSYS2_ROOT%\usr\bin;%PATH%
set QT_FORCE_STDERR_LOGGING=1

cd /d "%~dp0"
ctest --test-dir build --output-on-failure %*

set EXITCODE=%ERRORLEVEL%
if %EXITCODE% neq 0 (
    echo.
    echo ctest exited with code %EXITCODE%
    exit /b %EXITCODE%
)
