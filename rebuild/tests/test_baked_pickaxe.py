"""Exact pickaxe identity, source mounts and shared body clocks; no graphics."""
import copy
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
TOOLS=Path(__file__).resolve().parents[1]/'tools';sys.path.insert(0,str(TOOLS))
import export_baked_pickaxe as pickaxe
import export_reference_pickaxe as source
from test_baked_shovel import fixture as shovel_fixture


def fixture(root):
    args,rows=shovel_fixture(root)
    assets=args[2];data=json.loads((assets/'manifest.json').read_text())
    (assets/'wood-shovel.obj').rename(assets/'wood-pickaxe.obj')
    data.update(format='PE-source-wood-pickaxe-1',item_id=19,resource=source.RESOURCE)
    data['files']['wood-pickaxe.obj']=data['files'].pop('wood-shovel.obj')
    (assets/'manifest.json').write_text(json.dumps(data))
    return args,rows


class BakedPickaxeTests(unittest.TestCase):
    def test_source_action_override_rejects_shovel_values(self):
        e=dict(m_SkillID=20110002,m_AnimName='Mine',m_AnimSpeed=1.,m_StaminaCost=4.,
               m_AnimDownThreshold=[50.,20.],m_DigMask=27,
               m_HandChangeAttr=dict(m_PutOnBone='mountMain',m_PutOffBone='mountBack',
                                    m_PutOnAnim='MinePutOn',m_PutOffAnim='MinePutOff'))
        source.validate_equipment(e)
        for key,value in [('m_SkillID',20110001),('m_AnimName','Spade'),('m_StaminaCost',5),('m_AnimDownThreshold',[40,20])]:
            with self.subTest(key=key),self.assertRaises(ValueError):
                source.validate_equipment(dict(e,**{key:value}))
        bad=copy.deepcopy(e);bad['m_HandChangeAttr']['m_PutOffBone']='mountWaist'
        with self.assertRaises(ValueError):source.validate_equipment(bad)

    def test_exact_body_clocks_and_native_mount_pose(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);args,_=fixture(root);result=pickaxe.export(*args)
            self.assertEqual((result['item_id'],result['attachment'],result['attachment_mode']),(19,'mountMain','drawn'))
            self.assertFalse(result['collision_authorized']);self.assertFalse(result['activation_observed'])
            data=(args[-1]/result['parts'][0]['file']).read_bytes()
            self.assertEqual(pickaxe.clip_headers(data),pickaxe.clip_headers((args[3].parent/'body.peac').read_bytes()))
            self.assertIn('wood-pickaxe-part-',result['parts'][0]['file'])
            if not shutil.which('cc'):self.skipTest('C compiler unavailable')
            checker=root/'check.c';checker.write_text('''#include "pe_actor_scene.h"
#include <math.h>
int main(int argc,char**argv){if(argc!=2)return 1;char error[256];
PeActorSceneManifest*s=pe_actor_scene_manifest_load(argv[1],error,sizeof error);if(!s)return 2;
const PeActor*a=pe_actor_scene_actor(s,0);PeActorVertex p;int bad=pe_actor_scene_clip_count(s)!=2;
bad|=!pe_actor_pose_vertex(a,0,.75,0,&p)||fabsf(p.position[0]-.75f)>1e-6f||fabsf(p.position[1]-2)>1e-6f;
bad|=!pe_actor_pose_vertex(a,1,.5,0,&p)||fabsf(p.position[0]-10.5f)>1e-6f;
pe_actor_scene_manifest_free(s);return bad;}
''');src=TOOLS.parent/'src';binary=root/'check'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(src),str(checker),str(src/'pe_actor.c'),str(src/'pe_actor_scene_manifest.c'),'-lm','-o',str(binary)],check=True)
            subprocess.run([str(binary),str(args[-1]/'recording.peas')],check=True)

    def test_wrong_item_resource_mount_geometry_or_body_clock_is_atomic(self):
        for field in ('item','resource','mount','root','geometry','clock','incomplete'):
            with self.subTest(field=field),tempfile.TemporaryDirectory() as temp:
                args,_=fixture(Path(temp));path=args[2]/'manifest.json';data=json.loads(path.read_text())
                if field=='item':data['item_id']=12
                elif field=='resource':data['resource']='Prefab/Item/Equip/Weapon/Spade/wood_spade'
                elif field=='mount':data['source_equipment']['m_HandChangeAttr']['m_PutOnBone']='mountWaist'
                elif field=='root':data['source_root']['m_LocalPosition']['x']=1
                elif field=='geometry':(args[2]/'wood-pickaxe.obj').write_text('changed')
                elif field=='clock':
                    p=args[3].parent/'body.peac';b=bytearray(p.read_bytes());b[252]=ord('X');p.write_bytes(b)
                elif field=='incomplete':args[0].write_text('\n'.join(args[0].read_text().splitlines()[:-1])+'\n')
                path.write_text(json.dumps(data))
                with self.assertRaises(ValueError):pickaxe.export(*args)
                self.assertFalse(args[-1].exists())

    def test_stowed_mount_and_existing_destination_preserved(self):
        with tempfile.TemporaryDirectory() as temp:
            args,rows=fixture(Path(temp));data=json.loads((args[2]/'manifest.json').read_text())
            data['source_mounts'][0]['mount_chains'][0][-1]['name']='mountBack'
            (args[2]/'manifest.json').write_text(json.dumps(data));rows[1]['nodes'][1]['path']='mountBack'
            args[0].write_text(''.join(json.dumps(row)+'\n' for row in rows))
            with self.assertRaises(ValueError):pickaxe.export(*args,mode='invalid')
            result=pickaxe.export(*args,mode='stowed')
            self.assertEqual(result['attachment'],'mountBack')
            manifest=(args[-1]/'recording-manifest.json').read_bytes()
            with self.assertRaises(ValueError):pickaxe.export(*args,mode='stowed')
            self.assertEqual((args[-1]/'recording-manifest.json').read_bytes(),manifest)

    def test_optional_recovered_pickaxe_has_exact_source_identity(self):
        assets=TOOLS.parent/'build/reference-assets/wood-pickaxe'
        if not (assets/'manifest.json').exists():self.skipTest('Optional pickaxe assets unavailable')
        data=json.loads((assets/'manifest.json').read_text())
        self.assertEqual((data['item_id'],data['installed_mesh'],data['installed_root']),(19,6342,18702))
        self.assertEqual(data['source_sha256'],hashlib.sha256(source.PREFAB.read_bytes()).hexdigest())
        source.validate_equipment(data['source_equipment'])
        for name,expected in data['files'].items():
            content=(assets/name).read_bytes()
            self.assertEqual(len(content),expected['bytes'])
            self.assertEqual(hashlib.sha256(content).hexdigest(),expected['sha256'])

    def test_optional_actual_attachment_groups_share_body_clocks(self):
        root=TOOLS.parent/'build/reference-assets'
        groups=[('player-pickaxe','player','mountMain'),('player-pickaxe-stowed','player','mountBack'),
                ('player-pickaxe-mine','player-mine','mountMain')]
        tested=0
        for tool,body,mount in groups:
            output=root/tool;body=root/body
            if not (output/'recording.peas').exists():
                output=TOOLS.parent/'build/pickaxe-staging'/tool
            if not (output/'recording.peas').exists():continue
            self.assertTrue((body/'recording.peas').exists())
            result=json.loads((output/'recording-manifest.json').read_text())
            self.assertEqual((result['item_id'],result['attachment']),(19,mount))
            first=(body/'recording.peas').read_text().splitlines()[1].split()[0]
            expected=pickaxe.clip_headers((body/first).read_bytes())
            if expected[-1][:32].rstrip(b'\0')==b'RunSwordAttack':
                expected=expected[:-1] # Only the appended Knife pose is excluded.
            for part in result['parts']:
                self.assertEqual(pickaxe.clip_headers((output/part['file']).read_bytes()),expected)
            tested+=1
        if not tested:self.skipTest('Optional actual attachment groups unavailable')

    def test_optional_actual_locomotion_export_repeats_identical_actor_bytes(self):
        root=TOOLS.parent/'build';assets=root/'reference-assets/wood-pickaxe'
        original=root/'reference-assets/player-pickaxe'
        if not (original/'recording.peas').exists():original=root/'pickaxe-staging/player-pickaxe'
        if not (original/'recording-manifest.json').exists():self.skipTest('Optional pickaxe bake unavailable')
        manifest=json.loads((original/'recording-manifest.json').read_text())
        inputs=[Path(row['path']) for row in manifest['sources']]
        if not all(p.exists() for p in inputs):self.skipTest('Optional source capture/rig unavailable')
        with tempfile.TemporaryDirectory() as temp:
            output=Path(temp)/'repeat'
            result=pickaxe.export(inputs[0],inputs[1],assets,inputs[3],output)
            self.assertEqual([c['clip'] for c in result['clips']],[c['clip'] for c in manifest['clips']])
            for part in result['parts']:
                for key in ('file','texture'):
                    self.assertEqual((output/part[key]).read_bytes(),(original/part[key]).read_bytes())


if __name__=='__main__':unittest.main()
