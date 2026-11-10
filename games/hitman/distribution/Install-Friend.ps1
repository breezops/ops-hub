param(
    [string]$GamePath = '',
    [ValidateSet('Auto', 'Install', 'Update', 'Remove')][string]$Mode = 'Install',
    [switch]$CheckOnly
)

$ErrorActionPreference = 'Stop'
$Installer = Join-Path $PSScriptRoot 'installer.ps1'
$PowerShell = Join-Path $PSHOME 'powershell.exe'

function Set-IniValue([string]$Text, [string]$Section, [string]$Key, [string]$Value) {
    $Lines = @($Text -split '\r?\n')
    $Result = New-Object System.Collections.Generic.List[string]
    $InSection = $false
    $FoundSection = $false
    $Written = $false
    foreach ($Line in $Lines) {
        if ($Line -match '^\s*\[([^]]+)\]\s*$') {
            if ($InSection -and -not $Written) { $Result.Add("$Key=$Value"); $Written = $true }
            $InSection = $Matches[1] -ieq $Section
            if ($InSection) { $FoundSection = $true }
        }
        if ($InSection -and $Line -match ('^\s*' + [regex]::Escape($Key) + '\s*=')) {
            if (-not $Written) { $Result.Add("$Key=$Value"); $Written = $true }
        } else { $Result.Add($Line) }
    }
    if (-not $FoundSection) { $Result.Add("[$Section]") }
    if (-not $Written) { $Result.Add("$Key=$Value") }
    return ($Result -join "`r`n")
}

try {
    Write-Host "HITMAN Co-op - $Mode" -ForegroundColor Cyan
    Write-Host 'Co-op mod by Jordan Rodrigues; ZHMModSDK by OrfeasZ and contributors.'
    Write-Host 'Test build: 0.13.94-four-player.preview.2, for HITMAN 3.290.1 on Windows.'
    Write-Host ''

    # Verify the current-game payload before any change to the game folder.
    foreach ($Entry in $(if ($Mode -ne 'Remove') { Get-Content -LiteralPath (Join-Path $PSScriptRoot 'payload-hashes.json') -Raw | ConvertFrom-Json })) {
        $PayloadFile = Join-Path $PSScriptRoot $Entry.file
        if (-not (Test-Path -LiteralPath $PayloadFile -PathType Leaf)) {
            throw "Missing package file: $($Entry.file). Extract the entire ZIP first."
        }
        if ((Get-FileHash -LiteralPath $PayloadFile -Algorithm SHA256).Hash -ne $Entry.sha256) {
            throw "Package file changed or is damaged: $($Entry.file). Extract a fresh copy."
        }
    }

    if (-not $GamePath) {
        $Candidates = @()
        try {
            $Candidates = @(& $PowerShell -NoProfile -ExecutionPolicy Bypass -File $Installer -ListGames 2>$null)
            if ($LASTEXITCODE -ne 0) { $Candidates = @() }
        } catch { $Candidates = @() }
        if (-not $Candidates.Count) {
            Write-Host 'Automatic detection did not find a game. Select your HITMAN 3 folder.'
        }
        $Candidates = @($Candidates | Where-Object { $_ -and (Test-Path -LiteralPath (Join-Path $_ 'Retail\HITMAN3.exe')) })
        if ($Candidates.Count -eq 1) {
            $GamePath = $Candidates[0]
        } else {
            Add-Type -AssemblyName System.Windows.Forms
            $Picker = New-Object System.Windows.Forms.FolderBrowserDialog
            $Picker.Description = 'Select HITMAN 3 (the folder containing Retail and Runtime).'
            try {
                if ($Picker.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) {
                    throw 'Setup cancelled. No game files changed.'
                }
                $GamePath = $Picker.SelectedPath
            } finally { $Picker.Dispose() }
        }
    }
    if ((Split-Path $GamePath -Leaf) -ieq 'Retail') { $GamePath = Split-Path $GamePath -Parent }
    $GamePath = (Resolve-Path -LiteralPath $GamePath).Path
    if ($Mode -eq 'Auto') {
        $Mode = if (Test-Path -LiteralPath (Join-Path $GamePath 'Retail\HitmanCoop-install.json')) { 'Update' } else { 'Install' }
        Write-Host $(if ($Mode -eq 'Update') { 'Existing co-op found. Updating it and keeping your preferences.' } else { 'Setting up co-op for the first time.' })
    }
    $Exe = Join-Path $GamePath 'Retail\HITMAN3.exe'
    if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) {
        throw 'That folder does not contain Retail\HITMAN3.exe.'
    }
    $Version = (Get-Item -LiteralPath $Exe).VersionInfo
    $GameVersion = '{0}.{1}.{2}' -f $Version.FileMajorPart,$Version.FileMinorPart,$Version.FileBuildPart
    if ($Mode -ne 'Remove' -and $GameVersion -ne '3.290.1') {
        throw "Found HITMAN $GameVersion. This test package requires 3.290.1 to match the host. No files changed."
    }
    Write-Host "Found HITMAN $GameVersion in $GamePath"
    if ($CheckOnly) {
        Write-Host "CHECK PASSED: $Mode preflight. No files changed." -ForegroundColor Green
        exit 0
    }
    if (Get-Process -Name HITMAN3 -ErrorAction SilentlyContinue) {
        throw 'HITMAN is running. Close the game, then click Install / repair again.'
    }

    $Retail = Join-Path $GamePath 'Retail'
    # The package launcher must stay open to run setup and display its result.
    # Only launchers running from the destination game folder lock files that
    # this installer will replace/remove. Other package/release copies are safe.
    $LockedLaunchers = @('HITMAN-Coop-Launcher.exe','HITMAN-Coop-Party-Preview.exe') | ForEach-Object {
        [IO.Path]::GetFullPath((Join-Path $Retail $_))
    }
    foreach ($LauncherProcess in @(Get-Process -Name HITMAN-Coop-Launcher,HITMAN-Coop-Party-Preview -ErrorAction SilentlyContinue)) {
        $LauncherPath = $LauncherProcess.Path
        if (-not $LauncherPath -or $LockedLaunchers -contains [IO.Path]::GetFullPath($LauncherPath)) {
            throw 'A co-op launcher is running from the game folder. Close it and open the launcher from your extracted download folder instead.'
        }
    }
    $Settings = Join-Path $Retail 'mods\HitmanCoop.ini'
    $ManifestPath = Join-Path $Retail 'HitmanCoop-install.json'
    if ($Mode -eq 'Remove') {
        if (-not (Test-Path -LiteralPath $ManifestPath)) {
            throw 'No tracked co-op installation was found. Nothing was removed.'
        }
        # Validate every computed removal/backup target before invoking the upstream remover.
        $Manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
        foreach ($Relative in @($Manifest.added) + @($Manifest.backups)) {
            if (-not $Relative) { continue }
            $ResolvedTarget = [IO.Path]::GetFullPath((Join-Path $Retail $Relative))
            if (-not $ResolvedTarget.StartsWith($Retail.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
                throw 'The installation record contains an invalid removal path. Nothing was removed.'
            }
        }
        $BackupTarget = [IO.Path]::GetFullPath((Join-Path $Retail 'HitmanCoop-backup'))
        if ($BackupTarget -ne ($Retail.TrimEnd('\') + '\HitmanCoop-backup')) { throw 'Invalid backup location.' }
        & $PowerShell -NoProfile -ExecutionPolicy Bypass -File $Installer -GamePath $GamePath -Uninstall -Quiet -NoFirewall
        if ($LASTEXITCODE -ne 0) { throw 'Removal did not finish. Read the error above.' }
        if (Test-Path -LiteralPath (Join-Path $Retail 'mods\HitmanCoop.dll')) { throw 'The co-op DLL is still present.' }
        Write-Host 'CO-OP REMOVED. Prior loader files were restored where backups existed.' -ForegroundColor Green
        Write-Host 'Saves and game archives were not changed. Other enabled SDK mods may keep the loader installed.'
        exit 0
    }
    if ($Mode -eq 'Update' -and -not (Test-Path -LiteralPath $ManifestPath)) {
        throw 'No tracked co-op installation found. Use INSTALL.cmd for the first installation.'
    }
    $SavedSettings = if (Test-Path -LiteralPath $Settings) { [IO.File]::ReadAllText($Settings) } else { $null }

    # The original installer tracks the payload and backs up any files it replaces,
    # including the matching test settings added to this distribution.
    & $PowerShell -NoProfile -ExecutionPolicy Bypass -File $Installer -GamePath $GamePath -Quiet -NoFirewall
    if ($LASTEXITCODE -ne 0) {
        throw 'The installer did not finish. Read its error above. If it reported access denied, right-click INSTALL.cmd and choose Run as administrator.'
    }
    foreach ($Entry in (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'payload-hashes.json') -Raw | ConvertFrom-Json)) {
        $Relative = $Entry.file.Substring('files\'.Length)
        $Installed = Join-Path (Join-Path $GamePath 'Retail') $Relative
        if ((Get-FileHash -LiteralPath $Installed -Algorithm SHA256).Hash -ne $Entry.sha256) {
            throw "Installed file does not match the package: $Relative."
        }
    }
    # Preserve names, hotkeys and preferences on an upgrade while pinning the test version.
    if ($null -ne $SavedSettings) {
        $SavedSettings = Set-IniValue $SavedSettings 'general' 'auto_update' 'false'
        $SavedSettings = Set-IniValue $SavedSettings 'general' 'any_game_version' 'false'
        $SavedSettings = Set-IniValue $SavedSettings 'general' 'sounds' 'false'
        $SavedSettings = Set-IniValue $SavedSettings 'general' 'show_notifications' 'false'
        [IO.File]::WriteAllText($Settings, $SavedSettings, [Text.Encoding]::UTF8)
    }
    Write-Host ''
    Write-Host "$($Mode.ToUpper()) COMPLETE AND VERIFIED." -ForegroundColor Green
    $SdkIni = Join-Path $Retail 'mods.ini'
    $SdkText = if (Test-Path -LiteralPath $SdkIni) { [IO.File]::ReadAllText($SdkIni) } else { '' }
    $SdkText = Set-IniValue $SdkText 'sdk' 'crash_reporting' 'false'
    [IO.File]::WriteAllText($SdkIni, $SdkText, [Text.UTF8Encoding]::new($false))
    $LauncherHash = (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'HITMAN-Coop-Party-Preview.exe')).Hash.ToLowerInvariant()
    @{ mode='preview'; launcher='HITMAN-Coop-Party-Preview.exe'; sha256=$LauncherHash } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'launcher-mode.json') -Encoding UTF8
    Write-Host 'Open START-HERE.cmd or HITMAN-Coop-Party-Preview.exe to launch OPS HUB HITMAN co-op.'
    Write-Host 'Load Freeform Training manually on every PC, click Ready on each, then the host clicks Start preview.'
    Write-Host 'You do not need a port forward to join. See START-HERE.txt.'
    exit 0
} catch {
    Write-Host ''
    Write-Host "SETUP STOPPED: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
