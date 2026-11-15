[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vsRoot) { throw 'Install the Visual Studio C++ x64 build tools.' }
$out = Join-Path $PSScriptRoot 'bin'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$vcvars = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat'
$core = Join-Path (Split-Path $PSScriptRoot) 'core'
$commands = @(
    ('@call "' + $vcvars + '" >nul'),
    ('cl /nologo /std:c++17 /utf-8 /EHsc /W4 /WX /O2 /MT /DUNICODE /D_UNICODE /Fe:MW2-SpecOps4-Launcher.exe "' + $PSScriptRoot + '\src\main.cpp" "' + $PSScriptRoot + '\src\diagnostics.cpp" "' + $core + '\frame_codec.cpp" /link /SUBSYSTEM:WINDOWS /INCREMENTAL:NO /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA bcrypt.lib user32.lib gdi32.lib shell32.lib ole32.lib uuid.lib advapi32.lib comdlg32.lib dwmapi.lib'),
    'if errorlevel 1 exit /b %errorlevel%',
    ('cl /nologo /std:c++17 /utf-8 /EHsc /W4 /WX /O2 /MT /Fe:SpecOps4-Codec-Check.exe "' + $core + '\codec_check.cpp" "' + $core + '\frame_codec.cpp" /link /INCREMENTAL:NO'),
    'exit /b %errorlevel%'
)
$batch = Join-Path $out 'compile.cmd'
Set-Content -LiteralPath $batch -Value ($commands -join "`r`n") -Encoding ascii
Push-Location $out
try { & $env:ComSpec /d /c $batch; if ($LASTEXITCODE) { throw 'Launcher build failed.' } } finally { Pop-Location }
