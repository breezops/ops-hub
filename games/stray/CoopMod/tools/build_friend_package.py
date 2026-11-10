"""Build a mod-only Windows friend-test archive from the verified local runtime."""
import hashlib
import json
from pathlib import Path
import shutil
import sys
import zipfile
from bridge import VERSION as PROTOCOL

ROOT=Path(__file__).resolve().parents[2]
PROJECT=ROOT/'CoopMod'
VERSION='0.1.4'
MAIN='''local root="Mods/StrayCoop/"
local Context=dofile(root.."Scripts/context.lua")
local started=false
local queued=false
local function valid(o) return o~=nil and o:IsValid() end
Context.loop(500,function()
    if queued then return false end
    queued=true
    Context.run(function()
        local ok,err=pcall(function()
            if not started then
                dofile(root.."Scripts/cooperative.lua").start()
                dofile(root.."Scripts/cinematics.lua").start()
                dofile(root.."Scripts/pause.lua").start()
                dofile(root.."Scripts/objects.lua").start()
                dofile(root.."Scripts/movement.lua").start()
                dofile(root.."Scripts/story.lua").start()
                started=true
            end
            local settings=Context.settings()
            if valid(settings) then settings.AutoPauseEnabled=false end
            local f=io.open(root.."command.txt","r")
            if f then
                local command=f:read("*a"):match("^%s*(.-)%s*$");f:close()
                if command=="quit" then
                    f=io.open(root.."command.txt","w");if f then f:close() end
                    for _,pc in ipairs(FindAllOf("PlayerController") or {}) do
                        if valid(pc) and pc:IsLocalController() then
                            StaticFindObject("/Script/Engine.Default__KismetSystemLibrary"):QuitGame(pc,pc,0,true)
                            break
                        end
                    end
                end
            end
        end)
        if not ok then print("[StrayCoop/Friend] "..tostring(err).."\\n") end
        queued=false
    end)
    return false
end)
print("[StrayCoop/Friend] Protocol 6: shared cinematic skips, carryables, pause and buffered movement.\\n")
'''

def main():
    output=PROJECT/'dist'/f'StrayCoop-Friend-{VERSION}'
    if output.exists(): raise RuntimeError('Build folder exists; choose a new version or inspect the existing build.')
    runtime=output/'runtime';runtime.mkdir(parents=True)
    python=Path(sys.base_prefix)
    shutil.copy2(sys.executable,runtime/'python.exe')
    for source in python.glob('*.dll'): shutil.copy2(source,runtime/source.name)
    shutil.copy2(python/'LICENSE.txt',runtime/'PYTHON-LICENSE.txt')
    shutil.copytree(python/'DLLs',runtime/'DLLs',ignore=shutil.ignore_patterns('__pycache__','*.pdb'))
    with zipfile.ZipFile(runtime/'stdlib.zip','w',zipfile.ZIP_DEFLATED) as archive:
        for source in (python/'Lib').rglob('*.py'):
            relative=source.relative_to(python/'Lib')
            if any(p in {'site-packages','test','tests','idlelib','tkinter','turtledemo','ensurepip','venv','__pycache__'} for p in relative.parts): continue
            archive.write(source,relative.as_posix())
    tag=f'python{sys.version_info.major}{sys.version_info.minor}'
    (runtime/(tag+'._pth')).write_text('stdlib.zip\n.\nDLLs\n../tools\n',encoding='ascii')
    tools=output/'tools';tools.mkdir()
    for name in ('bridge.py','cooperative.py','cinematics.py','remote_session.py','test_bridge.py','preferences.py','shared_objects.py','test_shared_objects.py'):
        shutil.copy2(PROJECT/'tools'/name,tools/name)
    payload=output/'payload';payload.mkdir()
    loader=PROJECT/'vendor/UE4SS_v3.0.1'
    for name in ('dwmapi.dll','UE4SS.dll'): shutil.copy2(loader/name,payload/name)
    shutil.copy2(ROOT/'Hk_project/Binaries/Win64/UE4SS-settings.ini',payload/'UE4SS-settings.ini')
    scripts=payload/'Mods/StrayCoop/Scripts';scripts.mkdir(parents=True)
    for name in ('movement.lua','cooperative.lua','animation.lua','interpolation.lua','objects.lua','pause.lua','story.lua','context.lua','cinematics.lua'):
        shutil.copy2(PROJECT/'src/StrayCoop/Scripts'/name,scripts/name)
    (scripts/'main.lua').write_text(MAIN,encoding='utf-8')
    shutil.copy2(PROJECT/'vendor/UE4SS-LICENSE.txt',output/'UE4SS-LICENSE.txt')
    for filename,role in (('Host.cmd','host'),('Join.cmd','join'),('Preflight.cmd','preflight')):
        (output/filename).write_text('@echo off\n"%~dp0runtime\\python.exe" "%~dp0tools\\remote_session.py" '+role+' %*\npause\n',encoding='ascii',newline='\r\n')
    (output/'README-FIRST.txt').write_text('''STRAY CO-OP FRIEND TEST 0.1.4 - Windows x64

SHARED CARRYABLES: The common COMP_Grabbable pickup/drop system now shares native
objects, including the bucket handle and container, with host-owned pickup claims.
The Dead City bucket/fan blockage replays the native puzzle result on both PCs.
This is the first world-object adapter; other doors, inventory, switches and
scripted puzzle types still need separate synchronization and testing.

SHARED PAUSE: Either player opening the pause menu freezes both games. Play
resumes after both players close their pause menus. Each menu stays local.
Both players must update to protocol 5. Remote pause clears after disconnect.

Player graphics, controls, audio and language settings are carried between test
runs. A fresh campaign profile no longer forces a resolution or window mode.
Only background auto-pause and first-launch setup are disabled for co-op tests.
Each PC keeps its own settings cache under StrayCoopTests/Preferences; no normal
campaign saves are copied into the test profile or changed.

Both players must update. This build removes repeated global object searches,
uses decorative mesh actors instead of gameplay CatPawn clones, and matches
remote visuals to opening/campaign phase. Remote meow audio is temporarily omitted.
Remote visuals now interpolate buffered snapshots at approximately 60 updates
per second, with 100 ms of visual buffering. Jump poses follow the same timeline.
The isolated test profile starts with background auto-pause disabled. Launcher
windows print frame-rate diagnostics during gameplay.

This archive contains mod files and a private Python runtime. No game assets,
Steam/Spacewar replacement, or Python installation is required.

Both PCs must have the exact tested Stray executable. Preflight.cmd checks its
SHA-256 and changes no game files. Different builds stop with an error.

1. Extract the entire ZIP to a normal folder. Both players run Preflight.cmd and
   paste their Stray folder (the folder containing Hk_project).
2. Host forwards UDP 7777 on the router to the host PC's LAN IPv4, port 7777.
3. Host runs Host.cmd. It installs the mod, then displays a fresh session key.
4. Friend runs Join.cmd and enters the host PUBLIC IPv4 and that session key.
   For LAN/VPN tests use the host LAN/VPN IPv4 instead. The friend needs no forward.
5. Leave both launcher windows open. Once connected, both games start with fresh
   isolated test saves. Both choose START GAME > SLOT 1 > NEW GAME.
6. Check that each sees the other move. Split the three opening cat interactions
   between players, then let either player sleep. Both must see the transition,
   regain control, and keep seeing both player cats afterward.

If Windows requests network access, the user must handle that prompt. This
launcher does not create firewall rules, change router settings, or use a relay.
If connection fails, check the IPv4, session key, and UDP rule. Some ISP/router
setups require a VPN or relay; this build does not implement automatic NAT traversal.

Movement/animation, shared pause, carryable pickup/drop, the Dead City bucket/fan
puzzle and the opening interactions/sleep are shared.
Later cutscenes, enemies, inventory, checkpoints, and general campaign travel are
NOT synchronized. Cinematics are not frame-locked. This is a remote prototype test.

Only UE4SS loader/config and StrayCoop scripts are installed. Existing different
loaders are rejected. Changed existing mod files are backed up under the game's
StrayCoopTests/Backups. Normal saves are not used: each launch has its own UserData
under StrayCoopTests. Close Stray/Ctrl+C in the launcher to end the owned test.
Logs are in the printed StrayCoopTests session folder. Never share the session key
publicly. A new host launch creates a new key.
''',encoding='utf-8')
    manifest={str(f.relative_to(output)).replace('\\','/'):{'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()}
              for f in output.rglob('*') if f.is_file()}
    (output/'manifest.json').write_text(json.dumps({'version':VERSION,'protocol':PROTOCOL,'files':manifest},indent=2)+'\n')
    archive=Path(str(output)+'.zip')
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for file in output.rglob('*'):
            if file.is_file(): z.write(file,Path(output.name)/file.relative_to(output))
    print(json.dumps({'folder':str(output),'zip':str(archive),'bytes':archive.stat().st_size,
                      'sha256':hashlib.sha256(archive.read_bytes()).hexdigest()},indent=2))

if __name__=='__main__': main()
