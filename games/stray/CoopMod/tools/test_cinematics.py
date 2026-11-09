from pathlib import Path
import tempfile,time,unittest
from bridge import Bridge,VERSION,idle_state
from cinematics import Skips,validate,read,validate_events
from lupa.lua54 import LuaRuntime

ASSET='/Game/StrayCoop/Cinematics/Chapter_0'
class CinematicsTests(unittest.TestCase):
    def test_intent_bounds_and_expiry_cannot_rearm_old_scene(self):
        for value in (['/Script/Engine',1,1],[ASSET,True,1],['-',1,0],[ASSET,1,2],[ASSET,0,1]):
            with self.assertRaises(ValueError):validate(value)
        s=Skips();s.merge([ASSET,1,1,1],10)
        self.assertEqual(s.snapshot(11),[[ASSET,1,1]])
        s.merge([ASSET,1,1,1],101);self.assertEqual(s.snapshot(101),[])
        s.merge([ASSET,2,1,1],102);self.assertEqual(s.snapshot(102),[[ASSET,2,1]])
        s.merge([ASSET,2,1,0],103,1);s.merge([ASSET,2,0,0],103,2)
        self.assertEqual(s.snapshot(103),[[ASSET,2,2]])
        with self.assertRaises(ValueError):validate_events([[ASSET,1,1]]*5)
    def test_either_peer_requests_skip_repeated_through_packet_loss(self):
        for requester in (1,2):
            with self.subTest(requester=requester),tempfile.TemporaryDirectory() as tmp:
                h=Bridge('host',Path(tmp)/'h','cinematic-test-key',bind='127.0.0.1',port=0)
                c=Bridge('join',Path(tmp)/'c','cinematic-test-key',bind='127.0.0.1',port=h.sock.getsockname()[1])
                now=time.monotonic()
                def pump(i,drop=False):
                    for b in (h,c):b.tick(now+i*.05)
                    for b in (h,c):
                        while True:
                            try:data,address=b.sock.recvfrom(4097)
                            except BlockingIOError:break
                            if not drop:b.receive(data,address,now+i*.05)
                try:
                    for i in range(10):pump(i)
                    (h if requester==1 else c).directory.joinpath('cinematic-local.tsv').write_text(f'CINEMATIC\t{ASSET}\t1\t1\t1\n')
                    for i in range(10,14):pump(i,True)
                    for i in range(14,24):pump(i)
                    self.assertEqual(h.shared_skips,[[ASSET,1,1]])
                    self.assertEqual(c.shared_skips,h.shared_skips)
                    for b in (h,c):self.assertIn(f'SKIP\t{ASSET}\t1\t1\n',(b.directory/'cinematic-remote.tsv').read_text())
                    c.tick(now+8);self.assertNotIn('SKIP\t',(c.directory/'cinematic-remote.tsv').read_text())
                finally:h.sock.close();c.sock.close()

    def test_native_completion_scoped_hold_debounce_pause_and_duplicate(self):
        # Exercise the actual Lua tick with engine mocks. Native fixture tests
        # separately verify accelerated native completion releases control.
        lua=LuaRuntime(unpack_returned_tuples=True)
        lua.execute('''
            now=0;down=false;paused=false;playing=true;cinematic=true;completed=0;dilation=1
            files={};gen=0
            pc={IsValid=function()return true end,GetWorld=function()return world end,
                IsWaitingForStreamingAndLoading=function()return false end,
                IsIngameCinematicModeEnabled=function()return cinematic end,
                IsInputKeyDown=function()return down end}
            world={IsValid=function()return true end,GetFullName=function()return "World /Game/Test.Test" end}
            owner={IsValid=function()return true end,IsA=function()return true end,GetWorld=function()return world end}
            seq={IsValid=function()return true end,GetFullName=function()return "LevelSequence /Game/Map/Opening/Cine.Cine" end}
            player={Sequence=seq,IsValid=function()return true end,IsPlaying=function()return playing end,
                rate=1,PlaybackSettings={LoopCount={Value=0}},GetPlayRate=function(self)return self.rate end,SetPlayRate=function(self,value)self.rate=value end,
                GetFullName=function()return "LevelSequencePlayer /Game/Test.Player" end,
                GetOuter=function()return owner end,GoToEndAndStop=function() completed=completed+1;playing=false;cinematic=false end}
            dofile=function()return {player=function()return nil,pc end,
                singleton=function()return {IsValid=function()return true end,GetCurrentAdventureChapter=function()return 0 end} end,
                gameplay=function()return {
                GetRealTimeSeconds=function()return now end,IsGamePaused=function()return paused end,
                SetGamePaused=function(_,context,value)paused=value;return true end,
                GetGlobalTimeDilation=function()return dilation end,SetGlobalTimeDilation=function(_,context,value)dilation=value end} end} end
            FindAllOf=function()return {player} end;StaticFindObject=function()return player end
            FName=function(s)return s end;EFindName={FNAME_Add=0}
            io.open=function(name,mode)
                if mode=="r" then return {read=function()return files[name] or "" end,close=function()end} end
                return {write=function(self,s)files[name]=s end,close=function()end}
            end
            LoopAsync=function(_,f)callback=f end;ExecuteInGameThread=function(f)f()end
            function tick(t)now=t;callback()end
            function remote(asset,count,phase)
                gen=gen+1;local t,n=string.char(9),string.char(10)
                files["Mods/StrayCoop/cinematic-remote.tsv"]="SKIPS"..t..gen..t.."1"..n.."SKIP"..t..asset..t..count..t..(phase or 1)..n
            end
        ''')
        module=lua.execute((Path(__file__).parents[1]/'src/StrayCoop/Scripts/cinematics.lua').read_text());module.start()
        lua.execute('tick(0);down=true;tick(.1);tick(.7)')
        self.assertTrue(lua.eval('files["Mods/StrayCoop/cinematic-local.tsv"]').endswith('\t0\t1\n'))
        lua.execute('tick(.91)')
        self.assertTrue(lua.eval('files["Mods/StrayCoop/cinematic-local.tsv"]').endswith('\t1\t1\n'))
        lua.execute('remote("/Game/Other.Other",1);tick(1);remote("/Game/StrayCoop/Cinematics/Chapter_0",2);tick(1.1)')
        self.assertEqual(lua.eval('completed'),0)
        lua.execute('paused=true;remote("/Game/StrayCoop/Cinematics/Chapter_0",1);tick(1.2)')
        self.assertEqual(lua.eval('completed'),0)
        lua.execute('paused=false;tick(1.3)')
        self.assertEqual(lua.eval('dilation'),20)
        self.assertEqual(lua.eval('player.rate'),20)
        lua.execute('cinematic=false;tick(1.4)')
        self.assertEqual(lua.eval('dilation'),1)
        self.assertEqual(lua.eval('player.rate'),1)
        self.assertTrue(lua.eval('paused'))
        lua.execute('remote("/Game/StrayCoop/Cinematics/Chapter_0",1,2);tick(1.5)')
        self.assertFalse(lua.eval('paused'))
if __name__=='__main__':unittest.main()
