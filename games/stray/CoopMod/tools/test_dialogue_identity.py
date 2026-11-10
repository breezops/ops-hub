from pathlib import Path
import unittest
from lupa.lua54 import LuaRuntime

class DialogueIdentityTests(unittest.TestCase):
    def test_local_backpack_alias_survives_different_actor_numbers_but_not_other_dialogue(self):
        lua=LuaRuntime()
        lua.globals().I=lua.execute((Path(__file__).resolve().parents[1]/'src/StrayCoop/Scripts/dialogue_identity.lua').read_text())
        lua.execute("""
            local a='/Game/Map/_MainGame/BaseMap.BaseMap:PersistentLevel.BP_Backpack_C_111'
            local b='/Game/Map/_MainGame/BaseMap.BaseMap:PersistentLevel.BP_Backpack_C_999'
            local d='/Game/Map/_MainGame/03_Slums/Slums_GD.Slums_GD:PersistentLevel.Dial_b12_ask_the_guardian_2'
            local id=I.encode(a..'.externalTalkable',a,d)
            assert(id==I.encode(b..'.externalTalkable',b,d))
            assert(I.resolve(id,b,d)==b..'.externalTalkable')
            assert(I.resolve(id,b,d..'_other')==nil)
            assert(I.encode('Guardian.talkable',a,d)=='Guardian.talkable')
            assert(I.encode(b..'.externalTalkable',a,d)==b..'.externalTalkable')
            assert(I.resolve('/Script/Engine.Actor',b,d)=='/Script/Engine.Actor')
            local item=I.itemEncode(d,'Postcard')
            assert(I.itemResolve(item,d)=='Postcard')
            assert(I.itemResolve(item,d..'_other')==nil)
            assert(I.itemEncode(d,'../bad')==nil)
            assert(I.itemResolve(nil,d)==nil)
        """)
