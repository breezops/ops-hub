from pathlib import Path
import unittest
from lupa.lua54 import LuaRuntime

class ContextReferenceTests(unittest.TestCase):
    def test_cached_names_resolve_in_current_callback_without_rescanning(self):
        lua=LuaRuntime()
        lua.execute('''
            generation=1;scans=0;lookups=0
            function object()
                local owner=generation
                return {IsValid=function() assert(owner==generation,"wrapper belongs to old callback");return true end,
                    GetFullName=function() return "Thing /Engine/Transient.LiveThing" end}
            end
            FindFirstOf=function() scans=scans+1;return object() end
            StaticFindObject=function() lookups=lookups+1;return object() end
        ''')
        path=Path(__file__).resolve().parents[1]/'src/StrayCoop/Scripts/context.lua'
        lua.globals().C=lua.execute(path.read_text())
        lua.execute('''
            assert(C.singleton("Thing"):IsValid())
            generation=2
            assert(C.singleton("Thing"):IsValid())
            assert(scans==1 and lookups==1)
            assert(C.gameplay():IsValid())
            generation=3
            assert(C.gameplay():IsValid())
        ''')

    def test_engine_callback_coroutine_remains_rooted_with_gc_enabled(self):
        lua=LuaRuntime()
        lua.execute('StaticFindObject=function() return {} end')
        path=Path(__file__).resolve().parents[1]/'src/StrayCoop/Scripts/context.lua'
        lua.globals().C=lua.execute(path.read_text())
        lua.execute('''
            local weak=setmetatable({}, {__mode="v"})
            do
                local callback=coroutine.create(function() C.gameplay() end)
                weak[1]=callback
                assert(coroutine.resume(callback))
            end
            collectgarbage("collect")
            collectgarbage("collect")
            assert(collectgarbage("isrunning"))
            assert(weak[1]~=nil and StrayCoopThreadRoots[weak[1]])
        ''')

class SchedulerTests(unittest.TestCase):
    def test_shared_worker_bounds_queue_and_runs_due_timers(self):
        lua=LuaRuntime()
        lua.execute("workers=0;queue={};LoopAsync=function(ms,cb) workers=workers+1;poll=cb end;ExecuteInGameThread=function(cb) queue[#queue+1]=cb end")
        path=Path(__file__).resolve().parents[1]/'src/StrayCoop/Scripts/context.lua'
        lua.globals().C=lua.execute(path.read_text())
        lua.execute("""
            a=0;b=0
            C.loop(100,function() C.run(function() a=a+1 end) end)
            C.loop(200,function() b=b+1;return true end)
            assert(workers==1)
            poll();poll();poll();assert(#queue==1)
            queue[1]();assert(a==1 and b==1 and #C.timers==1)
            assert(#queue==1) -- Context.run did not enqueue nested work
            C.step(C.last_step+.05);assert(a==1)
            C.step(C.last_step+.1);assert(a==2)
            C.step(0);assert(a==3) -- clock rollback re-arms timers
            poll();assert(#queue==2)
        """)

if __name__=='__main__':unittest.main()
