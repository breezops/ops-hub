from pathlib import Path
import unittest
from lupa.lua54 import LuaRuntime

class GuardianEntryTests(unittest.TestCase):
    def test_received_scene_sets_native_entry_guard_and_cannot_reenter_or_echo(self):
        lua=LuaRuntime()
        scripts=Path(__file__).resolve().parents[1]/'src/StrayCoop/Scripts'
        lua.globals().Identity=lua.execute((scripts/'dialogue_identity.lua').read_text())
        lua.execute('''
            now=100;calls=0;files={};hooks={}
            guardianPath='/Game/Map/_MainGame/03_Slums/Slums_GD.Slums_GD:PersistentLevel.Seq_Slum_Meet_Guardian_2'
            guardian={SeqPlayed=false,IsValid=function() return true end,
                IsA=function(self,c) return c=='/Script/Hk_project.Sequence' end,
                LogicTrigger=function(self)
                    assert(self.SeqPlayed,'receiver must match proximity entry guard before native graph')
                    calls=calls+1
                end}
            StaticFindObject=function(p) if p==guardianPath then return guardian end end
            local invalid={IsValid=function() return false end}
            local b={IsValid=function() return true end,GetFullName=function() return 'Backpack /Game/Map/Test.Test:PersistentLevel.BP_Backpack_C_99' end,
                GetCurrentDialog=function() return invalid end,IsBackpackMeshVisible=function() return false end}
            local cat={IsValid=function() return true end,GetFullName=function() return 'Cat /Game/Map/Test.Test:PersistentLevel.Cat' end,
                GetBackpack=function() return b end}
            local pc={IsIngameCinematicModeEnabled=function() return false end}
            local context={player=function() return cat,pc end,gameplay=function() return {GetRealTimeSeconds=function() return now end} end}
            dofile=function(path) if path:find('context.lua',1,true) then return context elseif path:find('dialogue_identity.lua',1,true) then return Identity else return {} end end
            RegisterHook=function(name,fn) hooks[name]=fn end
            LoopAsync=function(ms,fn) poll=fn end
            ExecuteInGameThread=function(fn) fn() end
            print=function() end
            io.open=function(path,mode)
                if mode=='r' and not files[path] then return nil end
                return {read=function() return files[path] end,write=function(self,s) files[path]=s end,close=function() end}
            end
            files['Mods/StrayCoop/coop-remote.tsv']='COOP\\t1\\t1\\t2\\tsession\\nGEAR\\t0\\t0\\t0\\t0\\t0\\t0\\t0\\nEVENT\\t1\\tsequence\\t'..guardianPath..'\\t0\\n'
        ''')
        lua.globals().M=lua.execute((scripts/'cooperative.lua').read_text())
        lua.execute('''
            M.start();poll();assert(calls==1 and StrayCoopCooperative.errors==0)
            now=100.1;poll();assert(calls==1 and StrayCoopCooperative.applied==1)
            assert(StrayCoopCooperative.counter==0,'receiving the native cue must not echo it')
            for i=1,100 do now=now+.05;poll() end
            assert(calls==1 and StrayCoopCooperative.errors==0)
        ''')
