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
            widget={IsValid=function() return true end,IsA=function() return true end,IsInViewport=function() return menu end,
                RemoveFromParent=function() menu=false end}
            hud={IsValid=function() return true end,IsA=function() return true end,
                m_widgets={ForEach=function(self,fn) if menu then fn(0,{get=function() return widget end}) end end}}
            pc={IsWaitingForStreamingAndLoading=function() return false end,m_gameStateManager=manager,GetWorld=function() return world end,GetHUD=function() return hud end}
            utility={GetRealTimeSeconds=function() return clock end,IsGamePaused=function() return paused end,SetGamePaused=function(self,p,value) paused=value;return true end}
            settings={AutoPauseEnabled=true,IsValid=function() return true end,GetFullName=function() return "Settings Local" end}
            notices={}
            StaticFindObject=function() return {IsValid=function() return true end,
                PrintString=function(self,p,text) notices[#notices+1]=text end} end
            dofile=function() return {player=function() return {},pc end,gameplay=function() return utility end,settings=function() return settings end,singleton=function() return manager end} end
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
            P.start();poll();assert(not paused);assert(not settings.AutoPauseEnabled)
            files["Mods/StrayCoop/remote.tsv"]="STRAYCOOP\\t6\\t1\\t1\\t1\\n"
            -- Cinematic/internal suspension is not a player pause request.
            suspended=true;poll();assert(not paused)
            -- Unknown native menus are never dismissed or treated as pause input.
            suspended=false;menu=true;poll();assert(not paused and menu)
            -- Unowned native pause must stay untouched through hundreds of ticks.
            paused=true
            for i=1,300 do poll() end
            assert(paused and menu)
            paused=false;menu=false;poll()
            -- Actual Escape UI can exist while the native manager says false.
            hooks["/Script/Hk_project.GameStateManager:_OnMenuStartPressed"][1]()
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
            -- A disconnect may release the peer, but never an explicit local menu.
            hooks["/Script/Hk_project.GameStateManager:_OnMenuStartPressed"][1]()
            menu=true;poll();assert(paused)
            P.stop();assert(paused);assert(settings.AutoPauseEnabled)
            assert(#notices>0 and notices[1]:find("Partner is paused",1,true))
            -- A rejected native change cannot cause continuous pause calls.
            files["Mods/StrayCoop/pause-remote.tsv"]=""
            menu=false;paused=false;P.start();poll();calls=0
            utility.SetGamePaused=function(self,p,value) calls=calls+1;return false end
            hooks["/Script/Hk_project.GameStateManager:_OnMenuStartPressed"][1]()
            menu=true
            for i=1,300 do clock=clock+.05;poll() end
            assert(calls==3,"pause retries: "..calls.." errors="..tostring(StrayCoopPause.errors).." explicit="..tostring(StrayCoopPause.explicit).." desired="..tostring(StrayCoopPause.desired))
            P.stop()
        ''')

if __name__=='__main__': unittest.main(verbosity=2)
