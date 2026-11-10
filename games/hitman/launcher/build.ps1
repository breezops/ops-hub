$ErrorActionPreference = 'Stop'
$Framework = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319'
$Output = Join-Path $PSScriptRoot 'bin'
New-Item -ItemType Directory -Force -Path $Output | Out-Null
$Compiler = Join-Path $Framework 'csc.exe'
$Arguments = @('/nologo','/codepage:65001','/target:winexe','/platform:x64','/optimize+',('/out:' + (Join-Path $Output 'HITMAN-Coop-Launcher.exe')),
    '/reference:System.IO.Compression.dll','/reference:System.IO.Compression.FileSystem.dll','/reference:System.dll','/reference:System.Core.dll','/reference:System.Web.Extensions.dll','/reference:System.Xaml.dll',
    ('/reference:' + (Join-Path $Framework 'WPF\PresentationFramework.dll')),
    ('/reference:' + (Join-Path $Framework 'WPF\PresentationCore.dll')),
    ('/reference:' + (Join-Path $Framework 'WPF\WindowsBase.dll')),
    (Join-Path $PSScriptRoot 'Launcher.cs'))
& $Compiler @Arguments
if ($LASTEXITCODE) { throw 'Launcher compilation failed' }
$PreviewArguments = @($Arguments | ForEach-Object {
    if ($_ -like '/out:*') { '/out:' + (Join-Path $Output 'HITMAN-Coop-Party-Preview.exe') } else { $_ }
}) + @('/main:PartyPreviewLauncher', (Join-Path $PSScriptRoot 'PartyPreviewLauncher.cs'), (Join-Path $PSScriptRoot 'LauncherUpdate.cs'), ('/resource:'+(Join-Path $PSScriptRoot 'LauncherLayout.xaml')+',LauncherLayout.xaml'))
& $Compiler @PreviewArguments
if ($LASTEXITCODE) { throw 'Party preview launcher compilation failed' }
Write-Host 'Launcher built using Windows .NET Framework; no developer tools needed on player PCs.'

Copy-Item -LiteralPath (Join-Path $PSScriptRoot "updater-config.json") -Destination $Output -Force
