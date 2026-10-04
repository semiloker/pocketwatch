<#
.SYNOPSIS
    Copies the built PRX to a PSP connected over USB.

.DESCRIPTION
    Installs plugin\pocketwatch.prx to <PSP>\seplugins\pocketwatch\.
    With -Enable it also registers the plugin in seplugins\PLUGINS.TXT (ARK-4 format).

.EXAMPLE
    .\scripts\deploy.ps1
    .\scripts\deploy.ps1 -Drive E -Enable
#>
param(
    # Drive letter of the PSP. Auto-detected when there is exactly one PSP connected.
    [string]$Drive,
    # Add the plugin to seplugins\PLUGINS.TXT if it is not listed yet.
    [switch]$Enable,
    # Storage device as seen by the PSP: ms0 (Memory Stick) or ef0 (PSP Go internal).
    [ValidateSet('ms0', 'ef0')]
    [string]$Device = 'ms0'
)

$root = Split-Path -Parent $PSScriptRoot
$prx = Join-Path $root 'plugin\pocketwatch.prx'
if (-not (Test-Path $prx)) {
    Write-Error "Not built yet: $prx. Run .\scripts\build.ps1 first."
    exit 1
}

if (-not $Drive) {
    $psps = @(Get-Volume | Where-Object {
            $_.DriveType -eq 'Removable' -and $_.DriveLetter -and (Test-Path "$($_.DriveLetter):\PSP")
        })
    if ($psps.Count -ne 1) {
        Write-Error "Found $($psps.Count) PSP drive(s); pass -Drive explicitly."
        exit 1
    }
    $Drive = [string]$psps[0].DriveLetter
}
$pspRoot = $Drive.TrimEnd(':', '\') + ':\'

$dest = Join-Path $pspRoot 'seplugins\pocketwatch'
New-Item -ItemType Directory -Force -Path $dest | Out-Null
Copy-Item -Path $prx -Destination $dest -Force
Write-Host "Copied pocketwatch.prx -> $dest"

$pluginsTxt = Join-Path $pspRoot 'seplugins\PLUGINS.TXT'
# Runlevel UMD = retail games (UMD and ISO) only. GAME would also load it into homebrew.
$line = "UMD, ${Device}:/seplugins/pocketwatch/pocketwatch.prx, on"
$text = ''
if (Test-Path $pluginsTxt) { $text = [IO.File]::ReadAllText($pluginsTxt, [Text.Encoding]::ASCII) }
$pattern = '(?m)^[^#\r\n]*seplugins/pocketwatch/pocketwatch\.prx[^\r\n]*$'
$existing = [regex]::Match($text, $pattern)

if ($existing.Success -and $existing.Value -eq $line) {
    Write-Host "Already listed in $pluginsTxt"
}
elseif ($existing.Success) {
    if ($Enable) {
        $text = $text.Remove($existing.Index, $existing.Length).Insert($existing.Index, $line)
        [IO.File]::WriteAllText($pluginsTxt, $text, [Text.Encoding]::ASCII)
        Write-Host "Updated in ${pluginsTxt}: '$($existing.Value)' -> '$line'"
    }
    else {
        Write-Host "Listed as '$($existing.Value)'; expected '$line' (re-run with -Enable to fix)"
    }
}
elseif ($Enable) {
    # Keep the file's LF line endings and make sure we start on a fresh line.
    $prefix = ''
    if (Test-Path $pluginsTxt) {
        $bytes = [IO.File]::ReadAllBytes($pluginsTxt)
        if ($bytes.Length -gt 0 -and $bytes[-1] -ne 10) { $prefix = "`n" }
    }
    [IO.File]::AppendAllText($pluginsTxt, "$prefix# Pocketwatch`n$line`n", [Text.Encoding]::ASCII)
    Write-Host "Registered in $pluginsTxt"
}
else {
    Write-Host "Not registered yet. Add this line to $pluginsTxt (or re-run with -Enable):"
    Write-Host "  $line"
}
