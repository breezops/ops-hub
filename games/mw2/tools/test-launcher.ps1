[CmdletBinding()]
param([string]$GamePath)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$exe = Join-Path $root 'launcher\bin\MW2-SpecOps4-Launcher.exe'
$out = Join-Path $root 'test-output'
New-Item -ItemType Directory -Force -Path $out | Out-Null
function Assert-Diagnostic([string]$Folder, [string]$Name, [int]$ExpectedExit, [string]$ExpectedText) {
    $report = Join-Path $out ($Name + '.txt')
    $arguments = '--self-test "' + $Folder + '" --report "' + $report + '"'
    $process = Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
    if ($process.ExitCode -ne $ExpectedExit) { throw "$Name returned $($process.ExitCode), expected $ExpectedExit" }
    if (!(Select-String -LiteralPath $report -SimpleMatch $ExpectedText -Quiet)) { throw "$Name missing expected diagnostic text" }
    if (!(Select-String -LiteralPath $report -SimpleMatch 'FOUR-PLAYER MODE UNAVAILABLE' -Quiet)) { throw "$Name omitted mod status" }
    Write-Output ('PASS ' + $Name)
}
$missing = Join-Path $out 'missing-game'
New-Item -ItemType Directory -Force -Path $missing | Out-Null
Assert-Diagnostic $missing 'missing-installation' 1 'iw4sp.exe not found'
$unsupported = Join-Path $out 'unsupported-game'
New-Item -ItemType Directory -Force -Path $unsupported | Out-Null
Set-Content -LiteralPath (Join-Path $unsupported 'iw4sp.exe') -Value 'unsupported executable fixture' -Encoding ascii
Assert-Diagnostic $unsupported 'unsupported-executable' 1 'Different executable build'
if ($GamePath) {
    $original = Join-Path $GamePath 'iw4sp.exe'
    $before = (Get-FileHash -LiteralPath $original -Algorithm SHA256).Hash
    Assert-Diagnostic $GamePath 'supported-game-and-codec' 0 'Repeat application refused'
    & (Join-Path $root 'launcher\bin\SpecOps4-Codec-Check.exe') $original
    if ($LASTEXITCODE) { throw 'Standalone codec checks failed' }
    if ((Get-FileHash -LiteralPath $original -Algorithm SHA256).Hash -ne $before) { throw 'Installed executable changed' }
    Write-Output 'PASS installed executable unchanged'
} else { Write-Output 'SKIP real executable checks: supply -GamePath on a developer PC.' }
