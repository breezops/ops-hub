"""Exercise the game adapter with native menu state and a simulated bridge."""
from pathlib import Path
import unittest
from lupa.lua54 import LuaRuntime

class PauseMenuTests(unittest.TestCase):
    def test_real_menu_without_native_suspension_peer_hold_and_timeout(self):
        lua=LuaRuntime()
        lua.execute('''
            files={};clock=100;menu=false;paused=false;suspended=false;hooks={}
            os.time=function() return clock end
            print=function() end
            local world={GetFullName=function() return "World /Game/Test.Test" end}
            manager={IsValid=function() return true end,IsSuspended=function() return suspended end}
            widget={IsValid=function() return true end,IsA=function() return true end,IsInViewport=function() return menu end}
            hud={IsValid=function() return true end,IsA=function() return true end,
                m_widgets={ForEach=function(self,fn) if menu then fn(0,{get=function() return widget end}) end end}}
            pc={m_gameStateManager=manager,GetWorld=function() return world end,GetHUD=function() return hud end}
            utility={GetRealTimeSeconds=function() return clock end,IsGamePaused=function() return paused end,SetGamePaused=function(self,p,value) paused=value;return true end}
            dofile=function() return {player=function() return {},pc end,gameplay=function() return utility end} end
            RegisterHook=function(name,pre,post) hooks[name]={pre,post} end
            LoopAsync=function(ms,fn) poll=fn end
            ExecuteInGameThread=function(fn) fn() end
            io.open=function(path,mode)
                if mode=="r" and files[path]==nil then return nil end
                return {write=function(self,s) files[path]=s end,
                    read=function() return files[path] end,close=function() end}
            end
        ''')
        path=Path(__file__).resolve().parents[1]/'src/StrayCoop/Scripts/pause.lua'
        lua.globals().P=lua.execute(path.read_text())
        lua.execute('''
            P.start();poll();assert(not paused)
            -- Cinematic/internal suspension is not a player pause request.
            suspended=true;poll();assert(not paused)
            -- Actual Escape UI can exist while the native manager says false.
            suspended=false;menu=true;poll()
            assert(paused and files["Mods/StrayCoop/pause-local.tsv"]=="PAUSE\\t1\\n")
            files["Mods/StrayCoop/pause-remote.tsv"]="PAUSE\\t1\\t1\\t0\\t1\\n"
            menu=false;poll();assert(paused)
            assert(files["Mods/StrayCoop/pause-local.tsv"]=="PAUSE\\t1\\n")
            clock=100.3;poll();assert(paused)
            assert(files["Mods/StrayCoop/pause-local.tsv"]=="PAUSE\\t0\\n")
            files["Mods/StrayCoop/pause-remote.tsv"]="PAUSE\\t2\\t1\\t0\\t0\\n"
            poll();assert(not paused)
            files["Mods/StrayCoop/pause-remote.tsv"]="PAUSE\\t3\\t1\\t0\\t1\\n"
            poll();assert(paused)
            clock=106;poll();assert(not paused)
            -- A disconnect may release the peer, but never an open local menu.
            menu=true;poll();assert(paused)
            P.stop();assert(paused)
        ''')

if __name__=='__main__': unittest.main(verbosity=2)
