<#
.SYNOPSIS
    Builds the Pocketwatch PRX inside WSL (where the PSPDEV toolchain lives).

.EXAMPLE
    .\scripts\build.ps1
    .\scripts\build.ps1 -Target clean
#>
param(
    [string]$Target = 'all',
    [string]$Distro = 'Ubuntu'
)

$root = Split-Path -Parent $PSScriptRoot

# Resolve the version on the Windows side: git inside WSL rejects repos owned by Windows users.
$version = & git -C $root describe --tags --always --dirty 2>$null
if ($LASTEXITCODE -ne 0 -or -not $version) { $version = 'dev' }

wsl -d $Distro --cd $root -- bash scripts/build.sh $Target $version
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed (exit code $LASTEXITCODE)"
    exit $LASTEXITCODE
}

if ($Target -eq 'all') {
    $prx = Join-Path $root 'plugin\pocketwatch.prx'
    Write-Host "Built $prx ($version)"
}
