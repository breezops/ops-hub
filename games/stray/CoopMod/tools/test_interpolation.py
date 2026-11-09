"""Pure Lua 5.4 tests; requires lupa in the development test environment only."""
from pathlib import Path
import unittest
from lupa.lua54 import LuaRuntime

SCRIPTS=Path(__file__).resolve().parents[1]/'src/StrayCoop/Scripts'


class InterpolationTests(unittest.TestCase):
    def setUp(self):
        self.lua=LuaRuntime(unpack_returned_tuples=True)
        self.lua.globals().I=self.lua.execute((SCRIPTS/'interpolation.lua').read_text())
        self.lua.execute('I.delay=.10')
        self.lua.execute('''
            record={}
            function push(t,x,mode,phase,yaw,sequence,asset)
                return I.push(record,{t=t,sequence=sequence or t,
                    pose={x,0,10,0,yaw or 0,0},
                    animation={mode or 1,asset or "walk",1,phase or -1,1}})
            end
        ''')

    def test_all_modules_compile_on_lua54(self):
        for file in SCRIPTS.glob('*.lua'):
            self.lua.execute('assert(load(...))',file.read_text())

    def test_moves_between_packets_and_wraps_rotation_shortest_way(self):
        self.lua.execute('push(1,0,1,-1,179);push(1.05,10,1,-1,-179)')
        pose,_=self.lua.eval('I.sample(record,1.125)')
        self.assertAlmostEqual(pose[1],5)
        self.assertAlmostEqual(pose[5],180)
        xs=[self.lua.eval(f'I.sample(record,{t})')[0][1] for t in (1.11,1.12,1.13,1.14)]
        self.assertEqual(sorted(set(xs)),xs)

    def test_jump_position_and_phase_share_one_timeline(self):
        self.lua.execute('push(1,0,4,0.2);push(1.05,50,4,0.6)')
        pose,anim=self.lua.eval('I.sample(record,1.125)')
        self.assertAlmostEqual(pose[1],25)
        self.assertAlmostEqual(anim[4],0.4)
        pose,anim=self.lua.eval('I.sample(record,2)')
        self.assertEqual(pose[1],50)  # Never extrapolate an airborne trajectory.
        self.assertEqual(anim[4],0.6)

    def test_late_packets_use_bounded_prediction(self):
        self.lua.execute('push(1,0);push(1.05,10)')
        pose,_=self.lua.eval('I.sample(record,1.2)')
        self.assertAlmostEqual(pose[1],20)
        pose,_=self.lua.eval('I.sample(record,10)')
        self.assertAlmostEqual(pose[1],30)

    def test_duplicates_do_not_invent_samples_and_history_is_bounded(self):
        self.lua.execute('push(1,0,1,-1,0,1);push(1.01,99,1,-1,0,1)')
        self.assertEqual(self.lua.eval('#record.samples'),1)
        self.lua.execute('for i=2,100 do push(1+i*.05,i,1,-1,0,i) end')
        self.assertEqual(self.lua.eval('#record.samples'),12)

    def test_teleport_clock_reset_and_long_gap_clear_old_path(self):
        for setup in ('push(1,0);push(1.05,2000)',
                      'push(1,0);push(0.5,20)',
                      'push(1,0);push(2,20)'):
            self.lua.execute('record={};'+setup)
            self.assertEqual(self.lua.eval('#record.samples'),1)

    def test_animation_changes_wait_for_buffered_transition(self):
        self.lua.execute('push(1,0,4,0.9,0,1,"jump");push(1.05,50,1,-1,0,2,"walk")')
        _,anim=self.lua.eval('I.sample(record,1.125)')
        self.assertEqual(anim[2],'jump')
        _,anim=self.lua.eval('I.sample(record,1.16)')
        self.assertEqual(anim[2],'walk')

    def test_source_clock_ignores_receive_jitter(self):
        self.lua.execute('''
            push(1,0)
            record={}
            I.push(record,{t=10,source_t=1,sequence=1,pose={0,0,0,0,0,0},animation={1,"walk",1,-1,1}})
            I.push(record,{t=10.09,source_t=1.05,sequence=2,pose={10,0,0,0,0,0},animation={1,"walk",1,-1,1}})
        ''')
        pose,_=self.lua.eval('I.sample(record,10.125)')
        self.assertAlmostEqual(pose[1],5,delta=.1)
        self.assertAlmostEqual(self.lua.eval('record.samples[2].t-record.samples[1].t'),.05)

    def test_render_corrections_smooth_but_real_teleports_snap(self):
        self.lua.execute('I.display(record,{0,0,0,0,179,0},1)')
        pose=self.lua.eval('I.display(record,{100,0,0,0,-179,0},1.016)')
        self.assertGreater(pose[1],0);self.assertLess(pose[1],100)
        self.assertLess(abs(pose[5]-179),2)
        pose=self.lua.eval('I.display(record,{2000,0,0,0,0,0},1.032)')
        self.assertEqual(pose[1],2000)

    def test_jump_is_phase_controlled_and_small_rate_changes_accumulate(self):
        self.lua.globals().A=self.lua.execute((SCRIPTS/'animation.lua').read_text())
        self.lua.execute('''
            sequence={SequenceLength=2,IsValid=function() return true end,IsA=function() return true end}
            StaticFindObject=function() return sequence end
            v={GetAnimationMode=function() return 1 end,
                PlayAnimation=function() end,
                SetPlayRate=function(self,rate) self.rate=rate end,
                SetPosition=function(self,pos) self.pos=pos end}
            r={}
            A.apply(v,{4,"jump",1,0.4,0},r)
            assert(v.rate==0 and v.pos==0.8)
            A.apply(v,{1,"walk",1,-1,1},r)
            A.apply(v,{1,"walk",1.01,-1,1},r)
            A.apply(v,{1,"walk",1.03,-1,1},r)
            assert(v.rate==1.03)
        ''')


if __name__=='__main__': unittest.main(verbosity=2)
