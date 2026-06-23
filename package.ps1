<#
.SYNOPSIS
    Builds Dynamic Next Stream and creates local release artifacts.

.USAGE
    .\package.ps1
    .\package.ps1 -Version "1.0.6"
    .\package.ps1 -SkipDeploy
    .\package.ps1 -DeployUserPlugin
#>
param(
    [string]$Version = "1.0.6",
    [switch]$DeployUserPlugin,
    [switch]$SkipDeploy
)

$ErrorActionPreference = "Stop"
$Root = $PSScriptRoot
$Build = Join-Path $Root "build_x64"
$Config = "RelWithDebInfo"
$Dll = Join-Path $Build "$Config\dynamic-next-stream.dll"
$Pdb = Join-Path $Build "$Config\dynamic-next-stream.pdb"
$Data = Join-Path $Root "data"
$Dist = Join-Path $Root "dist"
$BuildspecPath = Join-Path $Root "buildspec.json"
$Nsis = "${env:ProgramFiles(x86)}\NSIS\makensis.exe"
if (-not (Test-Path $Nsis)) { $Nsis = "${env:ProgramFiles}\NSIS\makensis.exe" }

function Invoke-Native {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,
        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]]$Arguments
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $Command $($Arguments -join ' ')"
    }
}

$Buildspec = Get-Content -LiteralPath $BuildspecPath -Raw | ConvertFrom-Json
if (-not $PSBoundParameters.ContainsKey("Version")) {
    $Version = $Buildspec.version
} elseif ($Version -ne $Buildspec.version) {
    throw "Package version $Version does not match buildspec.json version $($Buildspec.version)"
}

Write-Host "`n[1/5] Build $Config..." -ForegroundColor Cyan
Invoke-Native cmake --preset windows-x64
Invoke-Native cmake --build --preset windows-x64
if (-not (Test-Path $Dll)) { throw "Build failed: $Dll not found" }

Write-Host "`n[2/5] Create dist layout..." -ForegroundColor Cyan
New-Item -ItemType Directory -Force $Dist | Out-Null
$ResolvedRoot = (Resolve-Path -LiteralPath $Root).Path
$ResolvedDist = (Resolve-Path -LiteralPath $Dist).Path
if (-not $ResolvedDist.StartsWith($ResolvedRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to clean dist outside plugin root: $ResolvedDist"
}
Remove-Item (Join-Path $Dist "dynamic-next-stream-*") -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $Dist "dynamic-next-stream.dll") -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $Dist "dynamic-next-stream.pdb") -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $Dist "dynamic-next-stream.nsi") -Force -ErrorAction SilentlyContinue
Remove-Item (Join-Path $Dist "locale") -Recurse -Force -ErrorAction SilentlyContinue

$PackageName = "dynamic-next-stream-$Version"
$PackageDir = Join-Path $Dist $PackageName
$PortableName = "$PackageName-portable"
$PortableDir = Join-Path $Dist $PortableName

New-Item -ItemType Directory -Force $PackageDir | Out-Null
New-Item -ItemType Directory -Force (Join-Path $PackageDir "locale") | Out-Null
Copy-Item $Dll (Join-Path $PackageDir "dynamic-next-stream.dll") -Force
if (Test-Path $Pdb) { Copy-Item $Pdb (Join-Path $PackageDir "dynamic-next-stream.pdb") -Force }
Copy-Item (Join-Path $Data "locale\en-US.ini") (Join-Path $PackageDir "locale\") -Force
Copy-Item (Join-Path $Data "locale\de-DE.ini") (Join-Path $PackageDir "locale\") -Force

New-Item -ItemType Directory -Force (Join-Path $PortableDir "obs-plugins\64bit") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $PortableDir "data\obs-plugins\dynamic-next-stream\locale") | Out-Null
Copy-Item $Dll (Join-Path $PortableDir "obs-plugins\64bit\dynamic-next-stream.dll") -Force
if (Test-Path $Pdb) { Copy-Item $Pdb (Join-Path $PortableDir "obs-plugins\64bit\dynamic-next-stream.pdb") -Force }
Copy-Item (Join-Path $Data "locale\en-US.ini") (Join-Path $PortableDir "data\obs-plugins\dynamic-next-stream\locale\") -Force
Copy-Item (Join-Path $Data "locale\de-DE.ini") (Join-Path $PortableDir "data\obs-plugins\dynamic-next-stream\locale\") -Force

$ZipPath = Join-Path $Dist "$PortableName.zip"
Compress-Archive -Path (Join-Path $PortableDir "*") -DestinationPath $ZipPath -Force
Write-Host "  -> $ZipPath" -ForegroundColor Green

Write-Host "`n[3/5] Create NSIS script..." -ForegroundColor Cyan
$NsiPath = Join-Path $Dist "dynamic-next-stream.nsi"
$NsiScript = @"
Unicode True
!define PLUGIN_NAME    "dynamic-next-stream"
!define PLUGIN_VERSION "$Version"
!define PUBLISHER      "K_STYER1"

Name            "`${PLUGIN_NAME} `${PLUGIN_VERSION}"
OutFile         "dynamic-next-stream-`${PLUGIN_VERSION}-installer.exe"
InstallDir      "`$APPDATA\obs-studio\plugins\`${PLUGIN_NAME}"
InstallDirRegKey HKCU "Software\`${PLUGIN_NAME}" "InstallDir"
RequestExecutionLevel user

SetCompressor    /SOLID lzma
SetCompressorDictSize 8

!include "MUI2.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON "`${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "`${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "German"
!insertmacro MUI_LANGUAGE "English"

Section "Install plugin" SecMain
    SectionIn RO

    SetOutPath "`$INSTDIR\bin\64bit"
    File "dynamic-next-stream.dll"

    SetOutPath "`$INSTDIR\data\locale"
    File "locale\en-US.ini"
    File "locale\de-DE.ini"

    WriteRegStr HKCU "Software\`${PLUGIN_NAME}" "InstallDir" "`$INSTDIR"
    WriteUninstaller "`$INSTDIR\uninstall.exe"

    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\`${PLUGIN_NAME}" "DisplayName" "`${PLUGIN_NAME} `${PLUGIN_VERSION}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\`${PLUGIN_NAME}" "UninstallString" "`$INSTDIR\uninstall.exe"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\`${PLUGIN_NAME}" "Publisher" "`${PUBLISHER}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\`${PLUGIN_NAME}" "DisplayVersion" "`${PLUGIN_VERSION}"
SectionEnd

Section "Uninstall"
    Delete "`$INSTDIR\bin\64bit\dynamic-next-stream.dll"
    Delete "`$INSTDIR\data\locale\en-US.ini"
    Delete "`$INSTDIR\data\locale\de-DE.ini"
    Delete "`$INSTDIR\uninstall.exe"
    RMDir  "`$INSTDIR\bin\64bit"
    RMDir  "`$INSTDIR\bin"
    RMDir  "`$INSTDIR\data\locale"
    RMDir  "`$INSTDIR\data"
    RMDir  "`$INSTDIR"
    DeleteRegKey HKCU "Software\`${PLUGIN_NAME}"
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\`${PLUGIN_NAME}"
SectionEnd
"@

Set-Content -Path $NsiPath -Value $NsiScript -Encoding UTF8
Copy-Item $Dll (Join-Path $Dist "dynamic-next-stream.dll") -Force
if (Test-Path $Pdb) { Copy-Item $Pdb (Join-Path $Dist "dynamic-next-stream.pdb") -Force }
New-Item -ItemType Directory -Force (Join-Path $Dist "locale") | Out-Null
Copy-Item (Join-Path $Data "locale\en-US.ini") (Join-Path $Dist "locale\") -Force
Copy-Item (Join-Path $Data "locale\de-DE.ini") (Join-Path $Dist "locale\") -Force

Write-Host "`n[4/5] Build NSIS installer..." -ForegroundColor Cyan
if (-not (Test-Path $Nsis)) {
    throw "makensis.exe not found. Installer cannot be built."
} else {
    Push-Location $Dist
    try {
        Invoke-Native $Nsis "dynamic-next-stream.nsi"
    } finally {
        Pop-Location
    }
}

Write-Host "`n[5/5] Deploy..." -ForegroundColor Cyan
$NeedsDeploy = (-not $SkipDeploy) -or $DeployUserPlugin
if ($NeedsDeploy) {
    $ObsProcess = Get-Process obs64 -ErrorAction SilentlyContinue
    if ($ObsProcess) {
        throw "OBS is running. Close OBS before deploying the DLL."
    }
}

if ($SkipDeploy) {
    Write-Host "  -> skipped (-SkipDeploy)" -ForegroundColor DarkGray
} else {
    $ObsBeta = Join-Path ([Environment]::GetFolderPath("Desktop")) "OBS 32.1 BETA"
    if (Test-Path $ObsBeta) {
        $Dst64 = Join-Path $ObsBeta "obs-plugins\64bit"
        $DstData = Join-Path $ObsBeta "data\obs-plugins\dynamic-next-stream\locale"
        New-Item -ItemType Directory -Force $Dst64 | Out-Null
        New-Item -ItemType Directory -Force $DstData | Out-Null
        Copy-Item $Dll (Join-Path $Dst64 "dynamic-next-stream.dll") -Force
        if (Test-Path $Pdb) { Copy-Item $Pdb (Join-Path $Dst64 "dynamic-next-stream.pdb") -Force }
        Copy-Item (Join-Path $Data "locale\en-US.ini") $DstData -Force
        Copy-Item (Join-Path $Data "locale\de-DE.ini") $DstData -Force
        Write-Host "  -> Portable OBS ($ObsBeta)" -ForegroundColor Green
    } else {
        throw "Portable OBS not found: $ObsBeta"
    }
}

if ($DeployUserPlugin) {
    $ObsUser = Join-Path $env:APPDATA "obs-studio\plugins\dynamic-next-stream"
    $User64 = Join-Path $ObsUser "bin\64bit"
    $UserData = Join-Path $ObsUser "data\locale"
    New-Item -ItemType Directory -Force $User64 | Out-Null
    New-Item -ItemType Directory -Force $UserData | Out-Null
    Copy-Item $Dll (Join-Path $User64 "dynamic-next-stream.dll") -Force
    if (Test-Path $Pdb) { Copy-Item $Pdb (Join-Path $User64 "dynamic-next-stream.pdb") -Force }
    Copy-Item (Join-Path $Data "locale\en-US.ini") $UserData -Force
    Copy-Item (Join-Path $Data "locale\de-DE.ini") $UserData -Force
    Write-Host "  -> User plugin ($ObsUser)" -ForegroundColor Green
}

Write-Host "`n=== Done ===" -ForegroundColor Yellow
Write-Host "Dist: $Dist"
Get-ChildItem $Dist -File | Select-Object Name, @{N="KB";E={[math]::Round($_.Length/1KB,1)}}
