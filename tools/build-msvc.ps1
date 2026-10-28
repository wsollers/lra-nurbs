<#
.SYNOPSIS
Builds and tests lra-nurbs with the Visual Studio MSVC toolchain.

.DESCRIPTION
Locates the installed Visual Studio C++ environment with vswhere, enters the
x64 developer environment, and then configures, builds, and tests with Ninja.
Using vcvars64.bat is required because invoking a cached cl.exe path directly
does not provide the MSVC standard-library or Windows SDK include paths.

.PARAMETER Configuration
Selects a Debug or Release build.

.EXAMPLE
.\tools\build-msvc.ps1 -Configuration Debug
#>
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "vswhere.exe was not found. Install Visual Studio with the C++ workload."
}

$vcvars = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'VC\Auxiliary\Build\vcvars64.bat' | Select-Object -First 1
if (-not $vcvars) {
    throw "vcvars64.bat was not found. Install the Visual Studio C++ workload."
}

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildDir = "build\msvc-$($Configuration.ToLowerInvariant())"
$command = @(
    ('call "{0}"' -f $vcvars)
    ('cd /d "{0}"' -f $repo)
    ('cmake -S . -B "{0}" -G Ninja -DCMAKE_BUILD_TYPE={1} -DENABLE_SANITIZERS=OFF -DNDDE_ENABLE_CLANG_TIDY=OFF' -f $buildDir, $Configuration)
    ('cmake --build "{0}" --parallel' -f $buildDir)
    ('ctest --test-dir "{0}" --output-on-failure --parallel 4' -f $buildDir)
) -join ' && '

cmd.exe /d /s /c $command
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
