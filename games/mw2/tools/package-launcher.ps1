[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$versionInfo = Get-Content -LiteralPath (Join-Path $root 'version.json') -Raw | ConvertFrom-Json
$version = $versionInfo.version
$output = Join-Path $root ('_releases\' + $version)
New-Item -ItemType Directory -Force -Path $output | Out-Null
$stage = Join-Path $output 'MW2-SpecOps4-Launcher'
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'launcher\bin\MW2-SpecOps4-Launcher.exe') -Destination $stage
Copy-Item -LiteralPath (Join-Path $root 'launcher\bin\SpecOps4-Codec-Check.exe') -Destination $stage
Copy-Item -LiteralPath (Join-Path $root 'launcher\assets\launcher.ico') -Destination $stage
Copy-Item -LiteralPath (Join-Path $root 'launcher\update-launcher.ps1') -Destination $stage
Copy-Item -LiteralPath (Join-Path $root 'launcher\vendor\miniz\LICENSE') -Destination (Join-Path $stage 'MINIZ-LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $root 'distribution\START-HERE.txt'),(Join-Path $root 'distribution\CHANGELOG.md') -Destination $stage
Copy-Item -LiteralPath (Join-Path $root 'README.md') -Destination (Join-Path $stage 'SOURCE-AND-STATUS.md')
Copy-Item -LiteralPath (Join-Path $root 'LOBBY.md') -Destination $stage
Copy-Item -LiteralPath (Join-Path $root 'launcher\assets\PROVENANCE.md') -Destination (Join-Path $stage 'ARTWORK-PROVENANCE.md')
$name = 'MW2-SpecOps4-Launcher-' + $version + '-win-x64.zip'
$archive = Join-Path $output $name
Compress-Archive -LiteralPath $stage -DestinationPath $archive -Force
$hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
$hash + '  ' + $name | Set-Content -LiteralPath (Join-Path $output 'SHA256SUMS.txt') -Encoding ascii
$manifest = [ordered]@{
    game = 'mw2-2009'; version = $version; channel = 'development-launcher'
    display_version = $versionInfo.display; lobby_protocol = $versionInfo.lobby_protocol
    mod_status = 'in-development'; four_player_playable = $false
    supported_steam_build = '25374375'; platform = 'windows-x64'
    download_url = 'https://ops-hub.breezops.chatgpt.site/downloads/mw2/' + $name
    sha256 = $hash; size_bytes = (Get-Item -LiteralPath $archive).Length
    source_url = 'https://github.com/breezops/ops-hub/tree/feature/mw2-specops4-launcher/games/mw2'
    notes = 'Live four-slot launcher lobby, shared mission selection and diagnostics. Four-player gameplay is unavailable.'
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'release.json') -Encoding utf8
Write-Output ('Package: ' + $archive)
Write-Output ('SHA-256: ' + $hash)
