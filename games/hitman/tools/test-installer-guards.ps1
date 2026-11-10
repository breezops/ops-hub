param([string]$Output=(Join-Path $PSScriptRoot '..\outputs'))
$ErrorActionPreference='Stop'
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$Output=[IO.Path]::GetFullPath($Output)
$Fixture=[IO.Path]::GetFullPath((Join-Path $Output ('installer-guards-'+[Guid]::NewGuid().ToString('N'))))
if (-not $Fixture.StartsWith($Output.TrimEnd('\')+'\installer-guards-',[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid fixture path' }
$Package=Join-Path $Fixture 'Package'
$Game=Join-Path $Fixture 'Game'
$Retail=Join-Path $Game 'Retail'
$Observer=Join-Path $Fixture 'Observer'
@($Package,(Join-Path $Package 'files\mods'),$Retail,$Observer) | ForEach-Object { New-Item -ItemType Directory -Force -Path $_ | Out-Null }
# Synthetic executables supply version metadata and named processes only.
# No game executable, SDK runtime or game install is read or executed.
$StubSource=Join-Path $Fixture 'Stub.cs'
@'
using System.Reflection;
[assembly: AssemblyFileVersion("3.290.1.0")]
public static class Stub { public static void Main() { System.Threading.Thread.Sleep(120000); } }
'@ | Set-Content -LiteralPath $StubSource
$Compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$Stub=Join-Path $Fixture 'Stub.exe'
& $Compiler /nologo /target:winexe /platform:x64 ('/out:'+$Stub) $StubSource
if ($LASTEXITCODE) { throw 'Process fixture compilation failed' }
Copy-Item -LiteralPath $Stub -Destination (Join-Path $Retail 'HITMAN3.exe')
Copy-Item -LiteralPath $Stub -Destination (Join-Path $Package 'HITMAN-Coop-Party-Preview.exe')
Copy-Item -LiteralPath $Stub -Destination (Join-Path $Package 'files\HITMAN-Coop-Launcher.exe')
Set-Content -LiteralPath (Join-Path $Package 'files\mods\HitmanCoop.dll') -Value 'synthetic mod fixture, not a native DLL'
Set-Content -LiteralPath (Join-Path $Package 'files\mods\HitmanCoop.ini') -Value "[general]`r`nplayer_name=GuardFixture"
Copy-Item -LiteralPath (Join-Path $Root 'distribution\Install-Friend.ps1') -Destination $Package
Copy-Item -LiteralPath (Join-Path $Root 'distribution\installer.ps1') -Destination $Package
$Hashes=@(Get-ChildItem -LiteralPath (Join-Path $Package 'files') -Recurse -File | ForEach-Object {
    @{file=$_.FullName.Substring($Package.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
})
$Hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Package 'payload-hashes.json')
$PowerShell=Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe'
$Results=Join-Path $Fixture 'setup-result.txt'
function Setup([string]$Mode) {
    & $PowerShell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Package 'Install-Friend.ps1') -GamePath $Game -Mode $Mode *> $Results
    return $LASTEXITCODE
}
$RemoteLauncher=$null;$GameProcess=$null;$InstalledLauncher=$null
try {
    $ExternalExe=Join-Path $Observer 'HITMAN-Coop-Party-Preview.exe'
    Copy-Item -LiteralPath $Stub -Destination $ExternalExe
    $RemoteLauncher=Start-Process -FilePath $ExternalExe -WindowStyle Hidden -PassThru
    Start-Sleep -Milliseconds 150
    if ((Setup 'Auto') -ne 0) { Get-Content $Results | Write-Host; throw 'Package launcher incorrectly blocks installation' }
    Write-Output 'PASS install succeeds while the package launcher stays open'
    if ((Setup 'Auto') -ne 0) { throw 'Package launcher incorrectly blocks repeat repair' }
    Write-Output 'PASS repeat repair succeeds with package launcher open'
    $Mod=Join-Path $Retail 'mods\HitmanCoop.dll'
    $Before=(Get-FileHash -LiteralPath $Mod).Hash
    $GameExe=Join-Path $Observer 'HITMAN3.exe'
    Copy-Item -LiteralPath $Stub -Destination $GameExe
    $GameProcess=Start-Process -FilePath $GameExe -WindowStyle Hidden -PassThru
    Start-Sleep -Milliseconds 150
    if ((Setup 'Auto') -eq 0 -or -not ([IO.File]::ReadAllText($Results).Contains('HITMAN is running'))) { throw 'Running-game guard failed' }
    if ((Get-FileHash -LiteralPath $Mod).Hash -ne $Before) { throw 'Blocked setup changed the mod' }
    Write-Output 'PASS running HITMAN blocks setup before file changes'
    $GameProcess.Kill();$GameProcess.WaitForExit();$GameProcess=$null
    # The actual destination launcher is still protected against replacement.
    $InstalledExe=Join-Path $Retail 'HITMAN-Coop-Launcher.exe'
    $InstalledLauncher=Start-Process -FilePath $InstalledExe -WindowStyle Hidden -PassThru
    Start-Sleep -Milliseconds 150
    if ((Setup 'Auto') -eq 0 -or -not ([IO.File]::ReadAllText($Results).Contains('running from the game folder'))) { throw 'Destination launcher guard failed' }
    Write-Output 'PASS destination game-folder launcher blocks replacement'
    $InstalledLauncher.Kill();$InstalledLauncher.WaitForExit();$InstalledLauncher=$null
    if ((Setup 'Remove') -ne 0 -or (Test-Path -LiteralPath $Mod)) { throw 'Package launcher incorrectly blocks removal' }
    if (-not (Test-Path -LiteralPath (Join-Path $Retail 'HITMAN3.exe'))) { throw 'Removal deleted unrelated executable' }
    Write-Output 'PASS removal succeeds with package launcher open and preserves unrelated files'
} finally {
    foreach ($OwnedProcess in @($RemoteLauncher,$GameProcess,$InstalledLauncher)) {
        if ($null -ne $OwnedProcess) { if (-not $OwnedProcess.HasExited) { $OwnedProcess.Kill();$OwnedProcess.WaitForExit() };$OwnedProcess.Dispose() }
    }
}
