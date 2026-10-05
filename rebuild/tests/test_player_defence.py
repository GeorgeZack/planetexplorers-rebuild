"""Source player capsule recovery and native all-clip contract; no graphics."""
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

TOOLS=Path(__file__).resolve().parents[1]/'tools';sys.path.insert(0,str(TOOLS))
import export_player_defence as tool
import export_reference_hitboxes as hb
from export_reference_assets import IDENTITY
ROOT=TOOLS.parents[1]
CAPTURE=Path.home()/'.local/share/planet-explorers-bake-reference/pose-captures/female-clips-02.jsonl'
PREFAB=ROOT/'Assets/Resources/Prefab/PlayerPrefab/FemaleModel.prefab'
HAS_SOURCE=importlib.util.find_spec('UnityPy') is not None and CAPTURE.is_file() and tool.DEFAULT_RIG.is_file()


class MappingTests(unittest.TestCase):
    def fixture(self):
        first=dict(bone_names=['torso'],bone_ids=[1],inverse_binds=[IDENTITY],resource='a',mesh_id=1)
        leg=dict(bone_names=['leg'],bone_ids=[2],inverse_binds=[IDENTITY],resource='b',mesh_id=2)
        entries=[dict(source_bone_name='leg',source_bone_path='root/leg',source_capsule=dict(radius=.1,heigh=.4,axis=1,offset=dict(x=0,y=0,z=0)))]
        return dict(parts=[first,leg]),entries,dict(bones={'leg':dict(relative_path='root/leg')})

    def test_extension_preserves_indices_and_source_data(self):
        rig,entries,mapping=self.fixture();before=copy.deepcopy(rig)
        result,provenance=tool.extend_part(rig,entries,mapping)
        self.assertEqual(rig,before)
        self.assertEqual(result['parts'][0]['bone_names'],['torso','leg'])
        self.assertEqual(entries[0]['joint'],1)
        self.assertEqual(provenance['added_joints'][0]['source_mesh'],2)
        self.assertEqual(entries[0]['bind_points'],hb.local_points(entries[0]['source_capsule']))

    def test_bone_basename_cannot_bypass_exact_source_path(self):
        rig,entries,mapping=self.fixture();mapping['bones']['leg']['relative_path']='other/leg'
        with self.assertRaises(ValueError):tool.extend_part(rig,entries,mapping)
        mapping['bones'].clear()
        with self.assertRaises(ValueError):tool.extend_part(rig,entries,mapping)


@unittest.skipUnless(HAS_SOURCE,'optional source rig, closed bake and UnityPy required')
class SourceTests(unittest.TestCase):
    def test_source_capsules_keep_all_body_regions_and_damage_scales(self):
        entries,trigger=tool.source_capsules(PREFAB)
        self.assertEqual((trigger['component'],trigger['layer']),(11488762,25))
        self.assertEqual(len(entries),16)
        byname={e['name']:e for e in entries}
        self.assertEqual(byname['Head']['damage_scale'],2)
        self.assertEqual(byname['Chest']['damage_scale'],1)
        self.assertAlmostEqual(byname['LHand']['damage_scale'],.2)
        self.assertEqual({e['form'] for e in entries},{0})
        self.assertEqual({e['material'] for e in entries},{0})
        self.assertTrue(all(e['source_bone_path'].startswith('Bip01/') for e in entries))

    def test_repeatable_all_clip_output_and_native_loader(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);a=root/'a';b=root/'b';a.mkdir();b.mkdir()
            first=tool.recover(PREFAB,tool.DEFAULT_RIG,CAPTURE,tool.DEFAULT_ACTORS,a)
            tool.recover(PREFAB,tool.DEFAULT_RIG,CAPTURE,tool.DEFAULT_ACTORS,b)
            for name in ('player-defence.peac','player-defence.pehb','player-defence.json'):
                self.assertEqual((a/name).read_bytes(),(b/name).read_bytes())
            self.assertEqual(first['joints'],23);self.assertEqual(first['enabled_defence'],16)
            self.assertEqual(sum(c['frames'] for c in first['clips']),345)
            self.assertEqual(len(first['clips']),10);self.assertLess(first['maximum_point_error_m'],1e-5)
            header=hb.HEADER.unpack_from((a/'player-defence.pehb').read_bytes())
            self.assertEqual(header[2:4],(16,23))
            if not shutil.which('cc'):self.skipTest('C compiler absent')
            source=root/'validate.c';source.write_text(r'''
#include "pe_hitbox.h"
#include <assert.h>
#include <math.h>
int main(int argc,char**argv){
 assert(argc==4);char err[256];
 PeActor*a=pe_actor_load(argv[1],err,sizeof err);assert(a);
 PeHitboxes*h=pe_hitboxes_load(argv[2],a,err,sizeof err);assert(h);
 assert(pe_hitboxes_count(h)==16&&pe_actor_stats(a).joints==23&&pe_actor_stats(a).clips==10);
 unsigned poses=0;
 for(size_t c=0;c<10;c++){
  const PeActorClip*k=pe_actor_clip(a,c);assert(k&&k->terminal_at_duration);
  for(size_t f=0;f<k->frames;f++){
   double t=f+1==k->frames?k->duration:(double)f/k->fps;
   for(size_t i=0;i<16;i++){
    PeHitboxCapsule p;assert(pe_hitbox_pose(h,a,i,c,t,(PeVec3){0,0,0},0,1,&p));
    assert(isfinite(p.a.x)&&isfinite(p.a.y)&&isfinite(p.a.z)&&p.radius>0&&p.radius<1);
    poses++;
   }
  }
 }
 assert(poses==345*16);
 PeActor*original=pe_actor_load(argv[3],err,sizeof err);assert(original);
 assert(!pe_hitboxes_load(argv[2],original,err,sizeof err));
 pe_actor_free(original);pe_hitboxes_free(h);pe_actor_free(a);return 0;
}
''')
            exe=root/'validate'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(ROOT/'rebuild/src'),str(source),str(ROOT/'rebuild/src/pe_actor.c'),str(ROOT/'rebuild/src/pe_hitbox.c'),'-lm','-o',str(exe)],check=True,capture_output=True)
            subprocess.run([str(exe),str(a/'player-defence.peac'),str(a/'player-defence.pehb'),str(tool.DEFAULT_ACTORS/'player-part-0-material-0.peac')],check=True,capture_output=True)

if __name__=='__main__':unittest.main()
