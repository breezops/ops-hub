from pathlib import Path
import os
import tempfile
import unittest
from unittest.mock import patch
from preferences import prepare_settings,remember_settings,settings_dir,cache_dir,coop_flags


class PreferencesTests(unittest.TestCase):
    def test_preferences_survive_fresh_campaign_without_copying_saves(self):
        with tempfile.TemporaryDirectory() as tmp,patch.dict(os.environ,{'LOCALAPPDATA':tmp+'/no-appdata'}):
            root=Path(tmp)
            original=root/'Hk_project/Saved/Config/WindowsNoEditor';original.mkdir(parents=True)
            content='[/Script/Hk_project.HKGameUserSettings]\nMasterVolume=0.42\nMusicVolume=0\nFullscreenMode=1\nResolutionSizeX=2560\nResolutionSizeY=1440\nAutoPauseEnabled=True\nFirstLaunch=True\nCustomKeyMappings=(Key=E)\nCustomKeyMappings=(Key=Q)\n'
            (original/'GameUserSettings.ini').write_text(content)
            (original/'Input.ini').write_text('+ActionMappings=(ActionName="Jump",Key=E)\n+ActionMappings=(ActionName="Meow",Key=Q)\n')
            saves=root/'Hk_project/Saved/SaveGames';saves.mkdir();(saves/'Data.sav').write_bytes(b'normal-campaign')
            first=root/'StrayCoopTests/session-1/UserData'
            prepare_settings(root,first)
            text=(settings_dir(first)/'GameUserSettings.ini').read_text()
            for value in ('MasterVolume=0.42','FullscreenMode=1','ResolutionSizeX=2560','ResolutionSizeY=1440',
                          'AutoPauseEnabled=False','FirstLaunch=False'):
                self.assertIn(value,text)
            self.assertEqual(text.count('CustomKeyMappings='),2)
            self.assertEqual((settings_dir(first)/'Input.ini').read_bytes(),(original/'Input.ini').read_bytes())
            self.assertFalse((first/'Saved/SaveGames').exists())
            self.assertEqual((original/'GameUserSettings.ini').read_text(),content)
            # A change in a test session carries into the next fresh campaign.
            (settings_dir(first)/'GameUserSettings.ini').write_text(text.replace('MasterVolume=0.42','MasterVolume=0.75'))
            remember_settings(root,first)
            second=root/'StrayCoopTests/session-2/UserData'
            prepare_settings(root,second)
            self.assertIn('MasterVolume=0.75',(settings_dir(second)/'GameUserSettings.ini').read_text())
            self.assertEqual((saves/'Data.sav').read_bytes(),b'normal-campaign')

    def test_migrates_the_latest_old_test_preferences(self):
        with tempfile.TemporaryDirectory() as tmp,patch.dict(os.environ,{'LOCALAPPDATA':tmp+'/no-appdata'}):
            root=Path(tmp)
            old=settings_dir(root/'StrayCoopTests/old/UserData');old.mkdir(parents=True)
            (old/'GameUserSettings.ini').write_text('[/Script/Hk_project.HKGameUserSettings]\nLookSensitivity=0.8\n')
            new=root/'StrayCoopTests/new/UserData'
            self.assertEqual(prepare_settings(root,new),old)
            self.assertIn('LookSensitivity=0.8',(settings_dir(new)/'GameUserSettings.ini').read_text())
            self.assertTrue((cache_dir(root)/'GameUserSettings.ini').exists())

    def test_flags_preserve_other_sections_and_utf16(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'settings.ini'
            text='[Other]\r\nFirstLaunch=KeepMe\r\n[/Script/Hk_project.HKGameUserSettings]\r\nCulture=日本語\r\n[ScalabilityGroups]\r\nsg.ShadowQuality=2\r\n'
            path.write_bytes(text.encode('utf-16'))
            coop_flags(path)
            result=path.read_bytes().decode('utf-16')
            self.assertIn('FirstLaunch=KeepMe',result)
            self.assertIn('Culture=日本語',result)
            self.assertIn('AutoPauseEnabled=False\r\nFirstLaunch=False\r\n[ScalabilityGroups]',result)
            self.assertIn('sg.ShadowQuality=2',result)

    def test_first_launch_does_not_force_video_preferences(self):
        with tempfile.TemporaryDirectory() as tmp,patch.dict(os.environ,{'LOCALAPPDATA':tmp+'/no-appdata'}):
            root=Path(tmp);profile=root/'fresh'
            self.assertIsNone(prepare_settings(root,profile))
            text=(settings_dir(profile)/'GameUserSettings.ini').read_text()
            self.assertNotIn('ResolutionSize',text)
            self.assertNotIn('FullscreenMode',text)

    def test_normal_game_cannot_replace_established_coop_preferences(self):
        with tempfile.TemporaryDirectory() as tmp,patch.dict(os.environ,{'LOCALAPPDATA':tmp+'/no-appdata'}):
            root=Path(tmp)
            cache=cache_dir(root);cache.mkdir(parents=True)
            (cache/'GameUserSettings.ini').write_text('MasterVolume=0.42\n')
            normal=root/'Hk_project/Saved/Config/WindowsNoEditor';normal.mkdir(parents=True)
            (normal/'GameUserSettings.ini').write_text('MasterVolume=0.99\n')
            os.utime(normal/'GameUserSettings.ini',(2000000000,2000000000))
            profile=root/'fresh'
            self.assertEqual(prepare_settings(root,profile),cache)
            self.assertIn('MasterVolume=0.42',(settings_dir(profile)/'GameUserSettings.ini').read_text())


if __name__=='__main__': unittest.main(verbosity=2)
