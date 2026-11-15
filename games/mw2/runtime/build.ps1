[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vsRoot) { throw 'Install Visual Studio C++ x64 build tools.' }
$vcvars = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat'
$out = Join-Path $PSScriptRoot 'bin'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$source = Join-Path $PSScriptRoot 'winmm.cpp'
$def = Join-Path $PSScriptRoot 'winmm.def'
$exports = @(Get-Content -LiteralPath $def | ForEach-Object { $_.Trim() } |
             Where-Object { $_ -and $_ -ne 'LIBRARY winmm.dll' -and $_ -ne 'EXPORTS' })
if ($exports.Count -lt 150 -or @($exports | Where-Object { $_ -notmatch '^[A-Za-z][A-Za-z0-9]*$' }).Count) {
    throw 'Unexpected WinMM export list.'
}
$header = @('#pragma once', ('constexpr size_t WINMM_EXPORT_COUNT = ' + $exports.Count + ';'),
            'constexpr const char* winmm_names[] = {')
$asm = @('EXTERN winmm_real:QWORD', '.code')
for ($index = 0; $index -lt $exports.Count; ++$index) {
    $name = $exports[$index]
    $header += ('    "' + $name + '",')
    $asm += ('PUBLIC ' + $name), ($name + ' PROC'),
            ('    jmp QWORD PTR [winmm_real + ' + ($index * 8) + ']'),
            ($name + ' ENDP')
}
$header += '};'
$asm += 'END'
Set-Content -LiteralPath (Join-Path $out 'winmm_names.h') -Value $header -Encoding ascii
Set-Content -LiteralPath (Join-Path $out 'winmm_stubs.asm') -Value $asm -Encoding ascii
$command = '@call "' + $vcvars + '" >nul' + "`r`n" +
           'ml64 /nologo /c /Fo"' + (Join-Path $out 'winmm_stubs.obj') + '" "' + (Join-Path $out 'winmm_stubs.asm') + '"' + "`r`n" +
           'if errorlevel 1 exit /b %errorlevel%' + "`r`n" +
           'cl /nologo /std:c++17 /utf-8 /EHsc /W4 /WX /O2 /MT /LD /DUNICODE /D_UNICODE /Fe:"' +
           (Join-Path $out 'winmm.dll') + '" /I "' + $out + '" "' + $source + '" "' +
           (Join-Path $out 'winmm_stubs.obj') + '" /link /DEF:"' + $def + '" /INCREMENTAL:NO bcrypt.lib' + "`r`n" +
           'exit /b %errorlevel%'
$batch = Join-Path $out 'compile.cmd'
Set-Content -LiteralPath $batch -Value $command -Encoding ascii
Push-Location $out
try {
    & $env:ComSpec /d /c $batch
    if ($LASTEXITCODE) { throw 'Runtime loader build failed.' }
} finally { Pop-Location }
