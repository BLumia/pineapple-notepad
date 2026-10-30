param(
    [switch]$UpdateTranslations
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

. "$PSScriptRoot\buildutils.ps1"

#region Initialize build Env and VSDev Shell
. Initialize-BuildEnvironment -EnvironmentName "env-msys2"
#endregion

$cmakeArgs = @("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON")

# Offline builds need no special flags anymore: CMake (cmake/PnDependencies.cmake)
# automatically consumes the archives under offline/ when that directory exists.

if ($UpdateTranslations) {
    cmake --build "build" --target "update_translations"
} else {
    Build-CMakeProject `
        -SourcePath "." -RepoName "pineapple-notepad" `
        -ForceRebuild -BuildBaseDir "build" `
        -CMakeArgs $cmakeArgs `
        -SkipInstall -NoSourceIdentifierFolder
}