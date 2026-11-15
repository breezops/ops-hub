[CmdletBinding()]
param([ValidateRange(1,3600)][int]$Seconds = 600,
      [string]$OutputPath = (Join-Path $env:TEMP ('mw2-lifecycle-' + [Guid]::NewGuid() + '.csv')))
$ErrorActionPreference = 'Stop'
if (![Environment]::Is64BitProcess) { throw 'Run in 64-bit PowerShell.' }
$game = @(Get-Process -Name iw4sp -ErrorAction SilentlyContinue)
if ($game.Count -ne 1) { throw 'Start the original MW2 single-player / Spec Ops game through Steam, then run again.' }
$game = $game[0]
$module = $game.MainModule
$expected = '2F8C9CFDA1B8C0FB308FD9719577154E8222FC7654F5E8F1BF54579BCA69D805'
if ((Get-FileHash -LiteralPath $module.FileName).Hash -ne $expected) { throw 'Unsupported MW2 executable.' }
if (-not ('MW2LifecycleRead' -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public static class MW2LifecycleRead {
    [DllImport("kernel32", SetLastError=true)]
    public static extern SafeProcessHandle OpenProcess(uint access, bool inherit, int id);
    [DllImport("kernel32", SetLastError=true)]
    public static extern bool ReadProcessMemory(SafeProcessHandle process, IntPtr address,
        byte[] data, UIntPtr count, out UIntPtr total);
}
"@
}
$handle = [MW2LifecycleRead]::OpenProcess(0x10, $false, $game.Id)
if ($handle.IsInvalid) { throw 'Cannot open game for read-only observation.' }
function Read-Field([long]$Rva, [int]$Size) {
    $bytes = [byte[]]::new($Size)
    [UIntPtr]$total = [UIntPtr]::Zero
    $address = [IntPtr]($module.BaseAddress.ToInt64() + $Rva)
    if (![MW2LifecycleRead]::ReadProcessMemory($handle,$address,$bytes,[UIntPtr]$Size,[ref]$total) -or $total.ToUInt64() -ne $Size) {
        throw 'Game memory sample unavailable.'
    }
    if ($Size -eq 8) { return [BitConverter]::ToInt64($bytes,0) }
    return [BitConverter]::ToInt32($bytes,0)
}
$stream = $null
try {
    $stream = [IO.StreamWriter]::new([IO.File]::Open($OutputPath,'CreateNew','Write','Read'))
    $stream.AutoFlush = $true
    $stream.WriteLine('elapsed_ms,slot,connection_state,server_state,entity_link_matches,player_link_matches')
    Write-Output "Sampling to $OutputPath"
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while (!$game.HasExited -and $timer.Elapsed.TotalSeconds -lt $Seconds) {
        $playerBase = Read-Field 0xd96e70 8
        foreach ($slot in 0,1) {
            $entity = 0xd9e070 + $slot * 0x2a0
            $server = 0x19ada98 + $slot * 0x1678
            $conn = Read-Field (0x540bb0 + $slot * 0x2650) 4
            $state = Read-Field $server 4
            $linked = (Read-Field ($server + 0x1670) 8) -eq ($module.BaseAddress.ToInt64() + $entity)
            $owned = $playerBase -ne 0 -and (Read-Field ($entity + 0x108) 8) -eq ($playerBase + $slot * 0xafe0)
            $stream.WriteLine(('{0},{1},{2},{3},{4},{5}' -f $timer.ElapsedMilliseconds,$slot,$conn,$state,[int]$linked,[int]$owned))
        }
        Start-Sleep -Milliseconds 250
    }
} finally {
    if ($stream) { $stream.Dispose() }
    $handle.Dispose()
}
