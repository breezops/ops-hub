param([string]$Output=(Join-Path $PSScriptRoot '..\outputs'))
$ErrorActionPreference='Stop'
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
New-Item -ItemType Directory -Force -Path $Output | Out-Null
$Compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
& $Compiler /nologo /target:exe /platform:x64 /optimize+ ('/out:'+(Join-Path $Output 'TestLauncherUpdate.exe')) /reference:System.Web.Extensions.dll /reference:System.IO.Compression.dll /reference:System.IO.Compression.FileSystem.dll (Join-Path $Root 'launcher\LauncherUpdate.cs') (Join-Path $PSScriptRoot 'TestLauncherUpdate.cs')
if ($LASTEXITCODE) { throw 'Updater test compilation failed' }
& (Join-Path $Output 'TestLauncherUpdate.exe') (Join-Path $Output ('update-fixture-'+[Guid]::NewGuid().ToString('N')))
if ($LASTEXITCODE) { throw 'Updater tests failed' }
& (Join-Path $PSScriptRoot 'test-installer-guards.ps1') -Output $Output

$Launcher=Join-Path $Root 'launcher\bin\HITMAN-Coop-Party-Preview.exe'
$LayoutReport=Join-Path ([IO.Path]::GetFullPath($Output)) ('layouts-'+[Guid]::NewGuid().ToString('N')+'.json')
$LayoutProcess=Start-Process -FilePath $Launcher -ArgumentList @('--layout-test',('"'+$LayoutReport+'"')) -WindowStyle Hidden -PassThru -Wait
if ($LayoutProcess.ExitCode) { if (Test-Path -LiteralPath ($LayoutReport+'.error')) { Get-Content -LiteralPath ($LayoutReport+'.error') | Write-Host };throw 'Launcher layout checks failed' }
Write-Output 'PASS launcher controls reachable at 1120x820, 1120x690, 800x600 and 640x480 logical pixels'
