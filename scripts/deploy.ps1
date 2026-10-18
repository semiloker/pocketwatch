<#
.SYNOPSIS
    Copies the built PRX files to a PSP connected over USB.

.DESCRIPTION
    Installs plugin\pocketwatch.prx (and vsh\pocketwatch_xmb.prx when it is built) to
    <PSP>\seplugins\pocketwatch\, and home\EBOOT.PBP (Pocketwatch Home) to
    <PSP>\PSP\GAME\PocketwatchHome\.
    With -Enable it also registers the game plugin in seplugins\PLUGINS.TXT (ARK-4 format);
    with -EnableXmb the XMB plugin (the Pocketwatch entry under Settings). If the XMB ever fails
    to start because of a plugin, hold START while the PSP boots: ARK-4 then loads no plugins.

.EXAMPLE
    .\scripts\deploy.ps1
    .\scripts\deploy.ps1 -Drive E -Enable -EnableXmb
#>
param(
    # Drive letter of the PSP. Auto-detected when there is exactly one PSP connected.
    [string]$Drive,
    # Add the game plugin to seplugins\PLUGINS.TXT if it is not listed yet.
    [switch]$Enable,
    # Add the XMB plugin to seplugins\PLUGINS.TXT if it is not listed yet.
    [switch]$EnableXmb,
    # Storage device as seen by the PSP: ms0 (Memory Stick) or ef0 (PSP Go internal).
    [ValidateSet('ms0', 'ef0')]
    [string]$Device = 'ms0'
)

$root = Split-Path -Parent $PSScriptRoot
$prx = Join-Path $root 'plugin\pocketwatch.prx'
$xmbPrx = Join-Path $root 'vsh\pocketwatch_xmb.prx'
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
if (Test-Path $xmbPrx) {
    Copy-Item -Path $xmbPrx -Destination $dest -Force
    Write-Host "Copied pocketwatch_xmb.prx -> $dest"
}

# Pocketwatch Home: a homebrew application, started from Game -> Memory Stick.
$homeEboot = Join-Path $root 'home\EBOOT.PBP'
if (Test-Path $homeEboot) {
    $homeDest = Join-Path $pspRoot 'PSP\GAME\PocketwatchHome'
    New-Item -ItemType Directory -Force -Path $homeDest | Out-Null
    Copy-Item -Path $homeEboot -Destination $homeDest -Force
    Write-Host "Copied EBOOT.PBP -> $homeDest"
    # The start screen (ADR-026): ARK-4 starts a launcher named relative to its own folder, so
    # Home is also there as PWHOME.PBP. Turned on in XMB -> Settings -> Pocketwatch.
    $arkDir = Join-Path $pspRoot 'PSP\SAVEDATA\ARK_01234'
    if (Test-Path $arkDir) {
        Copy-Item -Path $homeEboot -Destination (Join-Path $arkDir 'PWHOME.PBP') -Force
        Write-Host "Copied EBOOT.PBP -> $arkDir\PWHOME.PBP"
    }
}

# Translations: the plugins read lang\<code>.txt for the system language (English is built in).
$langSrc = Join-Path $root 'plugin\lang'
if (Test-Path $langSrc) {
    $langDest = Join-Path $dest 'lang'
    New-Item -ItemType Directory -Force -Path $langDest | Out-Null
    Copy-Item -Path (Join-Path $langSrc '*.txt') -Destination $langDest -Force
    Write-Host "Copied translations -> $langDest"
}

$pluginsTxt = Join-Path $pspRoot 'seplugins\PLUGINS.TXT'

# Make sure PLUGINS.TXT has $line for the PRX that $pattern finds; $register: change the file.
function Register-Plugin([string]$line, [string]$pattern, [bool]$register, [string]$switchName) {
    $text = ''
    if (Test-Path $pluginsTxt) { $text = [IO.File]::ReadAllText($pluginsTxt, [Text.Encoding]::ASCII) }
    $existing = [regex]::Match($text, $pattern)

    if ($existing.Success -and $existing.Value -eq $line) {
        Write-Host "Already listed in ${pluginsTxt}: '$line'"
    }
    elseif ($existing.Success) {
        if ($register) {
            $text = $text.Remove($existing.Index, $existing.Length).Insert($existing.Index, $line)
            [IO.File]::WriteAllText($pluginsTxt, $text, [Text.Encoding]::ASCII)
            Write-Host "Updated in ${pluginsTxt}: '$($existing.Value)' -> '$line'"
        }
        else {
            Write-Host "Listed as '$($existing.Value)'; expected '$line' (re-run with -$switchName to fix)"
        }
    }
    elseif ($register) {
        # Keep the file's LF line endings and make sure we start on a fresh line.
        $prefix = ''
        if (Test-Path $pluginsTxt) {
            $bytes = [IO.File]::ReadAllBytes($pluginsTxt)
            if ($bytes.Length -gt 0 -and $bytes[-1] -ne 10) { $prefix = "`n" }
        }
        [IO.File]::AppendAllText($pluginsTxt, "$prefix# Pocketwatch`n$line`n", [Text.Encoding]::ASCII)
        Write-Host "Registered in ${pluginsTxt}: '$line'"
    }
    else {
        Write-Host "Not registered yet. Add this line to $pluginsTxt (or re-run with -${switchName}):"
        Write-Host "  $line"
    }
}

# Runlevel UMD = retail games (UMD and ISO) only. GAME would also load it into homebrew.
Register-Plugin "UMD, ${Device}:/seplugins/pocketwatch/pocketwatch.prx, on" `
    '(?m)^[^#\r\n]*seplugins/pocketwatch/pocketwatch\.prx[^\r\n]*$' $Enable.IsPresent 'Enable'
if (Test-Path $xmbPrx) {
    Register-Plugin "VSH, ${Device}:/seplugins/pocketwatch/pocketwatch_xmb.prx, on" `
        '(?m)^[^#\r\n]*seplugins/pocketwatch/pocketwatch_xmb\.prx[^\r\n]*$' $EnableXmb.IsPresent 'EnableXmb'
}
