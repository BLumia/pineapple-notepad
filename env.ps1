if ($IsMacOS) {
    Write-Host "env.ps1 for macOS"
    $env:QT_DIR = "/Users/blumia/Qt/6.12.0/macos"
    $env:PATH = "$env:QT_DIR/bin:$env:PATH"
    $env:KF6REDIST_DIR = "/Users/blumia/Sources/kf6redist/kf6redist-install"
    $env:CMAKE_PREFIX_PATH = "$env:KF6REDIST_DIR"
    $env:MACOSX_DEPLOYMENT_TARGET = "14.4"
} elseif ($IsLinux) {
    Write-Host "env.ps1 for Linux"
    # Official Qt binaries (not the distribution's Qt)
    $env:QT_DIR = "$HOME/SDK/Qt/6.12.0/gcc_64"
    $env:PATH = "$env:QT_DIR/bin:$env:PATH"
    # KF6 binaries built by ~/Sources/kf6redist (not the distribution's KF6)
    $env:KF6REDIST_DIR = "$HOME/Sources/kf6redist/kf6redist-install"
    # NOTE: the CMAKE_PREFIX_PATH *environment* variable uses the platform path
    # separator (":" on Linux); CMake would otherwise treat the whole value as a
    # single entry. build.ps1 additionally passes it as a proper CMake list.
    $env:CMAKE_PREFIX_PATH = "$env:KF6REDIST_DIR:$env:QT_DIR"

    # KF6 6.31 needs C++20 <format> (GCC 13+ / Clang 17+)
    if (-not $env:CC) { $env:CC = "gcc-13" }
    if (-not $env:CXX) { $env:CXX = "g++-13" }

    # The kf6redist artifacts carry $ORIGIN-relative RPATHs only (no Qt), so the
    # tools it installs (e.g. kconfig_compiler_kf6) need Qt via LD_LIBRARY_PATH
    # while building. Consumers must do the same at runtime.
    if ($env:LD_LIBRARY_PATH -notlike "*$env:QT_DIR/lib*") {
        $env:LD_LIBRARY_PATH = "$env:QT_DIR/lib:$env:KF6REDIST_DIR/lib:$env:LD_LIBRARY_PATH"
    }
} elseif ($IsWindows) {
    Write-Host "env.ps1 for Windows"
    $env:QT_DIR = "D:\SDK\aqt\6.12.0\msvc2022_64"
    $env:PATH = "$env:QT_DIR\bin;$env:PATH"
    $env:KF6REDIST_DIR = "C:\Users\Gary\Sources\kf6redist\kf6redist-install"
    $env:CMAKE_PREFIX_PATH = "$env:KF6REDIST_DIR"
}
