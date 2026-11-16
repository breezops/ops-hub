param([string]$GamePath = '',[switch]$CheckOnly)
$ErrorActionPreference='Stop'
try {
    $Archive=Join-Path $PSScriptRoot 'rollback\HITMAN-Coop-0.1.3.zip'
    $Expected='5eaa60107478436fafe67f829f7a4798891f5ceab7fe68eadc65b5b2ab059fea'
    if (-not (Test-Path -LiteralPath $Archive -PathType Leaf) -or
        (Get-FileHash -LiteralPath $Archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Expected) {
        throw 'The preserved 0.1.3 package is missing or damaged. Extract a fresh copy of the candidate ZIP.'
    }
    if ($CheckOnly) { Write-Host 'RESTORE CHECK PASSED. Original 0.1.3 package verified; no files changed.'; exit 0 }
    if (Get-Process -Name HITMAN3 -ErrorAction SilentlyContinue) { throw 'Close HITMAN before restoring the tested build.' }
    $Root=[IO.Path]::GetFullPath($PSScriptRoot)
    $Destination=[IO.Path]::GetFullPath((Join-Path $Root 'rollback\restored-0.1.3'))
    if (-not $Destination.StartsWith($Root.TrimEnd('\')+'\rollback\',[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Invalid restoration directory.'
    }
    Expand-Archive -LiteralPath $Archive -DestinationPath $Destination -Force
    $Installer=Join-Path $Destination 'HITMAN-Coop\Install-Friend.ps1'
    if (-not (Test-Path -LiteralPath $Installer -PathType Leaf)) { throw 'The preserved package has no installer.' }
    $PowerShell=Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe'
    $InstallerArgs=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$Installer,'-Mode','Auto')
    if ($GamePath) { $InstallerArgs+=@('-GamePath',$GamePath) }
    & $PowerShell @InstallerArgs
    if ($LASTEXITCODE) { throw 'Restoration stopped. Read the setup details above.' }
    Write-Host 'TESTED OPS 0.1.3 RESTORED.' -ForegroundColor Green
    exit 0
} catch {
    Write-Host ('RESTORE STOPPED: '+$_.Exception.Message) -ForegroundColor Red
    exit 1
}
