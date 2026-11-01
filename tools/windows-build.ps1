# Build DanketSub on Windows with the isolated toolchain in this repository.
#
# Usage (from a normal PowerShell prompt):
#   powershell -ExecutionPolicy Bypass -File tools/windows-build.ps1
#   powershell -ExecutionPolicy Bypass -File tools/windows-build.ps1 -Dist
#
# The script expects:
#   .venv\Scripts\{meson,ninja}.exe      (python -m venv .venv; pip install meson ninja)
#   .toolchain\vs\VC\Auxiliary\Build\vcvars64.bat  (Visual Studio Build Tools)
#   .toolchain\bin\7z.exe                (7-Zip standalone)
#   .toolchain\innosetup\app\ISCC.exe    (Inno Setup)
#   .toolchain\gettext\bin\msgfmt.exe    (gettext)

param(
    [ValidateSet('release', 'debugoptimized', 'debug')]
    [string]$BuildType = 'release',
    [string]$Credit = 'Serina765P',
    [switch]$Clean,
    [switch]$NoPackage,
    [switch]$Dist
)

$ErrorActionPreference = 'Stop'

# Python 3.14's UTF-8 mode decodes localized MSVC output as UTF-8 and crashes
# Meson's compiler probe on non-English Windows. Use the console code page.
$env:PYTHONUTF8 = '0'
$env:PYTHONIOENCODING = 'cp' + (Get-Culture).TextInfo.ANSICodePage

$root = Split-Path $PSScriptRoot -Parent
$buildDir = Join-Path $root 'build'
$venv = Join-Path $root '.venv'
$meson = Join-Path $venv 'Scripts\meson.exe'
$ninja = Join-Path $venv 'Scripts\ninja.exe'

foreach ($tool in @($meson, $ninja)) {
    if (!(Test-Path -LiteralPath $tool)) {
        throw "Missing $tool. Create the environment with:`n  python -m venv .venv`n  .venv\Scripts\pip install meson ninja"
    }
}

# Put the isolated helper tools first on PATH.
$toolDirs = @(
    (Join-Path $root '.venv\Scripts'),
    (Join-Path $root '.toolchain\7zip'),
    (Join-Path $root '.toolchain\bin'),
    (Join-Path $root '.toolchain\innosetup\app'),
    (Join-Path $root '.toolchain\gettext\bin')
)
foreach ($dir in $toolDirs) {
    if (Test-Path -LiteralPath $dir) {
        $env:PATH = "$dir;$env:PATH"
    }
}

function Find-VcVars {
    $candidates = @(
        (Join-Path $root '.toolchain\vs\VC\Auxiliary\Build\vcvars64.bat'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'),
        (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat')
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($install) {
            $candidate = Join-Path $install 'VC\Auxiliary\Build\vcvars64.bat'
            if (Test-Path -LiteralPath $candidate) { return $candidate }
        }
    }
    return $null
}

$vcvars = Find-VcVars
if (!$vcvars) {
    throw 'Could not find vcvars64.bat. Install Visual Studio Build Tools with the VCTools workload.'
}

Write-Host "Using $vcvars"
$envDump = & cmd.exe /c "`"$vcvars`" >nul 2>&1 && set"
foreach ($line in $envDump) {
    if ($line -match '^([^=]+)=(.*)$') {
        Set-Item -Path "env:$($matches[1])" -Value $matches[2]
    }
}
if (!(Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    throw 'cl.exe is not available after initializing the Visual Studio environment.'
}

if ($Clean -and (Test-Path -LiteralPath $buildDir)) {
    Remove-Item -LiteralPath $buildDir -Recurse -Force
}

$setupOptions = @(
    "--buildtype=$BuildType",
    '-Ddefault_library=static',
    '-Dfftw3=disabled',
    '-Denable_update_checker=false',
    "-Dcredit=$Credit",
    '-Dtests=false',
    '-Dfallback_nasm=true',
    '--force-fallback-for=zlib,harfbuzz,freetype2,fribidi'
)

Write-Host "Configuring $buildDir"
if (Test-Path -LiteralPath (Join-Path $buildDir 'build.ninja')) {
    & $meson setup --reconfigure $buildDir @setupOptions
} else {
    & $meson setup $buildDir @setupOptions
}
if ($LASTEXITCODE -ne 0) { throw "meson setup failed (exit $LASTEXITCODE)" }

Write-Host 'Building'
& $ninja -C $buildDir
if ($LASTEXITCODE -ne 0) { throw "ninja failed (exit $LASTEXITCODE)" }

if (!$NoPackage) {
    Write-Host 'Building installer'
    & $ninja -C $buildDir win-installer
    if ($LASTEXITCODE -ne 0) { throw "win-installer failed (exit $LASTEXITCODE)" }

    Write-Host 'Building portable package'
    & $ninja -C $buildDir win-portable
    if ($LASTEXITCODE -ne 0) { throw "win-portable failed (exit $LASTEXITCODE)" }
}

if ($Dist) {
    Write-Host 'Creating source tarball'
    & $meson dist --include-subprojects -C $buildDir
    if ($LASTEXITCODE -ne 0) { throw "meson dist failed (exit $LASTEXITCODE)" }
}

Write-Host 'Done.'
