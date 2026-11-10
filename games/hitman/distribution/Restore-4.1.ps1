param([string]$GamePath='', [switch]$CheckOnly)
$ErrorActionPreference='Stop'
try {
    $Archive=Join-Path $PSScriptRoot 'rollback\HITMAN-Coop-Foundation-4.1.zip'
    $Expected='988e9c38819e01a98297fffff34c7df7026aaaa132e2946659e926accf40c24f'
    if ((Get-FileHash -LiteralPath $Archive -Algorithm SHA256).Hash -ne $Expected) { throw 'The Foundation 4.1 backup is damaged. Extract a fresh preview package.' }
    if ($CheckOnly) { Write-Host 'PASS: verified Foundation 4.1 restore archive.'; exit 0 }
    if (Get-Process -Name HITMAN3,HITMAN-Coop-Launcher,HITMAN-Coop-Party-Preview -ErrorAction SilentlyContinue) { throw 'Close HITMAN and all co-op launcher windows, then restore again.' }
    $CacheRelative='rollback-cache\'+[Guid]::NewGuid().ToString('N').Substring(0,8)
    $Cache=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot $CacheRelative))
    if (-not $Cache.StartsWith($PSScriptRoot.TrimEnd('\')+'\rollback-cache\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid restore destination.' }
    New-Item -ItemType Directory -Path $Cache | Out-Null
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::ExtractToDirectory($Archive,$Cache)
    $Foundation=Join-Path $Cache 'HITMAN-Coop-Foundation-4.1'
    $Arguments=@('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $Foundation 'Install-Friend.ps1'),'-Mode','Auto')
    if ($GamePath) { $Arguments+=@('-GamePath',$GamePath) }
    & (Join-Path $PSHOME 'powershell.exe') @Arguments
    if ($LASTEXITCODE -ne 0) { throw 'Foundation 4.1 restore did not finish. Read the setup message above.' }
    $Relative=Join-Path $CacheRelative 'HITMAN-Coop-Foundation-4.1\HITMAN-Coop-Launcher.exe'
    $Launcher=Join-Path $PSScriptRoot $Relative
    $LauncherHash=(Get-FileHash -LiteralPath $Launcher).Hash.ToLowerInvariant()
    if ($LauncherHash -ne '9e9aed3b19f31677bde5ed8c90a3ef98f2205ebcd3f02760d513f8b75d74eea8') { throw 'Restored launcher verification failed.' }
    @{mode='foundation'; launcher=$Relative; sha256=$LauncherHash} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'launcher-mode.json') -Encoding UTF8
    Write-Host 'FOUNDATION 4.1 RESTORED. Choose 4 in START-HERE to open the restored launcher.' -ForegroundColor Green
} catch { Write-Host ('RESTORE STOPPED: '+$_.Exception.Message) -ForegroundColor Red; exit 1 }
