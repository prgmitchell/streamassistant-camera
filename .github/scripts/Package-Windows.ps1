[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string] $Target = 'x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo',
    [Parameter(Mandatory)]
    [ValidatePattern('^[0-9]+\.[0-9]+\.[0-9]+(?:-(?:beta|rc)\.[0-9]+|-[A-Za-z0-9.-]+)?$')]
    [string] $PackageVersion,
    [switch] $SkipInstaller
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Resolve-Path -LiteralPath "$PSScriptRoot/../.."
$ReleaseRoot = Join-Path $ProjectRoot 'release'
$InstallRoot = Join-Path $ReleaseRoot "$Configuration/streamassistant-camera"
$OutputBase = "streamassistant-camera-v${PackageVersion}-windows-${Target}"
$ZipPath = Join-Path $ReleaseRoot "${OutputBase}.zip"

if (-not (Test-Path -LiteralPath "$InstallRoot/bin/64bit/streamassistant-camera.dll")) {
    throw "The installed plugin was not found at $InstallRoot. Build the project first."
}

New-Item -ItemType Directory -Path $ReleaseRoot -Force | Out-Null
Get-ChildItem -LiteralPath $InstallRoot -Recurse -Filter '*.pdb' | Remove-Item -Force
if (Test-Path -LiteralPath $ZipPath) {
    Remove-Item -LiteralPath $ZipPath -Force
}

Compress-Archive -Path $InstallRoot -DestinationPath $ZipPath -CompressionLevel Optimal

if ($SkipInstaller) {
    return
}

$CompilerCandidates = @(
    (Join-Path $env:LOCALAPPDATA 'Programs/Inno Setup 6/ISCC.exe'),
    (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6/ISCC.exe'),
    (Join-Path $env:ProgramFiles 'Inno Setup 6/ISCC.exe')
)
$Compiler = $CompilerCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $Compiler) {
    throw 'Inno Setup 6 was not found. Install it or pass -SkipInstaller for ZIP-only packaging.'
}

& $Compiler `
    "/DSourceDir=$InstallRoot" `
    "/DPackageVersion=$PackageVersion" `
    "/DOutputDir=$ReleaseRoot" `
    (Join-Path $ProjectRoot 'installer/streamassistant-camera.iss')

if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup failed with exit code $LASTEXITCODE."
}
