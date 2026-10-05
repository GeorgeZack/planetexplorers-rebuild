#!/usr/bin/env python3
"""Validate rejected launch options without requiring a display."""
from pathlib import Path
import subprocess
import unittest
ROOT=Path(__file__).resolve().parents[2]
BINARY=ROOT/'rebuild/build/planet_explorers'
class LaunchValidation(unittest.TestCase):
    def run_case(self,args,code,needle):
        result=subprocess.run([str(BINARY),*args],cwd=ROOT,capture_output=True,text=True,timeout=10)
        self.assertEqual(result.returncode,code,result.stdout+result.stderr)
        self.assertIn(needle,result.stdout+result.stderr)
    def test_help(self):self.run_case(['--help'],0,'--terrain')
    def test_app_routes_conflict(self):self.run_case(['--main-menu','--sandbox'],2,'Conflicting application routes')
    def test_menu_load_conflict(self):self.run_case(['--main-menu','--load','unused.pesave'],2,'Conflicting application routes')
    def test_sandbox_training_conflict(self):self.run_case(['--sandbox','--training-room','unused'],2,'Conflicting application routes')
    def test_character_panel_requires_smoke(self):self.run_case(['--smoke-panel','character'],2,'requires --smoke')
    def test_building_panel_requires_smoke(self):self.run_case(["--smoke-panel","building"],2,"requires --smoke")
    def test_origin_overflow(self):
        self.run_case(['--terrain','TutorialMode/Mainland/map_x0_y0.voxelform','--origin','2147483647,0,0'],1,'outside')
    def test_origin_requires_file(self):self.run_case(['--origin','0,0,0'],2,'requires --terrain')
    def test_bad_origin(self):self.run_case(['--origin','1,2,3junk'],2,'nonnegative integers')
    def test_missing_terrain(self):self.run_case(['--terrain','/does/not/exist.voxelform'],1,'Import failed')
    def test_missing_save(self):self.run_case(['--load','/does/not/exist.pesave'],1,'Load failed')
    def test_screenshot_requires_smoke(self):self.run_case(['--screenshot','/tmp/no.png'],2,'requires --smoke')
    def test_invalid_seed(self):self.run_case(['--seed','4294967296'],2,'Invalid number')
    def test_invalid_smoke(self):self.run_case(['--smoke','0'],2,'1..100000')
    def test_missing_reference(self):self.run_case(['--reference-room','/does/not/exist.obj'],1,'Reference OBJ missing')
    def test_reference_save_conflict(self):self.run_case(['--reference-room','unused.obj','--save','unused.pesave'],2,'cannot combine')
    def test_training_missing_assets(self):self.run_case(['--training-room','/does/not/exist'],1,'Training collision load failed')
    def test_training_preview_conflict(self):self.run_case(['--training-room','unused','--reference-room','unused.obj'],2,'cannot combine')
    def test_actor_missing(self):self.run_case(['--reference-actor','/does/not/exist.peac'],1,'Actor preview')
    def test_actor_training_conflict(self):self.run_case(['--reference-actor','unused.peac','--training-room','unused'],2,'cannot combine')
    def test_actor_scene_missing(self):self.run_case(['--reference-actor-scene','/does/not/exist.peas'],1,'scene')
    def test_actor_scene_conflict(self):self.run_case(['--reference-actor-scene','unused.peas','--reference-actor','unused.peac'],2,'cannot combine')
    def test_actor_scene_smoke_bound(self):self.run_case(['--reference-actor-scene','unused.peas','--smoke','36001'],2,'1..36000')
if __name__=='__main__':unittest.main()
