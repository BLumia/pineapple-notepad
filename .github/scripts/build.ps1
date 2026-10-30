param(
    [switch]$PrepareOffline,
    [switch]$UpdateHashes,
    [switch]$Deploy
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

. "$PSScriptRoot\buildutils.ps1"

#region Prepare offline dependencies
if ($PrepareOffline) {
    $offlineDir = Join-Path $PSScriptRoot "offline"
    if (-not (Test-Path -Path $offlineDir)) {
        New-Item -ItemType Directory -Path $offlineDir -Force | Out-Null
        Write-Host "Created offline directory: $offlineDir" -ForegroundColor Green
    }

    # dependencies.json is the single source of truth (also consumed by
    # CMake and the flatpak manifest); never hardcode urls here.
    $depsFile = Join-Path $PSScriptRoot "dependencies.json"
    $depsRaw = Get-Content $depsFile -Raw
    $dependencies = ConvertFrom-Json $depsRaw
    $hashesUpdated = $false

    foreach ($dep in $dependencies) {
        $filename = if ($dep.'dest-filename') { $dep.'dest-filename' } else { Split-Path $dep.url -Leaf }
        $filepath = Join-Path $offlineDir $filename
        if (-not (Test-Path -Path $filepath)) {
            Write-Host "Downloading $filename ..." -ForegroundColor Yellow
            Invoke-WebRequest -Uri $dep.url -OutFile $filepath
            Write-Host "Downloaded: $filepath" -ForegroundColor Green
        } else {
            Write-Host "$filename already exists, skipping download" -ForegroundColor Gray
        }

        $actualHash = (Get-FileHash -Path $filepath -Algorithm SHA256).Hash.ToLower()
        if ($actualHash -ne $dep.sha256) {
            if ($UpdateHashes) {
                Write-Warning "$filename : sha256 changed, updating dependencies.json ($($dep.sha256) -> $actualHash)"
                $depsRaw = $depsRaw.Replace($dep.sha256, $actualHash)
                $hashesUpdated = $true
            } else {
                Write-Error "SHA256 mismatch for $filename (expected $($dep.sha256), got $actualHash). Re-run with -UpdateHashes if the version was just bumped."
                exit 1
            }
        }
    }

    if ($hashesUpdated) {
        Set-Content -Path $depsFile -Value $depsRaw -NoNewline
        Write-Host "dependencies.json updated with new hashes" -ForegroundColor Green
    }

    Write-Host "Offline dependencies prepared successfully" -ForegroundColor Green
    exit 0
}
#endregion

#region Initialize and prepare build/deploy env
. Initialize-BuildEnvironment
if ($IsWindows) {
    Initialize-VSDevShell -DevCmdArguments "-arch=x64 -host_arch=x64"
}
if ($IsMacOS) {
    Patch-KF6ToolsRpath
}
#endregion

#region Deploy
if ($Deploy) {
    if ($IsWindows) {
        $binaryPath = Join-Path $PWD.Path "install-msvc\bin\pnotepad.exe"
        $buildDir = Join-Path $PWD.Path "build-msvc"
        $kf6InstallDir = $env:KF6REDIST_DIR

        Deploy-WindowsApp -BinaryPath $binaryPath `
            -Kf6InstallDir $kf6InstallDir -BuildDir $buildDir `
            -QtDir $env:QT_ROOT_DIR `
            -Locales @("zh_CN") `
            -ProjectMoPathPattern "po/locale/{locale}/LC_MESSAGES/pineapple-notepad.mo" `
            -KfPlugins @("kiconthemes", "breeze", "sonnet") `
            -CopyVCRuntime
    } elseif ($IsMacOS) {
        $buildDir = Join-Path $PSScriptRoot "build"
        $appBundle = Join-Path $buildDir "pnotepad.app"
        $kf6InstallDir = $env:KF6REDIST_DIR

        Deploy-MacOSAppBundle -AppBundle $appBundle -BinaryName "pnotepad" `
            -BuildDir $buildDir -Kf6InstallDir $kf6InstallDir `
            -QtDir $env:QT_DIR `
            -Locales @("zh_CN") `
            -ProjectMoPathPattern "po/locale/{locale}/LC_MESSAGES/pineapple-notepad.mo" `
            -DmgName "pineapple-notepad" `
            -KfPlugins @("kiconthemes", "breeze", "sonnet")
    } else {
        Write-Error "Deploy is not supported on this platform"
        exit 1
    }
    exit 0
}
#endregion

# Offline builds need no special flags anymore: CMake (cmake/PnDependencies.cmake)
# automatically consumes the archives under offline/ when that directory exists.

if ($IsWindows) {
    Build-CMakeProject -SourcePath "." -RepoName "pineapple-notepad" -ForceRebuild `
        -BuildBaseDir "build-msvc" -InstallPrefix "install-msvc" -NoSourceIdentifierFolder -AvoidReconfig `
        -CMakeArgs @("-DSPONSOR_EDITION=OFF")
} elseif ($IsLinux) {
    # FHS: /opt is where self-contained third-party applications belong. The .deb
    # installs the app (plus the bundled Qt6/KF6 runtime) there, and only the
    # desktop entry / icon / appstream metadata go to /usr/share.
    $linuxInstallPrefix = "/opt/pineapple-notepad"

    Build-CMakeProject -SourcePath "." -RepoName "pineapple-notepad" -ForceRebuild `
        -BuildBaseDir "build-linux" -InstallPrefix $linuxInstallPrefix -NoSourceIdentifierFolder -SkipInstall `
        -CMakeArgs @(
            "-DPN_BUNDLE_QT_KF6=ON",
            "-DKF6REDIST_DIR=$env:KF6REDIST_DIR",
            "-DQT_DIR=$env:QT_DIR",
            # proper CMake list (';'-separated), independent of the env var
            "-DCMAKE_PREFIX_PATH=$env:KF6REDIST_DIR;$env:QT_DIR"
        )
} else {
    Build-CMakeProject -SourcePath "." -RepoName "pineapple-notepad" -ForceRebuild `
        -BuildBaseDir "build" -SkipInstall -NoSourceIdentifierFolder -AvoidReconfig
}
