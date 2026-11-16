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

if (-not (Test-Path -LiteralPath "$InstallRoot/streamassistant-camera.dll" -PathType Leaf)) {
    throw "The installed plugin was not found at $InstallRoot. Build the project first."
}
if (-not (Test-Path -LiteralPath "$InstallRoot/data/locale/en-US.ini" -PathType Leaf)) {
    throw "The installed plugin data was not found at $InstallRoot. Install the build before packaging."
}

New-Item -ItemType Directory -Path $ReleaseRoot -Force | Out-Null
if (Test-Path -LiteralPath $ZipPath) {
    Remove-Item -LiteralPath $ZipPath -Force
}

# Package only the current runtime files. An incremental CMake install can leave
# the old bin/64bit layout and debug symbols in the install staging directory.
$PackageRoot = Join-Path $ReleaseRoot ('.package-windows-' + [guid]::NewGuid().ToString('N'))
$PluginRoot = Join-Path $PackageRoot 'streamassistant-camera'
New-Item -ItemType Directory -Path $PluginRoot | Out-Null
try {
    Copy-Item -LiteralPath "$InstallRoot/streamassistant-camera.dll" -Destination $PluginRoot
    Copy-Item -LiteralPath "$InstallRoot/data" -Destination $PluginRoot -Recurse
    Compress-Archive -Path $PluginRoot -DestinationPath $ZipPath -CompressionLevel Optimal
} finally {
    $ResolvedPackageRoot = (Resolve-Path -LiteralPath $PackageRoot).Path
    if ([System.IO.Path]::GetDirectoryName($ResolvedPackageRoot) -ne $ReleaseRoot) {
        throw "Refusing to remove package staging outside $ReleaseRoot."
    }
    Remove-Item -LiteralPath $ResolvedPackageRoot -Recurse -Force
}

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
