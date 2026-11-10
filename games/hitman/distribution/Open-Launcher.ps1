param([switch]$CheckOnly)
$ErrorActionPreference='Stop'
try {
    $Mode=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'launcher-mode.json') -Raw | ConvertFrom-Json
    $Launcher=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot $Mode.launcher))
    if (-not $Launcher.StartsWith($PSScriptRoot.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid launcher location.' }
    if ((Get-FileHash -LiteralPath $Launcher).Hash -ne $Mode.sha256) { throw 'The launcher is damaged. Extract a fresh package.' }
    if ($CheckOnly) { Write-Host ('PASS: '+$Mode.mode+' launcher verified.'); exit 0 }
    Start-Process -FilePath $Launcher
} catch { Write-Host ('LAUNCH STOPPED: '+$_.Exception.Message) -ForegroundColor Red; exit 1 }
