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
$project = Split-Path $PSScriptRoot
$version = Get-Content -LiteralPath (Join-Path $project 'version.json') -Raw | ConvertFrom-Json
function Cpp-String([string]$Value) { 'L"' + $Value.Replace('\','\\').Replace('"','\"').Replace("`r",'').Replace("`n",'\n') + '"' }
$generatedVersion = @(
    '#pragma once',
    ('#define MW2_VERSION_LITERAL ' + (Cpp-String $version.version)),
    'namespace mw2 {',
    ('inline constexpr wchar_t Version[] = ' + (Cpp-String $version.version) + ';'),
    ('inline constexpr wchar_t DisplayVersion[] = ' + (Cpp-String $version.display) + ';'),
    ('inline constexpr int LobbyProtocol = ' + $version.lobby_protocol + ';'),
    '}'
)
Set-Content -LiteralPath (Join-Path $out 'version.h') -Value $generatedVersion -Encoding utf8
$catalog = Get-Content -LiteralPath (Join-Path $project 'missions.json') -Raw | ConvertFrom-Json
$generatedCatalog = @('#pragma once','namespace mw2 {','struct Mission { const wchar_t* id; const wchar_t* name; const wchar_t* brief; bool concept; };','inline constexpr Mission MissionCatalog[] = {')
foreach ($mission in $catalog) {
    $generatedCatalog += ('{' + (Cpp-String $mission.id) + ',' + (Cpp-String $mission.name) + ',' + (Cpp-String $mission.brief) + ',' + ([string]$mission.concept).ToLowerInvariant() + '},')
}
$generatedCatalog += '};','}'
Set-Content -LiteralPath (Join-Path $out 'missions.h') -Value $generatedCatalog -Encoding utf8
$commands = @(
    ('@call "' + $vcvars + '" >nul'),
    ('rc /nologo /I "' + $PSScriptRoot + '" /fo resources.res "' + $PSScriptRoot + '\resources.rc"'),
    'if errorlevel 1 exit /b %errorlevel%',
    ('cl /nologo /std:c++17 /utf-8 /EHsc /W4 /WX /O2 /MT /I . /DUNICODE /D_UNICODE /Fe:MW2-SpecOps4-Launcher.exe "' + $PSScriptRoot + '\src\main.cpp" "' + $PSScriptRoot + '\src\lobby.cpp" "' + $PSScriptRoot + '\src\diagnostics.cpp" "' + $core + '\frame_codec.cpp" resources.res /link /SUBSYSTEM:WINDOWS /INCREMENTAL:NO /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA bcrypt.lib user32.lib gdi32.lib shell32.lib ole32.lib uuid.lib advapi32.lib comdlg32.lib dwmapi.lib gdiplus.lib winhttp.lib windowsapp.lib'),
    'if errorlevel 1 exit /b %errorlevel%',
    ('cl /nologo /std:c++17 /utf-8 /EHsc /W4 /WX /O2 /MT /Fe:SpecOps4-Codec-Check.exe "' + $core + '\codec_check.cpp" "' + $core + '\frame_codec.cpp" /link /INCREMENTAL:NO'),
    'if errorlevel 1 exit /b %errorlevel%',
    ('cl /nologo /std:c++17 /utf-8 /EHsc /W4 /WX /O2 /MT /I . /Fe:MW2-SpecOps4-Lobby-Check.exe "' + $PSScriptRoot + '\src\lobby_check.cpp" "' + $PSScriptRoot + '\src\lobby.cpp" /link /INCREMENTAL:NO bcrypt.lib winhttp.lib windowsapp.lib'),
    'exit /b %errorlevel%'
)
$batch = Join-Path $out 'compile.cmd'
Set-Content -LiteralPath $batch -Value ($commands -join "`r`n") -Encoding ascii
Push-Location $out
try { & $env:ComSpec /d /c $batch; if ($LASTEXITCODE) { throw 'Launcher build failed.' } } finally { Pop-Location }
