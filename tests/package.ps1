$ErrorActionPreference = 'Stop'
$PluginRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$CallerDirectory = $PSScriptRoot
$Before = (Get-Location).Path
$PreviousNativeExit = $global:LASTEXITCODE
$global:NextStreamCmakeTestDirectory = $null
# Stop before any build/package/deploy mutation; inspect the real script's CMake call.
function cmake {
    $global:NextStreamCmakeTestDirectory = (Get-Location).Path
    $global:LASTEXITCODE = 37
}
try {
    Set-Location $CallerDirectory
    $Caught = $false
    try { & (Join-Path $PluginRoot 'package.ps1') -SkipDeploy } catch {
        if ($_.Exception.Message -notlike 'Command failed with exit code 37:*') { throw }
        $Caught = $true
    }
    if (-not $Caught) { throw 'Native CMake failure was not propagated' }
    if ($global:NextStreamCmakeTestDirectory -ne $PluginRoot) { throw "CMake ran in caller directory: $global:NextStreamCmakeTestDirectory" }
    if ((Get-Location).Path -ne $CallerDirectory) { throw 'Caller location was not restored after failure' }
    Write-Output 'PASS packaging selects plugin root and restores caller location on CMake failure'
} finally {
    Set-Location $Before
    Remove-Item Function:cmake
    Remove-Variable NextStreamCmakeTestDirectory -Scope Global
    $global:LASTEXITCODE = $PreviousNativeExit
}
