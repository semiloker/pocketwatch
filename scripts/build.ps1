<#
.SYNOPSIS
    Builds the Pocketwatch PRX inside WSL (where the PSPDEV toolchain lives).

.EXAMPLE
    .\scripts\build.ps1
    .\scripts\build.ps1 -Target clean
    .\scripts\build.ps1 -Release          (release build: no debug probe, combos or test items)
    .\scripts\build.ps1 -Release -Target check
#>
param(
    [string]$Target = 'all',
    [string]$Distro = 'Ubuntu',
    [switch]$Release
)

$root = Split-Path -Parent $PSScriptRoot

# Resolve the version on the Windows side: git inside WSL rejects repos owned by Windows users.
$version = & git -C $root describe --tags --always --dirty 2>$null
if ($LASTEXITCODE -ne 0 -or -not $version) { $version = 'dev' }

$debug = '1'
if ($Release) { $debug = '0' }
wsl -d $Distro --cd $root -- bash scripts/build.sh $Target $version $debug
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed (exit code $LASTEXITCODE)"
    exit $LASTEXITCODE
}

if ($Target -eq 'all') {
    $prx = Join-Path $root 'plugin\pocketwatch.prx'
    $profileName = 'debug'
    if ($Release) { $profileName = 'release' }
    Write-Host "Built $prx ($version, $profileName)"
}
