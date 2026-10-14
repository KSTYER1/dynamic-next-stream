param([string]$DependenciesRoot = '')
$ErrorActionPreference = 'Stop'
$PluginRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$Deps = if ($DependenciesRoot) { (Resolve-Path -LiteralPath $DependenciesRoot).Path } else { Join-Path $PluginRoot '.deps' }
$Qt = Join-Path $Deps 'obs-deps-qt6-2025-07-11-x64'
$Build = Join-Path $PluginRoot 'build-tests'
& cmake -S (Join-Path $PSScriptRoot 'runtime') -B $Build -G 'Visual Studio 17 2022' -A x64 "-DQt6_DIR=$Qt/lib/cmake/Qt6" "-DOBS_DEPS_DIR=$Deps"
if ($LASTEXITCODE -ne 0) { throw 'Review harness configuration failed' }
& cmake --build $Build --config RelWithDebInfo
if ($LASTEXITCODE -ne 0) { throw 'Review harness compilation failed' }
$SavedPath = $env:PATH
$SavedQpa = $env:QT_QPA_PLATFORM
$SavedPlugins = $env:QT_PLUGIN_PATH
try {
    $env:PATH = "$Qt/bin;$Deps/bin/64bit;$Deps/obs-deps-2025-07-11-x64/bin;$SavedPath"
    $env:QT_PLUGIN_PATH = "$Qt/plugins"
    $env:QT_QPA_PLATFORM = 'minimal'
    # A corrupt-JSON fixture intentionally writes a diagnostic to stderr.
    # Keep that diagnostic separate from PowerShell 5's NativeCommandError handling.
    $ReviewProcess = Start-Process -FilePath (Join-Path $Build 'RelWithDebInfo/next_stream_review.exe') -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $Build 'runtime.log') -RedirectStandardError (Join-Path $Build 'runtime-stderr.log')
    $ReviewExit = $ReviewProcess.ExitCode
    Get-Content -LiteralPath (Join-Path $Build 'runtime.log')
    Get-Content -LiteralPath (Join-Path $Build 'runtime-stderr.log')
    if ($ReviewExit -ne 0) { throw "Review reproduction failed: $ReviewExit" }
} finally {
    $env:PATH = $SavedPath
    $env:QT_QPA_PLATFORM = $SavedQpa
    $env:QT_PLUGIN_PATH = $SavedPlugins
}
