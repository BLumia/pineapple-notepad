<#
.SYNOPSIS
  Thin wrapper around pmsix that packages Pineapple Notepad (install-msvc\bin
  payload) as an MSIX.

  All packaging properties (identity, display names incl. per-language
  overrides, icons, file associations, context menu, languages) live in
  pineapple-notepad.msix.toml next to this script; this wrapper only handles
  certificate creation, the pmsix invocation and optional installation.

  pmsix must be installed beforehand:
      cargo install --path C:\msys64\home\Gary\Sources\pineapple-msix

.EXAMPLE
  .\make-msix.ps1
  # Unsigned package (default)
.EXAMPLE
  .\make-msix.ps1 -Sign -Install
  # Self-signed package + install
.EXAMPLE
  .\make-msix.ps1 -PfxPath C:\certs\real.pfx -PfxPassword "secret" -Install -ForceRemoveOld
  # Real-cert signed package + install
.EXAMPLE
  .\make-msix.ps1 -Publisher "CN=67170278-1C36-4240-9886-28405ABDB11A"
  # Unsigned package with a custom Identity Publisher (e.g. the Store-assigned
  # identity from Partner Center); ignored when -PfxPath/-Sign derive the
  # publisher from a certificate
#>
param(
    [string]$InstallDir,
    [string]$Dll,
    [string]$OutDir,
    [string]$ConfigPath,
    [string]$PfxPath,
    [string]$PfxPassword,
    [string]$SelfSignedSubject = "PineappleNotepadTest",
    [string]$Publisher,
    [switch]$Sign,
    [switch]$NoTimestamp,
    [string]$TimestampServer = "http://timestamp.digicert.com",
    [switch]$Install,
    [switch]$ForceRemoveOld
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $scriptDir "..\.."))

if (-not $InstallDir) { $InstallDir = Join-Path $repoRoot "install-msvc\bin" }
if (-not $OutDir) { $OutDir = Join-Path $repoRoot "install-msvc\msix" }
if (-not $ConfigPath) { $ConfigPath = Join-Path $scriptDir "pineapple-notepad.msix.toml" }

# --- pmsix tool ---
$pmsixCmd = Get-Command pmsix -ErrorAction SilentlyContinue
if (-not $pmsixCmd) {
    throw "pmsix not found on PATH. Install it first:`n  cargo install --path C:\msys64\home\Gary\Sources\pineapple-msix"
}

if (-not (Test-Path $InstallDir)) { throw "App payload dir not found: $InstallDir" }
if (-not (Test-Path $ConfigPath)) { throw "Config not found: $ConfigPath" }

# --- Context menu DLL discovery (payload first, then the build tree) ---
if (-not $Dll) {
    $payloadDll = Join-Path $InstallDir "pnotepadshellext.dll"
    if (Test-Path $payloadDll) {
        $Dll = $payloadDll
    } else {
        $candidates = Get-ChildItem -Path (Join-Path $repoRoot "build-msvc") -Recurse -Filter "pnotepadshellext.dll" -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending
        if ($candidates) { $Dll = $candidates[0].FullName }
    }
}

# --- Publisher / certificate handling ---
# Identity Publisher precedence:
#   -PfxPath  -> the certificate's subject (must match; a conflicting -Publisher is warned and ignored)
#   -Sign     -> CN=<SelfSignedSubject> (same caveat)
#   -Publisher (e.g. a Store-assigned identity) -> used for an unsigned package
#   none      -> whatever the TOML config declares (pmsix falls back to the unsigned OID)
$cerFull = $null
$pfxFull = $null
$signPassword = $null

if ($PfxPath) {
    $pfxFull = (Resolve-Path $PfxPath).Path
    if ($PfxPassword) {
        $cert = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2($pfxFull, $PfxPassword)
    } else {
        $cert = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2($pfxFull)
    }
    $Publisher = $cert.Subject
    $signPassword = $PfxPassword
    if ($PSBoundParameters.ContainsKey("Publisher") -and $Publisher -ne $PSBoundParameters["Publisher"]) {
        Write-Warning "Ignoring -Publisher '$($PSBoundParameters['Publisher'])': it must match the signing certificate subject ($Publisher)."
    }
    Write-Host "Using provided certificate. Publisher = $Publisher"
} elseif ($Sign) {
    $Publisher = "CN=$SelfSignedSubject"
    if ($PSBoundParameters.ContainsKey("Publisher") -and $Publisher -ne $PSBoundParameters["Publisher"]) {
        Write-Warning "Ignoring -Publisher '$($PSBoundParameters['Publisher'])': -Sign derives the publisher from the self-signed certificate subject ($Publisher)."
    }
    $certDir = Join-Path $OutDir "cert"
    New-Item -ItemType Directory -Path $certDir -Force | Out-Null
    $pfxFull = Join-Path $certDir "$SelfSignedSubject.pfx"
    $cerFull = Join-Path $certDir "$SelfSignedSubject.cer"
    $signPassword = "PineappleNotepadTest123"
    if (-not (Test-Path $pfxFull)) {
        Write-Host "Creating self-signed code-signing certificate CN=$SelfSignedSubject ..."
        $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject "CN=$SelfSignedSubject" `
            -CertStoreLocation Cert:\CurrentUser\My -KeyExportPolicy Exportable `
            -KeyAlgorithm RSA -KeyLength 2048 -HashAlgorithm SHA256 -NotAfter (Get-Date).AddYears(3)
        $securePwd = ConvertTo-SecureString $signPassword -AsPlainText -Force
        Export-PfxCertificate -Cert $cert -FilePath $pfxFull -Password $securePwd -Force | Out-Null
        Write-Host "Self-signed test certificate created: $pfxFull"
    }
    if (-not (Test-Path $cerFull)) {
        $c = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2($pfxFull, $signPassword)
        [System.IO.File]::WriteAllBytes($cerFull,
            $c.Export([System.Security.Cryptography.X509Certificates.X509ContentType]::Cert))
    }
    Write-Host "Public cert exported for manual trust: $cerFull"
}

# --- Read package identity from the config (to locate the produced msix) ---
# Minimal section-aware TOML string reader: enough for package.name/version.
function Get-TomlStringValue([string]$Text, [string]$Section, [string]$Key) {
    $inSection = $false
    foreach ($line in ($Text -split "\r?\n")) {
        $t = $line.Trim()
        if ($t -match '^\[(.+)\]$') { $inSection = ($Matches[1].Trim() -eq $Section); continue }
        if ($inSection -and $t -match ('^' + [regex]::Escape($Key) + '\s*=\s*"([^"]*)"')) {
            return $Matches[1]
        }
    }
    return $null
}

$configRaw = Get-Content $ConfigPath -Raw
$pkgName = Get-TomlStringValue $configRaw "package" "name"
$pkgVersion = Get-TomlStringValue $configRaw "package" "version"
if (-not $pkgName -or -not $pkgVersion) {
    throw "Failed to read package.name / package.version from $ConfigPath"
}
$packagePath = Join-Path $OutDir ("{0}_{1}.msix" -f $pkgName, $pkgVersion)

# --- Run pmsix ---
$pmArgs = @(
    "--config", $ConfigPath,
    "--payload", $InstallDir,
    "--out", $OutDir
)
if ($Dll) { $pmArgs += @("--dll", $Dll) }
if ($Publisher) { $pmArgs += @("--publisher", $Publisher) }
if ($pfxFull) {
    $pmArgs += @("--pfx", $pfxFull)
    if ($signPassword) { $pmArgs += @("--password", $signPassword) }
}
if ($NoTimestamp) {
    $pmArgs += "--no-timestamp"
} else {
    $pmArgs += @("--timestamp-server", $TimestampServer)
}

& $pmsixCmd.Source @pmArgs
if ($LASTEXITCODE -ne 0) { throw "pmsix failed (exit code $LASTEXITCODE)" }

# --- Optional install ---
$isSigned = [bool]$pfxFull
if ($Install) {
    if ($ForceRemoveOld) {
        Get-AppxPackage -Name $pkgName -ErrorAction SilentlyContinue | Remove-AppxPackage -ErrorAction SilentlyContinue
    }
    Write-Host "Installing $packagePath ..."
    try {
        if (-not $isSigned) {
            Write-Host "Unsigned package (publisher uses the unsigned namespace OID). Installing with -AllowUnsigned ..."
            Add-AppxPackage -Path $packagePath -AllowUnsigned -ErrorAction Stop
        } else {
            Add-AppxPackage -Path $packagePath -ErrorAction Stop
        }
        Write-Host "Installed. Sign out/in or restart Explorer to see the menu item."
    } catch {
        Write-Warning "Add-AppxPackage failed: $($_.Exception.Message)"
        if (-not $isSigned) {
            Write-Host "Unsigned packages with executable content must be installed from an elevated (Administrator) PowerShell." -ForegroundColor Yellow
        } elseif (-not $PfxPath) {
            Write-Host "Untrusted-publisher errors (0x800B0109 / 0x80073CF9) are expected for a self-signed test cert." -ForegroundColor Yellow
            Write-Host "  1. Trust the cert: double-click $cerFull -> Install Certificate -> Current User -> Trusted Root Certificate Store, then re-run with -Install." -ForegroundColor Yellow
            Write-Host "  or 2. Enable Developer Mode: Settings -> Privacy & security -> For developers -> Developer Mode." -ForegroundColor Yellow
            Write-Host "  or 3. Use a real code-signing cert: -PfxPath <file.pfx> -PfxPassword <pw>." -ForegroundColor Yellow
        }
    }
}

Write-Host ""
Write-Host "Done. Package: $packagePath"
Write-Host "Uninstall: Get-AppxPackage -Name $pkgName | Remove-AppxPackage"
