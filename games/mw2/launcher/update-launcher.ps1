[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$LauncherVersion,
    [Parameter(Mandatory)][long]$LauncherWindow
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Net.Http
function Show-Update([string]$Text, [string]$Title = 'MW2 launcher updates', [System.Windows.Forms.MessageBoxButtons]$Buttons = [System.Windows.Forms.MessageBoxButtons]::OK) {
    [System.Windows.Forms.MessageBox]::Show($Text, $Title, $Buttons, [System.Windows.Forms.MessageBoxIcon]::Information)
}
$client = $null
$stage = $null
try {
    if ($LauncherVersion -notmatch '^([0-9]+)\.([0-9]+)\.([0-9]+)-prealpha\.([0-9]+)$') { throw 'This launcher version cannot use the updater.' }
    $current = [version]::new([int]$Matches[1],[int]$Matches[2],[int]$Matches[3],[int]$Matches[4])
    $root = $PSScriptRoot
    $handler = [Net.Http.HttpClientHandler]::new()
    $handler.AllowAutoRedirect = $false
    $client = [Net.Http.HttpClient]::new($handler)
    $client.Timeout = [TimeSpan]::FromSeconds(45)
    $manifestUrl = 'https://ops-hub.breezops.chatgpt.site/mw2/release.json'
    $response = $client.GetAsync($manifestUrl).GetAwaiter().GetResult()
    if (!$response.IsSuccessStatusCode) { throw 'Release information is unavailable.' }
    $body = $response.Content.ReadAsByteArrayAsync().GetAwaiter().GetResult()
    if ($body.Length -gt 16384) { throw 'Release information is too large.' }
    $release = [Text.Encoding]::UTF8.GetString($body) | ConvertFrom-Json
    if ($release.version -notmatch '^([0-9]+)\.([0-9]+)\.([0-9]+)-prealpha\.([0-9]+)$') { throw 'Invalid release version.' }
    $latest = [version]::new([int]$Matches[1],[int]$Matches[2],[int]$Matches[3],[int]$Matches[4])
    if ($release.game -ne 'mw2-2009' -or $release.channel -ne 'development-launcher' -or $release.platform -ne 'windows-x64') { throw 'Release is for another launcher.' }
    if ($latest -le $current) { [void](Show-Update "You're up to date ($LauncherVersion)."); return }
    if ($release.sha256 -notmatch '^[0-9a-f]{64}$' -or $release.size_bytes -lt 1 -or $release.size_bytes -gt 100MB) { throw 'Invalid release integrity metadata.' }
    $expected = 'https://ops-hub.breezops.chatgpt.site/downloads/mw2/MW2-SpecOps4-Launcher-' + $release.version + '-win-x64.zip'
    if ($release.download_url -cne $expected) { throw 'Unexpected release download address.' }
    if ((Show-Update "Launcher $($release.version) is available. Download and install it now? The launcher will restart when ready." 'MW2 launcher update' YesNo) -ne [System.Windows.Forms.DialogResult]::Yes) { return }
    $stage = Join-Path $root ('.update-' + [Guid]::NewGuid().ToString('N'))
    [void][IO.Directory]::CreateDirectory($stage)
    $archive = Join-Path $stage 'release.zip'
    $download = $client.GetAsync($expected,[Net.Http.HttpCompletionOption]::ResponseHeadersRead).GetAwaiter().GetResult()
    if (!$download.IsSuccessStatusCode) { throw 'Update download failed.' }
    $source = $download.Content.ReadAsStreamAsync().GetAwaiter().GetResult()
    $output = [IO.File]::Create($archive)
    try {
        $buffer = [byte[]]::new(131072)
        $size = 0L
        while (($read = $source.Read($buffer,0,$buffer.Length)) -gt 0) {
            $size += $read
            if ($size -gt 100MB) { throw 'Update download exceeds size limit.' }
            $output.Write($buffer,0,$read)
        }
    } finally { $output.Dispose(); $source.Dispose() }
    if ($size -ne [long]$release.size_bytes -or (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $release.sha256) { throw 'Update checksum mismatch.' }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead($archive)
    try {
        $prefix = 'MW2-SpecOps4-Launcher/'
        $allowed = @('MW2-SpecOps4-Launcher.exe','update-launcher.ps1','launcher.ico','START-HERE.txt','CHANGELOG.md','SOURCE-AND-STATUS.md','LOBBY.md','ARTWORK-PROVENANCE.md','MINIZ-LICENSE.txt','SpecOps4-Codec-Check.exe')
        $seen = @{}
        $expanded = 0L
        if ($zip.Entries.Count -gt 32) { throw 'Unexpected update contents.' }
        foreach ($entry in $zip.Entries) {
            $path = $entry.FullName.Replace('\','/')
            if ($path -eq $prefix) { continue }
            if (!$path.StartsWith($prefix,[StringComparison]::Ordinal) -or $path.Substring($prefix.Length).Contains('/') -or $allowed -cnotcontains $path.Substring($prefix.Length)) { throw 'Unexpected update path.' }
            $name = $path.Substring($prefix.Length)
            if ($seen.ContainsKey($name) -or $entry.Length -gt 30MB) { throw 'Duplicate or oversized update file.' }
            $seen[$name] = $true
            $target = [IO.File]::Create((Join-Path $stage $name))
            $input = $entry.Open()
            try { $input.CopyTo($target) } finally { $input.Dispose(); $target.Dispose() }
        }
        if (!$seen.ContainsKey('MW2-SpecOps4-Launcher.exe') -or !$seen.ContainsKey('update-launcher.ps1')) { throw 'Update package is incomplete.' }
    } finally { $zip.Dispose() }
    $currentExe = Join-Path $root 'MW2-SpecOps4-Launcher.exe'
    $currentHash = (Get-FileHash -LiteralPath $currentExe -Algorithm SHA256).Hash
    Add-Type -TypeDefinition 'using System; using System.Runtime.InteropServices; public static class LauncherClose { [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam); }'
    [void][LauncherClose]::PostMessage([IntPtr]$LauncherWindow,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-Process -Name MW2-SpecOps4-Launcher -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $currentExe }) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 250 }
    if (Get-Process -Name MW2-SpecOps4-Launcher -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $currentExe }) { throw 'Close the launcher and check for updates again.' }
    $backup = Join-Path $root 'MW2-SpecOps4-Launcher.previous.exe'
    Copy-Item -LiteralPath $currentExe -Destination $backup -Force
    try {
        foreach ($name in $seen.Keys) { Copy-Item -LiteralPath (Join-Path $stage $name) -Destination (Join-Path $root $name) -Force }
    } catch {
        Copy-Item -LiteralPath $backup -Destination $currentExe -Force
        throw
    }
    Start-Process -FilePath $currentExe -WorkingDirectory $root
} catch {
    [void](Show-Update ('Update failed. ' + $_.Exception.Message))
} finally {
    if ($client) { $client.Dispose() }
    if ($stage -and (Test-Path -LiteralPath $stage)) { Remove-Item -LiteralPath $stage -Recurse -Force }
}
