# BuildUtils.ps1
# Reusable utilities for CMake-based build workflows

function Test-ExecutableExists {
    [CmdletBinding()]
    Param(
        [Parameter(Mandatory = $True, Position = 0)]
        [string]$ExecutableName
    )

    # Use Get-Command to check for the executable
    # -Type Application ensures only external programs are considered
    # -ErrorAction SilentlyContinue suppresses error messages if the command is not found
    $command = Get-Command -Name $ExecutableName -CommandType Application -ErrorAction SilentlyContinue

    # Return $true if $command is not $null (i.e., the executable was found), otherwise $false
    return [bool]$command
}

function Check-ExecutableExists {
    [CmdletBinding()]
    Param(
        [Parameter(Mandatory = $True, Position = 0)]
        [string]$ExecutableName
    )

    if (-not (Test-ExecutableExists $ExecutableName)) {
        Write-Error "Required executable '$ExecutableName' not found in PATH"
        exit 1
    } else {
        Write-Host "Found executable '$ExecutableName'" -ForegroundColor Green
    }
}

<#
.SYNOPSIS
    Initialize build environment by loading environment configuration scripts.
.DESCRIPTION
    Loads environment configuration scripts with priority:
    1. <Name>.local.ps1 (local override, not committed to VCS)
    2. <Name>.ps1       (shared defaults)

    Sets $env:ENV_SCRIPT_LOADED to prevent duplicate loading in the same session.
    Must be dot-sourced to properly set variables in caller's scope.

.PARAMETER EnvironmentName
    The base name of the environment script to load.
    Default is 'env', which looks for 'env.local.ps1' or 'env.ps1'.
    Example: 'env-msys2' will look for 'env-msys2.local.ps1' or 'env-msys2.ps1'.
    Do not include the .ps1 extension.

.EXAMPLE
    . Initialize-BuildEnvironment
    # Loads env.local.ps1 or env.ps1

.EXAMPLE
    . Initialize-BuildEnvironment -EnvironmentName "env-msys2"
    # Loads env-msys2.local.ps1 or env-msys2.ps1
#>
function Initialize-BuildEnvironment {
    param(
        [string]$EnvironmentName = 'env'
    )

    # Skip if already loaded in current session (Prevent variable pollution)
    if ($env:ENV_SCRIPT_LOADED) {
        Write-Host "Environment script already loaded from: $env:ENV_SCRIPT_LOADED" -ForegroundColor Yellow
        Write-Host "Skipping initialization to prevent conflicts." -ForegroundColor Gray
        return
    }

    # Determine script directory (works for both direct execution and dot-sourcing)
    $private:scriptDir = if ($PSScriptRoot) {
        $PSScriptRoot
    } else {
        # Fallback for interactive console execution
        $private:callingScript = $MyInvocation.PSCommandPath
        if (-not $private:callingScript) {
            $private:callingScript = $MyInvocation.MyCommand.Definition
        }
        if ($private:callingScript) {
            Split-Path -Parent -Path $private:callingScript
        } else {
            $PWD.Path  # Fallback to current directory
        }
    }

    # Construct environment script paths based on the provided name
    # Pattern: <Name>.local.ps1 > <Name>.ps1
    $private:localEnvPath = Join-Path -Path $private:scriptDir -ChildPath "${EnvironmentName}.local.ps1"
    $private:globalEnvPath = Join-Path -Path $private:scriptDir -ChildPath "${EnvironmentName}.ps1"

    # Execute environment script based on priority rules
    if (Test-Path -Path $private:localEnvPath -PathType Leaf) {
        Write-Host "Loading local environment override: $private:localEnvPath" -ForegroundColor Cyan
        . $private:localEnvPath
        $env:ENV_SCRIPT_LOADED = $private:localEnvPath
    }
    elseif (Test-Path -Path $private:globalEnvPath -PathType Leaf) {
        Write-Host "Loading shared environment: $private:globalEnvPath" -ForegroundColor Cyan
        . $private:globalEnvPath
        $env:ENV_SCRIPT_LOADED = $private:globalEnvPath
    }
    else {
        Write-Error "Environment script not found in $private:scriptDir`nExpected: ${EnvironmentName}.local.ps1 or ${EnvironmentName}.ps1"
        return
    }

    Write-Host "Environment initialized from: $env:ENV_SCRIPT_LOADED" -ForegroundColor Green
}

<#
.SYNOPSIS
    Safely execute external commands and check exit codes
.DESCRIPTION
    Wraps external command execution with automatic $LASTEXITCODE validation.
    Required because $ErrorActionPreference="Stop" does NOT affect native commands.
.PARAMETER ScriptBlock
    Script block containing the external command to execute
.PARAMETER Description
    Human-readable description for error messages
#>
function Invoke-ExternalCommand {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [scriptblock]$ScriptBlock,
        [string]$Description = "External command"
    )
    & $ScriptBlock
    if ($LASTEXITCODE -ne 0) {
        throw "[FAILED] $Description exited with code $LASTEXITCODE"
    }
}

<#
.SYNOPSIS
    Patch kf6redist tool rpaths for macOS
.DESCRIPTION
    kf6redist tools (e.g. kconfig_compiler_kf6) use relative @loader_path rpaths,
    but they link against Qt frameworks which live in the local Qt installation.

    This function adds the local Qt lib path to the rpath of each tool binary
    using install_name_tool, so they can find Qt frameworks at runtime.

    Skips binaries that already have the correct rpath (idempotent).
.PARAMETER KF6InstallPrefix
    Path to the kf6redist install prefix (default: from CMAKE_PREFIX_PATH env)
.PARAMETER QtLibDir
    Path to the Qt lib directory (default: from QT_DIR env)
#>
function Patch-KF6ToolsRpath {
    [CmdletBinding()]
    param(
        [string]$KF6InstallPrefix = $env:CMAKE_PREFIX_PATH,
        [string]$QtLibDir = "$(if ($env:QT_DIR) { "$env:QT_DIR/lib" })"
    )

    if (-not $IsMacOS) { return }
    if (-not $KF6InstallPrefix -or -not $QtLibDir) {
        Write-Host "  → Skipping rpath patch (CMAKE_PREFIX_PATH or QT_DIR not set)" -ForegroundColor Gray
        return
    }

    $toolDir = Join-Path $KF6InstallPrefix "lib/libexec/kf6"
    if (-not (Test-Path $toolDir)) { return }

    Get-ChildItem -Path $toolDir -File | ForEach-Object {
        $existing = & otool -l $_.FullName 2>&1 | Select-String "path $QtLibDir"
        if (-not $existing) {
            Write-Host "  → Patching rpath: $($_.Name)" -ForegroundColor Yellow
            & install_name_tool -add_rpath $QtLibDir $_.FullName 2>&1 | Out-Null
        }
    }
}

<#
.SYNOPSIS
    Build generic CMake project from Git repository or local source path
.DESCRIPTION
    Supports two modes:
      1. Remote mode: Clones Git repository (with optional patches), builds, and installs
      2. Local mode: Builds directly from existing local source directory

    Skips completed builds via marker file in build directory.
    Supports tags, branches, and commit hashes in remote mode.
.PARAMETER RepoUrl
    Full Git repository URL (e.g., https://github.com/user/repo.git)
    Required in RemoteSource mode.
.PARAMETER Version
    Version identifier: tag, branch name, or commit hash
    Required in RemoteSource mode.
.PARAMETER SourcePath
    Local path to existing source directory (absolute or relative)
    Required in LocalSource mode. Skips cloning and patching.
.PARAMETER RepoName
    Project identifier for directory naming and logging (e.g., "extra-cmake-modules")
.PARAMETER SourceSubdir
    Optional subdirectory within repo/source containing CMakeLists.txt
.PARAMETER PatchFiles
    Optional list of patch files to apply after cloning (RemoteSource mode only).
    Patches are applied in order using 'git apply --ignore-whitespace'.
.PARAMETER SkipCloneIfExist
    (RemoteSource mode only) Skip cloning if source directory already exists.
    When skipped, patching is also skipped (assumes patches were applied previously).
.PARAMETER CMakeArgs
    Additional CMake arguments (e.g., @("-DBUILD_TESTING=OFF"))
.PARAMETER InstallPrefix
    Installation root directory (default: kf6redist-install)
.PARAMETER BuildBaseDir
    Root directory for build outputs (default: build)
.PARAMETER BuildType
    CMake build type (default: Release)
.PARAMETER ForceRebuild
    Ignore completion marker and rebuild from scratch
.PARAMETER SkipInstall
    Avoid/skip install
.PARAMETER NoSourceIdentifierFolder
    Avoid creating a subfolder as the source identifier (e.g., simply ""build/" instead of "build/v1.2.3-reponame/")
.PARAMETER AvoidReconfig
    Skip CMake configuration step if build directory already exists (useful for incremental builds)
#>
function Build-CMakeProject {
    [CmdletBinding(DefaultParameterSetName = "RemoteSource")]
    param(
        [Parameter(Mandatory, ParameterSetName = "RemoteSource")]
        [string]$RepoUrl,

        [Parameter(Mandatory, ParameterSetName = "RemoteSource")]
        [string]$Version,

        [Parameter(Mandatory, ParameterSetName = "LocalSource")]
        [string]$SourcePath,

        [Parameter(Mandatory)]
        [string]$RepoName,

        [Parameter(ParameterSetName = "RemoteSource")]
        [string[]]$PatchFiles = @(),

        [Parameter(ParameterSetName = "RemoteSource")]
        [switch]$SkipCloneIfExist,

        [string]$SourceSubdir = "",
        [string[]]$CMakeArgs = @(),
        [string]$InstallPrefix = "kf6redist-install",
        [string]$BuildBaseDir = "build",
        [string]$BuildType = "Release",
        [switch]$ForceRebuild,
        [switch]$SkipInstall,
        [switch]$NoSourceIdentifierFolder,
        [switch]$AvoidReconfig
    )

    if ($env:GITHUB_ACTIONS -eq "true") {
        Write-Host "::group::Building: $RepoName"
    }

    # Determine mode and set up paths
    $isRemoteMode = ($PSCmdlet.ParameterSetName -eq "RemoteSource")
    if ($isRemoteMode) {
        $sourceDir = "${Version}-${RepoName}"
        $buildDir  = $NoSourceIdentifierFolder ? $BuildBaseDir : (Join-Path $BuildBaseDir "${Version}-${RepoName}")
        $sourceType = "remote"
        $sourceIdentifier = "${Version}-${RepoName}"
    } else {
        # LocalSource mode: normalize path immediately
        try {
            $sourceDir = Resolve-Path -LiteralPath $SourcePath -ErrorAction Stop
        } catch {
            throw "Source path not found: '$SourcePath' (current dir: $($PWD.Path))"
        }
        $buildDir  = $NoSourceIdentifierFolder ? $BuildBaseDir : (Join-Path $BuildBaseDir "local-${RepoName}")
        $sourceType = "local"
        $sourceIdentifier = "local-${RepoName}"
    }

    $doneFile = Join-Path $buildDir ".ci-build-done"

    # Skip if already successfully built (unless forced)
    if (-not $ForceRebuild -and (Test-Path -LiteralPath $doneFile)) {
        Write-Host "[SKIP] ${RepoName}@${sourceIdentifier} already built" -ForegroundColor Green
        if ($env:GITHUB_ACTIONS -eq "true") {
            Write-Host "::endgroup::"
        }
        return
    }

    Write-Host "`n[BUILD] ${RepoName}@${sourceIdentifier}" -ForegroundColor Cyan
    if ($isRemoteMode) {
        Write-Host "        Repo: $RepoUrl" -ForegroundColor DarkGray
        if ($PatchFiles) {
            Write-Host "        Patches: $($PatchFiles -join ', ')" -ForegroundColor DarkGray
        }
    } else {
        Write-Host "        Local source: $sourceDir" -ForegroundColor DarkGray
    }
    if ($SourceSubdir) {
        Write-Host "        Subdir: $SourceSubdir" -ForegroundColor DarkGray
    }

    try {
        # ========== REMOTE MODE: Clone + Patching ==========
        if ($isRemoteMode) {
            $shouldClone = $true

            # Check if we can skip cloning
            if ($SkipCloneIfExist -and (Test-Path -LiteralPath $sourceDir -PathType Container)) {
                Write-Host "  → Source directory exists, skipping clone/patching (SkipCloneIfExist)" -ForegroundColor Yellow
                $shouldClone = $false
            }
            # Always clean if we need to clone (or if directory exists but shouldn't)
            elseif (Test-Path -LiteralPath $sourceDir) {
                Write-Host "  → Cleaning source directory: $sourceDir" -ForegroundColor Yellow
                Remove-Item -Recurse -Force $sourceDir -ErrorAction Stop
            }

            # Perform clone + checkout if needed
            if ($shouldClone) {
                Write-Host "  → Cloning repository (version: $Version) ..." -ForegroundColor Yellow
                if ($Version -match '^[0-9a-f]{7,40}$') {
                    # Commit hash: clone then checkout
                    Invoke-ExternalCommand -ScriptBlock {
                        git clone --depth 1 $RepoUrl $sourceDir --quiet
                    } -Description "Git clone"
                    Push-Location $sourceDir
                    try {
                        Invoke-ExternalCommand -ScriptBlock {
                            git checkout $Version --quiet
                        } -Description "Git checkout commit"
                    } finally {
                        Pop-Location
                    }
                } else {
                    # Tag or branch: direct clone with --branch
                    Invoke-ExternalCommand -ScriptBlock {
                        git clone --depth 1 --branch $Version $RepoUrl $sourceDir --quiet
                    } -Description "Git clone (branch/tag)"
                }

                # Resolve patch paths BEFORE directory changes (critical!)
                $absolutePatchFiles = @()
                foreach ($patch in $PatchFiles) {
                    try {
                        $absPath = Resolve-Path -LiteralPath $patch -ErrorAction Stop
                        $absolutePatchFiles += $absPath.ProviderPath
                    } catch {
                        $currentDir = $PWD.Path
                        $suggestedPath = Join-Path $PSScriptRoot $patch
                        throw "Patch file not found: '$patch'`n" +
                              "  Current directory: $currentDir`n" +
                              "  Suggested path: $suggestedPath"
                    }
                }

                # Apply patches if needed
                if ($absolutePatchFiles.Count -gt 0) {
                    Write-Host "  → Applying patches ..." -ForegroundColor Yellow
                    Push-Location $sourceDir
                    try {
                        foreach ($patchAbsPath in $absolutePatchFiles) {
                            $patchName = Split-Path -Leaf $patchAbsPath
                            Write-Host "    → Applying: $patchName" -ForegroundColor DarkYellow
                            Invoke-ExternalCommand -ScriptBlock {
                                git apply --ignore-whitespace "$patchAbsPath" 2>&1
                            } -Description "Apply patch: $patchName"
                        }
                    } finally {
                        Pop-Location
                    }
                }
            }
        }
        # ========== LOCAL MODE: Validate source path ==========
        else {
            if (-not (Test-Path -LiteralPath $sourceDir -PathType Container)) {
                throw "Local source directory not found: $sourceDir"
            }
            Write-Host "  → Using existing source directory" -ForegroundColor Yellow
        }

        # ========== COMMON BUILD STEPS ==========
        # Prepare build directory
        $null = New-Item -ItemType Directory -Path $buildDir -Force -ErrorAction Stop

        # Configure with CMake (skip if AvoidReconfig and already configured)
        $cmakeCacheFile = Join-Path $buildDir "CMakeCache.txt"
        if ($AvoidReconfig -and (Test-Path -LiteralPath $cmakeCacheFile -PathType Leaf)) {
            Write-Host "  → Skipping CMake configure (AvoidReconfig, existing configuration found)" -ForegroundColor Yellow
        } else {
            Write-Host "  → Configuring with CMake ..." -ForegroundColor Yellow
            if ($IsMacOS) {
                $platformCMakeArgs += "-DKDE_SKIP_RPATH_SETTINGS=TRUE"
            }
            Invoke-ExternalCommand -ScriptBlock {
                $sourcePath = if ($SourceSubdir) { Join-Path $sourceDir $SourceSubdir } else { $sourceDir }
                cmake -S $sourcePath -B $buildDir -DCMAKE_INSTALL_PREFIX="$InstallPrefix" -DCMAKE_BUILD_TYPE="$BuildType" @CMakeArgs
            } -Description "CMake configure"
        }

        # Build
        Write-Host "  → Building ..." -ForegroundColor Yellow
        Invoke-ExternalCommand -ScriptBlock {
            cmake --build "$buildDir" --config "$BuildType" -j
        } -Description "Build"

        # Install
        if (-not $SkipInstall) {
            Write-Host "  → Installing to: $InstallPrefix" -ForegroundColor Yellow
            Invoke-ExternalCommand -ScriptBlock {
                cmake --install "$buildDir" --prefix "$InstallPrefix" --config "$BuildType"
            } -Description "Install"
        } else {
            Write-Host "  → Install step skipped." -ForegroundColor Yellow
        }

        # Create completion marker with build metadata
        $markerContent = if ($isRemoteMode) {
            $patchList = if ($PatchFiles) { $PatchFiles -join ';' } else { 'none' }
            @"
Built at: $(Get-Date -Format 'u')
Source: remote
Repo: $RepoUrl
Version: $Version
Patches: $patchList
SkipCloneIfExist: $($SkipCloneIfExist.IsPresent)
Type: $($BuildType)
"@
        } else {
            @"
Built at: $(Get-Date -Format 'u')
Source: local
Path: $sourceDir
Type: $($BuildType)
"@
        }
        Set-Content -Path $doneFile -Value $markerContent -Force
        Write-Host "[✓] ${RepoName}@${sourceIdentifier} build succeeded" -ForegroundColor Green

        if ($env:GITHUB_ACTIONS -eq "true") {
            Write-Host "::endgroup::"
        }
    } catch {
        Write-Error "[✗] ${RepoName}@${sourceIdentifier} build failed: $_"
        if ($env:GITHUB_ACTIONS -eq "true") {
            Write-Host "::endgroup::"
        }
        # Clean up failed build artifacts
        if (Test-Path $buildDir) {
            Remove-Item -Recurse -Force $buildDir -ErrorAction SilentlyContinue
        }
        throw
    }
}

<#
.SYNOPSIS
    Recursively copy all @rpath dependencies to Frameworks directory
.DESCRIPTION
    Scans a binary or library for @rpath dependencies and copies them to the
    Frameworks directory. Recursively processes dependencies of copied libraries.
    Supports custom search paths for project-specific libraries.
.PARAMETER BinaryPath
    Path to the binary or library to scan
.PARAMETER FrameworksDir
    Destination Frameworks directory
.PARAMETER Kf6InstallDir
    Path to KF6 installation directory
.PARAMETER AdditionalSearchPaths
    Additional directories to search for libraries (e.g., build directory for project-specific libs)
.PARAMETER Processed
    Hashtable tracking already processed libraries (prevents duplicates)
.EXAMPLE
    $processed = @{}
    Copy-RpathDependencies -BinaryPath $binary -FrameworksDir $fwDir -Kf6InstallDir $kf6Dir -AdditionalSearchPaths @($buildDir) -Processed $processed
#>
function Copy-RpathDependencies {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$BinaryPath,
        [Parameter(Mandatory)]
        [string]$FrameworksDir,
        [Parameter(Mandatory)]
        [string]$Kf6InstallDir,
        [string[]]$AdditionalSearchPaths = @(),
        [hashtable]$Processed = @{}
    )

    $otoolOutput = & otool -L $BinaryPath 2>$null
    foreach ($line in $otoolOutput) {
        if ($line -match "@rpath/(.+\.dylib)") {
            $libName = $Matches[1]
            if ($Processed.ContainsKey($libName)) { continue }
            $Processed[$libName] = $true

            $dstPath = Join-Path $FrameworksDir $libName
            if (Test-Path -Path $dstPath) { continue }

            # Search in KF6 install dir first, then additional paths
            $srcPath = $null
            $searchPaths = @($Kf6InstallDir) + $AdditionalSearchPaths
            foreach ($searchDir in $searchPaths) {
                $candidatePath = Join-Path $searchDir "lib/$libName"
                if (Test-Path -Path $candidatePath) {
                    $srcPath = $candidatePath
                    break
                }
                # Also check direct path (for build directory outputs)
                $candidatePath = Join-Path $searchDir $libName
                if (Test-Path -Path $candidatePath) {
                    $srcPath = $candidatePath
                    break
                }
            }

            if ($srcPath) {
                Copy-Item -Path $srcPath -Destination $dstPath -Force
                Write-Host "  Copied: $libName" -ForegroundColor Gray
                # Recursively process this library's dependencies
                Copy-RpathDependencies -BinaryPath $dstPath -FrameworksDir $FrameworksDir `
                    -Kf6InstallDir $Kf6InstallDir -AdditionalSearchPaths $AdditionalSearchPaths -Processed $Processed
            } else {
                Write-Warning "Library not found: $libName"
            }
        }
    }
}

<#
.SYNOPSIS
    Resolve and copy PE (Windows) DLL dependencies
.DESCRIPTION
    Scans a binary or DLL for dependencies by parsing its PE import tables
    (Get-PEImportedDllNames; no external tools needed) and copies missing DLLs
    from specified source directories. Recursively processes dependencies of
    copied DLLs. Supports additional binaries to scan (e.g., plugins loaded
    at runtime).
.PARAMETER BinaryPath
    Path to the main binary or DLL to scan
.PARAMETER DestinationDir
    Destination directory for resolved DLLs
.PARAMETER SearchDirectories
    Hashtable mapping DLL name patterns (regex) to source directories.
    Example: @{ "^KF6" = "C:/kf6/bin"; "^Qt6" = "C:/Qt/bin" }
.PARAMETER AdditionalBinaries
    Extra binaries to scan (e.g., plugin DLLs in subdirectories)
.PARAMETER Processed
    Hashtable tracking already processed DLLs (prevents duplicates)
.EXAMPLE
    $searchDirs = @{ "^KF6" = "$kf6/bin"; "^Qt6" = "$qt/bin" }
    Copy-PEDependencies -BinaryPath "app.exe" -DestinationDir "deploy" `
        -SearchDirectories $searchDirs -AdditionalBinaries @("plugins/breeze6.dll")
#>
function Copy-PEDependencies {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$BinaryPath,
        [Parameter(Mandatory)]
        [string]$DestinationDir,
        [Parameter(Mandatory)]
        [hashtable]$SearchDirectories,
        [string[]]$AdditionalBinaries = @(),
        [hashtable]$Processed = @{}
    )

    $queue = [System.Collections.Queue]::new()
    $queue.Enqueue($BinaryPath)
    foreach ($bin in $AdditionalBinaries) {
        $queue.Enqueue($bin)
    }

    while ($queue.Count -gt 0) {
        $current = $queue.Dequeue()
        foreach ($depName in (Get-PEImportedDllNames -Path $current)) {
            if ($Processed.ContainsKey($depName)) { continue }
            $Processed[$depName] = $true

            $dstPath = Join-Path $DestinationDir $depName
            if (Test-Path -Path $dstPath) {
                $queue.Enqueue($dstPath)
                continue
            }

            $srcPath = $null
            foreach ($pattern in $SearchDirectories.Keys) {
                if ($depName -match $pattern) {
                    $candidate = Join-Path $SearchDirectories[$pattern] $depName
                    if (Test-Path -Path $candidate) {
                        $srcPath = $candidate
                        break
                    }
                }
            }

            if ($srcPath) {
                # Preserve the source file's original name casing
                $srcItem = Get-Item -LiteralPath $srcPath
                $dstPath = Join-Path $DestinationDir $srcItem.Name
                Copy-Item -LiteralPath $srcItem.FullName -Destination $dstPath -Force
                Write-Host "  Copied: $($srcItem.Name)" -ForegroundColor Gray
                $queue.Enqueue($dstPath)
            }
        }
    }
}

<#
.SYNOPSIS
    Parse the import table of a PE (Windows) binary
.DESCRIPTION
    Reads the IMAGE_IMPORT_DESCRIPTOR chain and the delay-load import
    descriptors (ImgDelayDescr) of a portable executable and returns the
    names of all imported DLLs. Pure PowerShell (no dumpbin); supports PE32
    and PE32+.
.PARAMETER Path
    Path to the .exe/.dll file
.OUTPUTS
    string[] of imported DLL names (lowercase); empty for non-PE files
#>
function Get-PEImportedDllNames {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    $fs = [System.IO.File]::OpenRead($Path)
    $br = New-Object System.IO.BinaryReader($fs)
    try {
        if ($fs.Length -lt 0x100 -or $br.ReadUInt16() -ne 0x5A4D) { return @() }  # no MZ signature
        $fs.Position = 0x3C
        $peOffset = $br.ReadInt32()
        $fs.Position = $peOffset
        if ($br.ReadUInt32() -ne 0x4550) { return @() }                            # no PE\0\0 signature

        # COFF header
        $null = $br.ReadUInt16()                                                   # Machine
        $numberOfSections = $br.ReadUInt16()
        $null = $br.ReadUInt32()                                                   # TimeDateStamp
        $null = $br.ReadUInt32()                                                   # PointerToSymbolTable
        $null = $br.ReadUInt32()                                                   # NumberOfSymbols
        $sizeOfOptionalHeader = $br.ReadUInt16()
        $null = $br.ReadUInt16()                                                   # Characteristics

        # Optional header
        $optHeaderStart = $fs.Position
        $magic = $br.ReadUInt16()
        if ($magic -eq 0x20B) { $dataDirOffset = 112; $imageBaseOffset = 24 }   # PE32+
        elseif ($magic -eq 0x10B) { $dataDirOffset = 96; $imageBaseOffset = 28 } # PE32
        else { return @() }
        $fs.Position = $optHeaderStart + $imageBaseOffset
        $imageBase = $br.ReadUInt64()
        $fs.Position = $optHeaderStart + $dataDirOffset + (1 * 8)                  # data directory [1] = imports
        $importTableRva = $br.ReadUInt32()
        $fs.Position = $optHeaderStart + $dataDirOffset + (13 * 8)                 # data directory [13] = delay imports
        $delayImportRva = $br.ReadUInt32()

        # Section headers (for RVA -> file offset translation)
        $fs.Position = $peOffset + 24 + $sizeOfOptionalHeader
        $sections = @()
        for ($i = 0; $i -lt $numberOfSections; $i++) {
            $secStart = $fs.Position
            if ($secStart + 40 -gt $fs.Length) { break }
            $null = $br.ReadBytes(8)                                               # Name
            $virtualSize = $br.ReadUInt32()
            $virtualAddress = $br.ReadUInt32()
            $sizeOfRawData = $br.ReadUInt32()
            $pointerToRawData = $br.ReadUInt32()
            $fs.Position = $secStart + 40
            if ($virtualAddress -eq 0) { continue }
            $sections += [pscustomobject]@{
                VA      = $virtualAddress
                Size    = [math]::Max($virtualSize, $sizeOfRawData)
                RawPtr  = $pointerToRawData
            }
        }
        if ($sections.Count -eq 0) { return @() }

        function Convert-RvaToOffset([uint32]$Rva) {
            foreach ($s in $sections) {
                if ($Rva -ge $s.VA -and $Rva -lt ($s.VA + $s.Size)) {
                    return [int64]($s.RawPtr + ($Rva - $s.VA))
                }
            }
            return -1
        }

        $names = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)

        # Read a NUL-terminated ASCII name at the given file offset.
        # Original casing is preserved (import names typically match the on-disk
        # file name); deduplication is case-insensitive via the HashSet.
        function Read-NameAtOffset([int64]$Offset) {
            $fs.Position = $Offset
            $bytes = New-Object System.Collections.Generic.List[byte]
            while (($b = $br.ReadByte()) -ne 0) { $bytes.Add($b) }
            return [System.Text.Encoding]::ASCII.GetString($bytes.ToArray())
        }

        # Direct imports: IMAGE_IMPORT_DESCRIPTOR chain (20 bytes each)
        $descriptorOffset = Convert-RvaToOffset $importTableRva
        if ($descriptorOffset -ge 0) {
            $fs.Position = $descriptorOffset
            while ($true) {
                $desc = $br.ReadBytes(20)
                if (($desc | Where-Object { $_ -ne 0 }).Count -eq 0) { break }     # all-zero terminator
                $nameRva = [BitConverter]::ToUInt32($desc, 12)
                $nameOffset = Convert-RvaToOffset $nameRva
                if ($nameOffset -lt 0) { break }
                $null = $names.Add((Read-NameAtOffset $nameOffset))
                $descriptorOffset += 20
                $fs.Position = $descriptorOffset
            }
        }

        # Delay-load imports: ImgDelayDescr chain (32 bytes each) from data
        # directory [13]. Fields are RVAs when grAttrs bit 1 (dlattrRva) is
        # set (always the case for modern linkers), VAs otherwise.
        $delayOffset = Convert-RvaToOffset $delayImportRva
        if ($delayOffset -ge 0) {
            $fs.Position = $delayOffset
            while ($true) {
                $desc = $br.ReadBytes(32)
                if (($desc | Where-Object { $_ -ne 0 }).Count -eq 0) { break }     # all-zero terminator
                $grAttrs = [BitConverter]::ToUInt32($desc, 0)
                $nameField = [BitConverter]::ToUInt32($desc, 4)
                if ($nameField -eq 0) { break }
                if ($grAttrs -band 2) {
                    $nameRva = $nameField
                } else {
                    $nameRva = $nameField - $imageBase
                }
                $nameOffset = Convert-RvaToOffset $nameRva
                if ($nameOffset -lt 0) { break }
                $null = $names.Add((Read-NameAtOffset $nameOffset))
                $delayOffset += 32
                $fs.Position = $delayOffset
            }
        }
        return @($names)
    } finally {
        $br.Close()
        $fs.Close()
    }
}

<#
.SYNOPSIS
    Copy the VC++ runtime DLLs actually used by deployed binaries (app-local)
.DESCRIPTION
    Locates the latest VC++ Redistributable CRT directory (VCINSTALLDIR or
    vswhere), scans every PE file under DestinationDir (recursively, including
    plugins) for import-table references, and copies only the referenced CRT
    DLLs next to the binaries. This avoids the classic redist installer (which
    MSIX cannot run) and Store "undisclosed dependency" warnings. App-local
    redistribution of these DLLs is permitted by the Visual Studio license.
.PARAMETER DestinationDir
    Deployed binaries directory (scanned recursively; DLLs are copied here)
.PARAMETER Architecture
    Redist architecture folder, defaults to "x64"
.OUTPUTS
    string[] of copied DLL file names
.EXAMPLE
    Copy-VCRuntimeAppLocal -DestinationDir "install-msvc/bin"
#>
function Copy-VCRuntimeAppLocal {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$DestinationDir,
        [string]$Architecture = "x64"
    )

    $redistRoot = $null
    if ($env:VCINSTALLDIR) {
        $redistRoot = Join-Path $env:VCINSTALLDIR "Redist\MSVC"
    } else {
        $vswherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path -LiteralPath $vswherePath -PathType Leaf) {
            $vsPath = & $vswherePath -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
            if ($vsPath) { $redistRoot = Join-Path $vsPath "VC\Redist\MSVC" }
        }
    }
    if (-not $redistRoot -or -not (Test-Path -LiteralPath $redistRoot)) {
        throw "VC++ Redist directory not found (looked under VCINSTALLDIR and via vswhere). Cannot ship app-local CRT."
    }
    $crtDir = Get-ChildItem -LiteralPath $redistRoot -Directory |
        Sort-Object Name -Descending |
        ForEach-Object { Join-Path $_.FullName $Architecture } |
        Where-Object { Test-Path -LiteralPath $_ -PathType Container } |
        ForEach-Object { Get-ChildItem -LiteralPath $_ -Directory -Filter "Microsoft.VC*.CRT" } |
        Select-Object -First 1
    if (-not $crtDir) {
        throw "Microsoft.VC*.CRT ($Architecture) not found under $redistRoot"
    }
    $available = @{}
    Get-ChildItem -LiteralPath $crtDir.FullName -Filter "*.dll" | ForEach-Object {
        $available[$_.Name] = $_    # PowerShell hashtables are case-insensitive
    }
    if ($available.Count -eq 0) {
        throw "No CRT DLLs found in $($crtDir.FullName)"
    }

    # Collect the set of VC runtime DLLs actually imported by deployed binaries
    $needed = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    $binaries = @(Get-ChildItem -LiteralPath $DestinationDir -Recurse -Include "*.exe", "*.dll" -File)
    foreach ($bin in $binaries) {
        foreach ($import in Get-PEImportedDllNames -Path $bin.FullName) {
            if ($available.ContainsKey($import)) { $null = $needed.Add($import) }
        }
    }
    if ($needed.Count -eq 0) {
        Write-Host "No VC++ runtime DLLs are imported by binaries under $DestinationDir; nothing to copy." -ForegroundColor DarkGray
        return @()
    }

    $copied = @()
    foreach ($name in ($needed | Sort-Object)) {
        $item = $available[$name]
        Copy-Item -LiteralPath $item.FullName -Destination (Join-Path $DestinationDir $item.Name) -Force
        $copied += $item.Name
        Write-Host "  Copied: $($item.Name)" -ForegroundColor Gray
    }
    return $copied
}

<#
.SYNOPSIS
    Copy translations for specified locales
.DESCRIPTION
    Copies KF6 translations (.mo, .qm), kf6_entry.desktop, project-specific
    translations, and Qt base translations to the specified directories.
.PARAMETER Kf6LocaleDir
    Source locale directory from kf6redist (e.g., "kf6redist-install/bin/data/locale").
    Optional: when empty, KF6 translations are skipped and only project/Qt
    translations are processed.
.PARAMETER DestinationDir
    Destination locale directory (e.g., "bin/data/locale" or "AppBundle/Contents/Resources/locale")
.PARAMETER Locales
    Array of locale codes to copy (e.g., @("zh_CN", "en"))
.PARAMETER BuildDir
    Path to the build directory (for project translations)
.PARAMETER ProjectMoPathPattern
    Path pattern for project .mo files relative to BuildDir. Use {locale} as placeholder for locale code.
    Example: "po/locale/{locale}/LC_MESSAGES/myapp.mo"
.PARAMETER QtTranslationsDir
    Source Qt translations directory (e.g., "$QtDir/translations")
.PARAMETER QtTranslationsDestinationDir
    Destination for Qt translations (e.g., "bin/translations" or "AppBundle/Contents/translations")
.EXAMPLE
    Copy-Translations -Kf6LocaleDir "$kf6/bin/data/locale" -DestinationDir "bin/data/locale" `
        -Locales @("zh_CN") -BuildDir $build `
        -ProjectMoPathPattern "po/locale/{locale}/LC_MESSAGES/myapp.mo" `
        -QtTranslationsDir "$qt/translations" -QtTranslationsDestinationDir "bin/translations"
#>
function Copy-Translations {
    [CmdletBinding()]
    param(
        [string]$Kf6LocaleDir = "",
        [Parameter(Mandatory)]
        [string]$DestinationDir,
        [Parameter(Mandatory)]
        [string[]]$Locales,
        [Parameter(Mandatory)]
        [string]$BuildDir,
        [string]$ProjectMoPathPattern,
        [string]$QtTranslationsDir,
        [string]$QtTranslationsDestinationDir
    )

    foreach ($localeName in $Locales) {
        $dstLocaleDir = Join-Path $DestinationDir $localeName
        $dstMessagesDir = Join-Path $dstLocaleDir "LC_MESSAGES"

        # Copy KF6 translations (.mo / .qm / kf6_entry.desktop), when provided
        if ($Kf6LocaleDir) {
            $srcLocaleDir = Join-Path $Kf6LocaleDir $localeName
            if (-not (Test-Path -Path $srcLocaleDir)) {
                Write-Warning "Locale not found: $localeName"
            } else {
                if (-not (Test-Path -Path $dstMessagesDir)) {
                    New-Item -ItemType Directory -Path $dstMessagesDir -Force | Out-Null
                }

                # Copy KF6 .mo files
                Get-ChildItem -Path "$srcLocaleDir/LC_MESSAGES" -Filter "*.mo" -ErrorAction SilentlyContinue | ForEach-Object {
                    Copy-Item -Path $_.FullName -Destination $dstMessagesDir -Force
                }

                # Copy KF6 .qm files
                Get-ChildItem -Path "$srcLocaleDir/LC_MESSAGES" -Filter "*.qm" -ErrorAction SilentlyContinue | ForEach-Object {
                    Copy-Item -Path $_.FullName -Destination $dstMessagesDir -Force
                }

                # Copy kf6_entry.desktop
                $desktopFile = Join-Path $srcLocaleDir "kf6_entry.desktop"
                if (Test-Path -Path $desktopFile) {
                    Copy-Item -Path $desktopFile -Destination $dstLocaleDir -Force
                }
            }
        }

        # Copy project's own .mo file
        if ($ProjectMoPathPattern) {
            $projectMoRelPath = $ProjectMoPathPattern -replace '\{locale\}', $localeName
            $projectMoFile = Join-Path $BuildDir $projectMoRelPath
            if (Test-Path -Path $projectMoFile) {
                if (-not (Test-Path -Path $dstMessagesDir)) {
                    New-Item -ItemType Directory -Path $dstMessagesDir -Force | Out-Null
                }
                Copy-Item -Path $projectMoFile -Destination $dstMessagesDir -Force
                Write-Host "  Copied: $(Split-Path -Leaf $projectMoFile) ($localeName)" -ForegroundColor Gray
            }
        }

        # Copy Qt base translations
        if ($QtTranslationsDir -and $QtTranslationsDestinationDir) {
            if (-not (Test-Path -Path $QtTranslationsDestinationDir)) {
                New-Item -ItemType Directory -Path $QtTranslationsDestinationDir -Force | Out-Null
            }
            Get-ChildItem -Path $QtTranslationsDir -Filter "qtbase_$localeName.qm" -ErrorAction SilentlyContinue | ForEach-Object {
                Copy-Item -Path $_.FullName -Destination $QtTranslationsDestinationDir -Force
            }
        }

        Write-Host "  Copied translations for: $localeName" -ForegroundColor Gray
    }
}

<#
.SYNOPSIS
    Deploy a KDE Framework 6 + Qt Widgets application bundle on macOS
.DESCRIPTION
    Performs complete macOS deployment:
    1. Validates prerequisites (app bundle, kf6redist installation, main executable)
    2. Copies KF6 libraries with @rpath resolution
    3. Copies the KF6 integration plugins selected via KfPlugins
    4. Resolves plugin dependencies
    5. Copies translations (KF6 + project-specific)
    6. Runs macdeployqt for code signing
    7. Creates DMG installer
.PARAMETER AppBundle
    Path to the .app bundle (e.g., "build/MyApp.app"). Must exist.
.PARAMETER BinaryName
    Name of the main executable inside MacOS directory (e.g., "myapp")
.PARAMETER BuildDir
    Path to the build directory (for resolving project-specific libraries)
.PARAMETER Kf6InstallDir
    Path to KF6 installation directory. Must exist. Typically from $env:KF6REDIST_DIR.
.PARAMETER QtDir
    Path to the Qt installation directory (for Qt translations)
.PARAMETER Locales
    Array of locale codes to copy (e.g., @("zh_CN", "en"))
.PARAMETER ProjectMoPathPattern
    Path pattern for project .mo files relative to BuildDir. Use {locale} as placeholder.
    Example: "po/locale/{locale}/LC_MESSAGES/myapp.mo"
.PARAMETER CopyVCRuntime
    Copy the VC++ runtime DLLs actually imported by the deployed binaries
    (app-local; required for MSIX/Store which cannot install the classic
    VC++ Redistributable).
.PARAMETER DmgName
    Name for the final DMG file (without extension). If not specified, uses app bundle name.
.PARAMETER KfPlugins
    KF6 integration plugins to ship, selected from: 'kiconthemes'
    (KIconEnginePlugin for icon theming), 'breeze' (Breeze style) and 'sonnet'
    (spell check backends, deployed into PlugIns/kf6/). Sonnet is loaded by KF6
    at runtime from <libraryPaths>/kf6/<name>/, so it is never picked up by
    dependency scanning and must be copied explicitly. Empty deploys none of
    them.
.EXAMPLE
    Deploy-MacOSAppBundle -AppBundle "build/MyApp.app" -BinaryName "myapp" `
        -BuildDir "build" -Kf6InstallDir $env:KF6REDIST_DIR `
        -Locales @("zh_CN") `
        -ProjectMoPathPattern "po/locale/{locale}/LC_MESSAGES/myapp.mo" `
        -DmgName "my-application" -KfPlugins @("kiconthemes", "breeze", "sonnet")
#>
function Deploy-MacOSAppBundle {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$AppBundle,
        [Parameter(Mandatory)]
        [string]$BinaryName,
        [Parameter(Mandatory)]
        [string]$BuildDir,
        [Parameter(Mandatory)]
        [string]$Kf6InstallDir,
        [Parameter(Mandatory)]
        [string]$QtDir,
        [Parameter(Mandatory)]
        [string[]]$Locales,
        [string]$ProjectMoPathPattern,
        [string]$DmgName,
        [ValidateSet('kiconthemes', 'breeze', 'sonnet')]
        [string[]]$KfPlugins = @()
    )

    # Validate prerequisites
    if (-not (Test-Path -Path $AppBundle)) {
        throw "App bundle not found: $AppBundle. Please build the project first."
    }
    if (-not (Test-Path -Path $Kf6InstallDir)) {
        throw "KF6 installation directory not found: $Kf6InstallDir"
    }
    $binaryPath = Join-Path $AppBundle "Contents/MacOS/$BinaryName"
    if (-not (Test-Path -Path $binaryPath)) {
        throw "Main executable not found: $binaryPath"
    }

    #region Copy dynamic libraries
    Write-Host "Copying libraries ..." -ForegroundColor Yellow

    $frameworksDir = Join-Path $AppBundle "Contents/Frameworks"
    if (-not (Test-Path -Path $frameworksDir)) {
        New-Item -ItemType Directory -Path $frameworksDir -Force | Out-Null
    }

    $binaryPath = Join-Path $AppBundle "Contents/MacOS/$BinaryName"
    $processedLibs = @{}
    Copy-RpathDependencies -BinaryPath $binaryPath -FrameworksDir $frameworksDir `
        -Kf6InstallDir $Kf6InstallDir -AdditionalSearchPaths @($BuildDir) -Processed $processedLibs
    #endregion

    #region Copy plugins
    Write-Host "Copying plugins ..." -ForegroundColor Yellow

    $pluginsDir = Join-Path $AppBundle "Contents/PlugIns"

    # Copy KIconEnginePlugin for icon theming (opt-in via KfPlugins)
    if ($KfPlugins -contains 'kiconthemes') {
        $iconEngineSrc = Join-Path $Kf6InstallDir "lib/plugins/kiconthemes6/iconengines/KIconEnginePlugin.so"
        $iconEngineDst = Join-Path $pluginsDir "iconengines/KIconEnginePlugin.so"
        if (Test-Path -Path $iconEngineSrc) {
            $iconEngineDir = Join-Path $pluginsDir "iconengines"
            if (-not (Test-Path -Path $iconEngineDir)) {
                New-Item -ItemType Directory -Path $iconEngineDir -Force | Out-Null
            }
            Copy-Item -Path $iconEngineSrc -Destination $iconEngineDst -Force
            Write-Host "  Copied: KIconEnginePlugin.so" -ForegroundColor Gray
        }
    }

    # Copy Breeze style (opt-in via KfPlugins)
    if ($KfPlugins -contains 'breeze') {
        $breezeStyleSrc = Join-Path $Kf6InstallDir "lib/plugins/styles/breeze6.so"
        $breezeStyleDst = Join-Path $pluginsDir "styles/breeze6.so"
        if (Test-Path -Path $breezeStyleSrc) {
            $breezeStyleDir = Join-Path $pluginsDir "styles"
            if (-not (Test-Path -Path $breezeStyleDir)) {
                New-Item -ItemType Directory -Path $breezeStyleDir -Force | Out-Null
            }
            Copy-Item -Path $breezeStyleSrc -Destination $breezeStyleDst -Force
            Write-Host "  Copied: breeze6.so" -ForegroundColor Gray
        }
    }

    # Copy extra KF6 plugins requested by the caller. KF6 loads these at
    # runtime from <libraryPaths>/kf6/<name>/ (e.g. Sonnet's Loader scans
    # PlugIns/kf6/sonnet/), and PlugIns is in the bundle's library paths.
    foreach ($kfPluginName in ($KfPlugins | Where-Object { $_ -eq 'sonnet' })) {
        $kfPluginSrcDir = Join-Path $Kf6InstallDir "lib/plugins/kf6/$kfPluginName"
        if (-not (Test-Path -Path $kfPluginSrcDir)) {
            Write-Warning "Extra KF plugin directory not found: $kfPluginSrcDir"
            continue
        }
        $kfPluginDstDir = Join-Path $pluginsDir "kf6/$kfPluginName"
        if (-not (Test-Path -Path $kfPluginDstDir)) {
            New-Item -ItemType Directory -Path $kfPluginDstDir -Force | Out-Null
        }
        Copy-Item -Path "$kfPluginSrcDir/*" -Destination $kfPluginDstDir -Force
        Write-Host "  Copied: kf6/$kfPluginName plugins" -ForegroundColor Gray
    }

    # Copy dependencies for plugins
    Write-Host "Resolving plugin dependencies ..." -ForegroundColor Yellow
    Get-ChildItem -Path $pluginsDir -Recurse -Include "*.so", "*.dylib" | ForEach-Object {
        Copy-RpathDependencies -BinaryPath $_.FullName -FrameworksDir $frameworksDir `
            -Kf6InstallDir $Kf6InstallDir -AdditionalSearchPaths @($BuildDir) -Processed $processedLibs
    }
    #endregion

    #region Copy translations
    Write-Host "Copying translations ..." -ForegroundColor Yellow
    $kf6LocaleDir = Join-Path $Kf6InstallDir "share/locale"
    $dstLocaleDir = Join-Path $AppBundle "Contents/Resources/locale"
    $qtTransDir = Join-Path $QtDir "translations"
    $qtTransDst = Join-Path $AppBundle "Contents/translations"
    Copy-Translations -Kf6LocaleDir $kf6LocaleDir -DestinationDir $dstLocaleDir `
        -Locales $Locales -BuildDir $BuildDir -ProjectMoPathPattern $ProjectMoPathPattern `
        -QtTranslationsDir $qtTransDir -QtTranslationsDestinationDir $qtTransDst
    #endregion

    #region Run macdeployqt and create DMG
    Write-Host "Running macdeployqt ..." -ForegroundColor Yellow

    $appBundleName = [System.IO.Path]::GetFileNameWithoutExtension($AppBundle)
    $expectedDmg = Join-Path $BuildDir "$appBundleName.dmg"
    $finalDmg = if ($DmgName) { Join-Path $BuildDir "$DmgName.dmg" } else { $expectedDmg }

    # Remove existing DMG files
    if (Test-Path -Path $expectedDmg) {
        Remove-Item -Path $expectedDmg -Force
    }
    if ($finalDmg -ne $expectedDmg -and (Test-Path -Path $finalDmg)) {
        Remove-Item -Path $finalDmg -Force
    }

    & macdeployqt $AppBundle -codesign="-" -dmg
    if ($LASTEXITCODE -ne 0) {
        Write-Error "macdeployqt failed with exit code $LASTEXITCODE"
        exit 1
    }

    # Rename to more descriptive name if needed
    if ($DmgName -and (Test-Path -Path $expectedDmg)) {
        Rename-Item -Path $expectedDmg -NewName "$DmgName.dmg" -Force
    }

    if (Test-Path -Path $finalDmg) {
        Write-Host "Created DMG: $finalDmg" -ForegroundColor Green
    }
    #endregion

    Write-Host "`nDeploy completed successfully" -ForegroundColor Green
}

<#
.SYNOPSIS
    Deploy a KDE Framework 6 + Qt Widgets application on Windows
.DESCRIPTION
    Performs complete Windows deployment:
    1. Runs windeployqt to copy Qt DLLs and plugins
    2. Copies the KF6 integration plugins selected via KfPlugins (empty = none)
        3. Resolves KF6 and missing Qt DLL dependencies (PE import table parsing)
        4. Optionally copies the VC++ runtime DLLs actually used (app-local, -CopyVCRuntime)
        5. Copies Breeze color schemes and theme data
        6. Copies translations (KF6 + Qt + project-specific)
.PARAMETER BinaryPath
    Path to the main executable (e.g., "install-msvc/bin/pnotepad.exe")
.PARAMETER Kf6InstallDir
    Path to KF6 installation directory (kf6redist-install)
.PARAMETER BuildDir
    Path to the build directory (for project translations)
.PARAMETER QtDir
    Path to the Qt installation directory (for Qt translations and DLLs)
.PARAMETER Locales
    Array of locale codes to copy (e.g., @("zh_CN"))
.PARAMETER ProjectMoPathPattern
    Path pattern for project .mo files relative to BuildDir. Use {locale} as placeholder.
.EXAMPLE
    Deploy-WindowsApp -BinaryPath "install-msvc/bin/pnotepad.exe" `
        -Kf6InstallDir $env:CMAKE_PREFIX_PATH -BuildDir "build-msvc" `
        -QtDir $env:QT_DIR -Locales @("zh_CN") `
        -ProjectMoPathPattern "po/locale/{locale}/LC_MESSAGES/pineapple-notepad.mo"
#>
function Deploy-WindowsApp {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$BinaryPath,
        # KF6 integration is opt-in: pass Kf6InstallDir together with the
        # KfPlugins entries to ship. Projects not using KF6 set neither and get
        # a KF6-free deployment.
        [string]$Kf6InstallDir = "",
        [ValidateSet('kiconthemes', 'breeze', 'sonnet')]
        [string[]]$KfPlugins = @(),
        [Parameter(Mandatory)]
        [string]$BuildDir,
        [Parameter(Mandatory)]
        [string]$QtDir,
        [Parameter(Mandatory)]
        [string[]]$Locales,
        [string]$ProjectMoPathPattern,
        [switch]$CopyVCRuntime
    )

    # Validate prerequisites
    if (-not (Test-Path -Path $BinaryPath)) {
        throw "Binary not found: $BinaryPath. Please build the project first."
    }
    $kf6DirAvailable = -not [string]::IsNullOrWhiteSpace($Kf6InstallDir) -and
        (Test-Path -LiteralPath $Kf6InstallDir -PathType Container)
    if ($KfPlugins.Count -gt 0 -and -not $kf6DirAvailable) {
        throw "KfPlugins requested but KF6 installation directory not found: '$Kf6InstallDir'"
    }

    if ($env:GITHUB_ACTIONS -eq "true") {
        Write-Host "::group::Deploying: $BinaryPath"
    }

    $binDir = Split-Path -Path $BinaryPath -Parent

    #region Run windeployqt
    Write-Host "Running windeployqt ..." -ForegroundColor Yellow
    Check-ExecutableExists "windeployqt"
    Invoke-ExternalCommand -ScriptBlock {
        windeployqt --verbose=2 --no-compiler-runtime --no-quick-import --no-translations `
            --no-opengl-sw --no-system-d3d-compiler --no-system-dxc-compiler `
            --skip-plugin-types tls,networkinformation "$BinaryPath"
    } -Description "windeployqt"

    #region Copy KF6 plugins (before dependency resolution so their deps are included)
    if ($kf6DirAvailable -and $KfPlugins.Count -gt 0) {
        Write-Host "Copying KF6 plugins ..." -ForegroundColor Yellow

        if ($KfPlugins -contains 'kiconthemes') {
            $iconEngineSrc = Join-Path $Kf6InstallDir "lib\plugins\kiconthemes6\iconengines\KIconEnginePlugin.dll"
            if (Test-Path -Path $iconEngineSrc) {
                $iconEngineDir = Join-Path $binDir "iconengines"
                if (-not (Test-Path -Path $iconEngineDir)) {
                    New-Item -ItemType Directory -Path $iconEngineDir -Force | Out-Null
                }
                Copy-Item -Path $iconEngineSrc -Destination (Join-Path $iconEngineDir "KIconEnginePlugin.dll") -Force
                Write-Host "  Copied: KIconEnginePlugin.dll" -ForegroundColor Gray
            }
        }

        if ($KfPlugins -contains 'breeze') {
            $breezeSrc = Join-Path $Kf6InstallDir "lib\plugins\styles\breeze6.dll"
            if (Test-Path -Path $breezeSrc) {
                $stylesDir = Join-Path $binDir "styles"
                if (-not (Test-Path -Path $stylesDir)) {
                    New-Item -ItemType Directory -Path $stylesDir -Force | Out-Null
                }
                Copy-Item -Path $breezeSrc -Destination (Join-Path $stylesDir "breeze6.dll") -Force
                Write-Host "  Copied: breeze6.dll" -ForegroundColor Gray
            }
        }

        # Copy Sonnet spell check backend plugins. Sonnet's Loader scans
        # <libraryPaths>/kf6/sonnet/ at runtime, and the application
        # directory (bin) is in the library paths.
        if ($KfPlugins -contains 'sonnet') {
            $sonnetSrcDir = Join-Path $Kf6InstallDir "lib\plugins\kf6\sonnet"
            if (Test-Path -Path $sonnetSrcDir) {
                $sonnetDstDir = Join-Path $binDir "kf6\sonnet"
                if (-not (Test-Path -Path $sonnetDstDir)) {
                    New-Item -ItemType Directory -Path $sonnetDstDir -Force | Out-Null
                }
                Copy-Item -Path "$sonnetSrcDir\*" -Destination $sonnetDstDir -Force
                Write-Host "  Copied: sonnet spell check plugins" -ForegroundColor Gray
            }
        }
    }
    #endregion

    #region Resolve missing Qt DLL dependencies (and KF6 ones, when shipping KF6)
    Write-Host "Resolving dependencies ..." -ForegroundColor Yellow
    $qtDirBin = Join-Path $QtDir "bin"

    # Collect plugin binaries to scan (they're loaded at runtime, not in dumpbin's dependency chain)
    $pluginBinaries = Get-ChildItem -Path $binDir -Recurse -Filter "*.dll" |
                      Where-Object { $_.DirectoryName -ne $binDir } |
                      Select-Object -ExpandProperty FullName

    $searchDirs = @{
        "^Qt6" = $qtDirBin
    }
    if ($kf6DirAvailable) {
        # Lookup rules only: they fire solely for DLL names that are actually
        # imported by something already present in the deployment directory.
        $kf6BinDir = Join-Path $Kf6InstallDir "bin"
        $searchDirs["^KF6"] = $kf6BinDir
        $searchDirs["^(intl|z)\.dll$"] = $kf6BinDir
    }
    Copy-PEDependencies -BinaryPath $BinaryPath -DestinationDir $binDir `
        -SearchDirectories $searchDirs -AdditionalBinaries $pluginBinaries
    #endregion

    #region Copy VC++ runtime (app-local)
    # MSIX cannot install the classic VC++ Redistributable, and Store policy
    # requires external dependencies to be disclosed; shipping the CRT DLLs
    # app-locally avoids both. windeployqt's --compiler-runtime is a no-op for
    # official (system) Qt builds, hence this explicit copy.
    if ($CopyVCRuntime) {
        Write-Host "Copying VC++ runtime (app-local) ..." -ForegroundColor Yellow
        $null = Copy-VCRuntimeAppLocal -DestinationDir $binDir
    }
    #endregion

    #region Copy Breeze data files
    if ($kf6DirAvailable) {
        Write-Host "Copying Breeze data files ..." -ForegroundColor Yellow
        $kf6DataDir = Join-Path $Kf6InstallDir "bin\data"

        # Color schemes
        $colorSchemesSrc = Join-Path $kf6DataDir "color-schemes"
        if (Test-Path -Path $colorSchemesSrc) {
            $colorSchemesDst = Join-Path $binDir "data\color-schemes"
            if (-not (Test-Path -Path $colorSchemesDst)) {
                New-Item -ItemType Directory -Path $colorSchemesDst -Force | Out-Null
            }
            Copy-Item -Path "$colorSchemesSrc\*" -Destination $colorSchemesDst -Force
            Write-Host "  Copied: color-schemes" -ForegroundColor Gray
        }

        # KStyle theme
        $kstyleSrc = Join-Path $kf6DataDir "kstyle"
        if (Test-Path -Path $kstyleSrc) {
            $kstyleDst = Join-Path $binDir "data\kstyle"
            Copy-Item -Path $kstyleSrc -Destination $kstyleDst -Recurse -Force
            Write-Host "  Copied: kstyle themes" -ForegroundColor Gray
        }
    }
    #endregion

    #region Copy translations
    Write-Host "Copying translations ..." -ForegroundColor Yellow
    $kf6LocaleDir = if ($kf6DirAvailable) { Join-Path (Join-Path $Kf6InstallDir "bin\data") "locale" } else { "" }
    $dstLocaleDir = Join-Path $binDir "data\locale"
    $qtTransDir = Join-Path $QtDir "translations"
    $qtTransDst = Join-Path $binDir "translations"
    Copy-Translations -Kf6LocaleDir $kf6LocaleDir -DestinationDir $dstLocaleDir `
        -Locales $Locales -BuildDir $BuildDir -ProjectMoPathPattern $ProjectMoPathPattern `
        -QtTranslationsDir $qtTransDir -QtTranslationsDestinationDir $qtTransDst
    #endregion

    Write-Host "`nDeploy completed successfully" -ForegroundColor Green

    if ($env:GITHUB_ACTIONS -eq "true") {
        Write-Host "::endgroup::"
    }
}

<#
.SYNOPSIS
    Initialize Visual Studio Developer Shell environment
.DESCRIPTION
    Locates Visual Studio installation using vswhere.exe, imports DevShell module,
    and configures build environment variables (PATH, LIB, INCLUDE, etc.).

    Sets $env:VS_DEV_SHELL_INITIALIZED to prevent duplicate initialization.
    Idempotent - safe to call multiple times in the same session.
.PARAMETER DevCmdArguments
    Optional arguments to pass to underlying vsdevcmd.bat (e.g., "-arch=x64")
.PARAMETER Force
    Force re-initialization even if environment is already configured
.EXAMPLE
    Initialize-VSDevShell
.EXAMPLE
    Initialize-VSDevShell -DevCmdArguments "-arch=x64 -host_arch=x64"
#>
function Initialize-VSDevShell {
    [CmdletBinding()]
    param(
        [string]$DevCmdArguments,
        [switch]$Force
    )

    # Skip if already initialized and not forced
    if (-not $Force -and $env:VS_DEV_SHELL_INITIALIZED) {
        Write-Host "Visual Studio Developer Shell already initialized at: $env:VS_DEV_SHELL_INITIALIZED" -ForegroundColor Green
        return
    }

    # Locate vswhere.exe
    $vswherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswherePath -PathType Leaf)) {
        throw "vswhere.exe not found at: $vswherePath"
    }

    # Query latest VS installation with VC++ tools
    $vsPath = & $vswherePath -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
    if (-not $vsPath) {
        throw "Could not find Visual Studio installation with VC++ tools"
    }

    Write-Host "Using Visual Studio installation at: $vsPath" -ForegroundColor Cyan

    # Import DevShell module if not already loaded
    $devShellModuleName = "Microsoft.VisualStudio.DevShell"
    if (-not (Get-Module -Name $devShellModuleName -ErrorAction SilentlyContinue)) {
        $devShellModulePath = Join-Path $vsPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
        if (-not (Test-Path -LiteralPath $devShellModulePath -PathType Leaf)) {
            throw "DevShell module not found at: $devShellModulePath"
        }
        Import-Module $devShellModulePath -Force -ErrorAction Stop
    }

    # Enter VS Developer Shell environment
    $enterParams = @{
        VsInstallPath = $vsPath
        SkipAutomaticLocation = $true
        SetDefaultWindowTitle = $true
    }
    if ($DevCmdArguments) { $enterParams.DevCmdArguments = $DevCmdArguments }

    Enter-VsDevShell @enterParams -ErrorAction Stop

    # Mark as initialized
    $env:VS_DEV_SHELL_INITIALIZED = $vsPath
    Write-Host "Visual Studio Developer Shell initialized successfully" -ForegroundColor Green
}
