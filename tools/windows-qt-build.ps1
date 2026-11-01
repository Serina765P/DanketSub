# Build the Qt 6 client with the workspace-local Qt SDK and MSVC toolchain.
#
# Usage (from a normal PowerShell prompt):
#   powershell -ExecutionPolicy Bypass -File tools/windows-qt-build.ps1
#   powershell -ExecutionPolicy Bypass -File tools/windows-qt-build.ps1 -Action test
#   powershell -ExecutionPolicy Bypass -File tools/windows-qt-build.ps1 -Action configure
#   powershell -ExecutionPolicy Bypass -File tools/windows-qt-build.ps1 -VcVars .toolchain\vs\VC\Auxiliary\Build\vcvars64.bat
#
# The script expects:
#   .toolchain\qt\<version>\msvc2022_64\lib\cmake\Qt6\Qt6Config.cmake
#   .toolchain\vs\VC\Auxiliary\Build\vcvars64.bat  (workspace-local Build Tools)

param(
    [ValidateSet('configure', 'build', 'test')]
    [string]$Action = 'build',
    [string]$QtRoot = '',
    [string]$VcVars = '',
    [string]$BuildDir = 'build-app'
)

$ErrorActionPreference = 'Stop'

$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root $BuildDir

function Find-QtRoot {
    $qtRoots = Get-ChildItem (Join-Path $root '.toolchain\qt') -Directory -ErrorAction SilentlyContinue |
        ForEach-Object { Get-ChildItem $_.FullName -Directory -ErrorAction SilentlyContinue } |
        Where-Object { Test-Path (Join-Path $_.FullName 'lib\cmake\Qt6\Qt6Config.cmake') } |
        Sort-Object Name -Descending
    return $qtRoots | Select-Object -First 1 -ExpandProperty FullName
}

function Import-VcVars {
    $candidates = @()
    if ($VcVars) {
        if (-not (Test-Path -LiteralPath $VcVars)) {
            throw "VcVars path not found: $VcVars"
        }
        $resolved = (Resolve-Path -LiteralPath $VcVars).Path
        if (-not $resolved.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "VcVars must point inside the workspace ($root)."
        }
        $candidates += $resolved
    }
    $candidates += (Join-Path $root '.toolchain\vs\VC\Auxiliary\Build\vcvars64.bat')

    $vcvars = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (-not $vcvars) {
        throw "Workspace-local vcvars64.bat not found. Expected .toolchain\vs\VC\Auxiliary\Build\vcvars64.bat; install the Visual Studio Build Tools into .toolchain or pass -VcVars <path>."
    }

    $envLines = & cmd.exe /c "`"$vcvars`" >nul && set"
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to initialise the MSVC environment from $vcvars"
    }
    foreach ($line in $envLines) {
        if ($line -match '^([^=]+)=(.*)$') {
            Set-Item -Path ("env:" + $matches[1]) -Value $matches[2]
        }
    }
}

if (-not $QtRoot) {
    $QtRoot = Find-QtRoot
}
if (-not $QtRoot -or -not (Test-Path (Join-Path $QtRoot 'lib\cmake\Qt6\Qt6Config.cmake'))) {
    throw "Qt 6 SDK not found. Install it into .toolchain\qt, for example:`n" +
          "  .venv\Scripts\aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 --outputdir .toolchain\qt --modules qt5compat"
}

Import-VcVars
$env:PATH = "$QtRoot\bin;$env:PATH"

Write-Host "Qt SDK:  $QtRoot"
Write-Host "Build:   $build"

Push-Location $root
try {
    if ($Action -eq 'configure' -or -not (Test-Path (Join-Path $build 'CMakeCache.txt'))) {
        & cmake -S $root -B $build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo "-DCMAKE_PREFIX_PATH=$QtRoot"
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }

    if ($Action -ne 'configure') {
        & cmake --build $build --parallel
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }

    if ($Action -ne 'configure') {
        Push-Location $build
        try {
            & ctest --output-on-failure
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        }
        finally {
            Pop-Location
        }
    }
}
finally {
    Pop-Location
}
