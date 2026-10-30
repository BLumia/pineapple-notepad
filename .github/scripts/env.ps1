if ($IsMacOS) {
    Write-Host "Not supported yet..."
} elseif ($IsLinux) {
    Write-Host "Not supported yet..."
} elseif ($IsWindows) {
    Write-Host "env.ps1 for Windows"
    # If you are building it locally, you probably need something like (commented code):
    #
    # $env:QT_DIR = "D:\SDK\aqt\6.12.0\msvc2022_64"
    # $env:PATH = "$env:QT_DIR\bin;$env:PATH"
    # $env:KF6REDIST_DIR = "C:\Users\Gary\Sources\kf6redist\kf6redist-install"
    #
    # Save it to env.local.ps1 to override the default.
    $env:CMAKE_PREFIX_PATH = "$env:KF6REDIST_DIR"
    # Prefer Ninja over MSBuild for parallel builds
    $env:CMAKE_GENERATOR = "Ninja"
}
