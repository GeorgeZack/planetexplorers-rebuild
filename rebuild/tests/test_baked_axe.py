"""Source-bound rigid axe export validation, no Unity or graphics needed."""
import copy
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import struct
import math
import sys
import tempfile
import unittest
TOOLS=Path(__file__).resolve().parents[1]/'tools';sys.path.insert(0,str(TOOLS))
import export_baked_axe as axe
from test_baked_knife import drawn_fixture


def fixture(root):
    args,rows=drawn_fixture(root)
    visual=json.loads((args[3]/'training-knife-manifest.json').read_text())['assets']['training-knife']
    collision=json.loads(args[2].read_text())['source'];placement=visual['placement_provenance']
    mesh=(args[3]/'training-knife.obj').read_bytes();(args[3]/'safety-axe.obj').write_bytes(mesh)
    files={n:dict(bytes=(args[3]/n).stat().st_size,sha256=hashlib.sha256((args[3]/n).read_bytes()).hexdigest()) for n in ('safety-axe.obj','knife.png')}
    data=dict(format='PE-source-safety-axe-1',item_id=1527,resource='Prefab/Item/Equip/Weapon/Ax/safety_axe',
        source_equipment=dict(m_HandChangeAttr=dict(m_PutOnBone='mountMain')),source_root=placement['root_override'],
        source_mounts=collision['source_mounts'],materials=visual['materials'],files=files)
    (args[3]/'manifest.json').write_text(json.dumps(data))
    return (args[0],args[1],args[3],args[4],args[5]),rows


class BakedAxeTests(unittest.TestCase):
    def test_exact_clocks_and_native_rigid_pose(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);args,_=fixture(root);result=axe.export(*args)
            self.assertEqual(result['item_id'],1527);self.assertEqual(result['attachment'],'mountMain')
            self.assertFalse(result['activation_observed']);self.assertFalse(result['collision_authorized'])
            actor=args[-1]/result['parts'][0]['file']
            self.assertEqual(axe.clip_headers(actor.read_bytes()),axe.clip_headers((args[3].parent/'body.peac').read_bytes()))
            if not shutil.which('cc'):self.skipTest('C compiler unavailable')
            source=root/'check.c';source.write_text('''#include "pe_actor_scene.h"
#include <math.h>
int main(int argc,char**argv){if(argc!=2)return 1;char e[256];PeActorSceneManifest*s=pe_actor_scene_manifest_load(argv[1],e,sizeof e);if(!s)return 2;
const PeActor*a=pe_actor_scene_actor(s,0);PeActorVertex p;int bad=pe_actor_scene_clip_count(s)!=2;
bad|=!pe_actor_pose_vertex(a,0,.75,0,&p)||fabsf(p.position[0]-.75f)>1e-6f||fabsf(p.position[1]-2)>1e-6f;
bad|=!pe_actor_pose_vertex(a,1,.5,0,&p)||fabsf(p.position[0]-10.5f)>1e-6f;pe_actor_scene_manifest_free(s);return bad;}
''');src=TOOLS.parent/'src';binary=root/'check'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(src),str(source),str(src/'pe_actor.c'),str(src/'pe_actor_scene_manifest.c'),'-lm','-o',str(binary)],check=True)
            subprocess.run([str(binary),str(args[-1]/'recording.peas')],check=True)

    def test_wood_axe_identity_is_preserved_without_safety_mesh_substitution(self):
        with tempfile.TemporaryDirectory() as temp:
            args,_=fixture(Path(temp));path=args[2]/'manifest.json';data=json.loads(path.read_text())
            data.update(format='PE-source-wood-axe-1',item_id=26,resource='Prefab/Item/Equip/Weapon/Ax/wood_ax')
            (args[2]/'safety-axe.obj').rename(args[2]/'wood-axe.obj')
            data['files']['wood-axe.obj']=data['files'].pop('safety-axe.obj');path.write_text(json.dumps(data))
            result=axe.export(*args)
            self.assertEqual(result['item_id'],26)
            self.assertTrue(all(p['file'].startswith('wood-axe-part-') for p in result['parts']))
            self.assertTrue(all(p['resource'].endswith('/wood_ax') for p in result['parts']))
            # A changed ID alone cannot relabel a safety axe as a wood axe.
            data['format']='PE-source-safety-axe-1'
            with self.assertRaises(ValueError):axe.identity(data)

    def test_wrong_item_mount_root_and_geometry_hash_are_atomic(self):
        for field in ('item','mount','root','geometry'):
            with self.subTest(field=field),tempfile.TemporaryDirectory() as temp:
                args,_=fixture(Path(temp));path=args[2]/'manifest.json';data=json.loads(path.read_text())
                if field=='item':data['item_id']=33
                elif field=='mount':data['source_equipment']['m_HandChangeAttr']['m_PutOnBone']='mountWaist'
                elif field=='root':data['source_root']['m_LocalPosition']['x']=1
                else:(args[2]/'safety-axe.obj').write_text('changed')
                path.write_text(json.dumps(data))
                with self.assertRaises(ValueError):axe.export(*args)
                self.assertFalse(args[-1].exists())

    def test_body_clock_and_incomplete_bake_reject_without_output(self):
        for field in ('clock','incomplete','existing'):
            with self.subTest(field=field),tempfile.TemporaryDirectory() as temp:
                args,_=fixture(Path(temp))
                if field=='clock':
                    p=args[3].parent/'body.peac';data=bytearray(p.read_bytes());data[252]=ord('X');p.write_bytes(data)
                elif field=='incomplete':args[0].write_text('\n'.join(args[0].read_text().splitlines()[:-1])+'\n')
                else:args[-1].mkdir();(args[-1]/'keep').write_text('keep')
                with self.assertRaises(ValueError):axe.export(*args)
                if field=='existing':self.assertEqual((args[-1]/'keep').read_text(),'keep')
                else:self.assertFalse(args[-1].exists())

    def test_optional_actual_axe(self):
        root=TOOLS.parent/'build';assets=root/'reference-assets/safety-axe';output=root/'animator-bake/player-female/axe-all'
        if not (output/'recording.peas').exists():self.skipTest('Optional original assets unavailable')
        source=json.loads((assets/'manifest.json').read_text());self.assertEqual((source['vertices'],source['triangles']),(706,524))
        result=json.loads((output/'recording-manifest.json').read_text());self.assertEqual(len(result['parts']),1);self.assertEqual(len(result['clips']),10)
        actor=(output/result['parts'][0]['file']).read_bytes()
        body=root/'animator-bake/player-female/all';first=(body/'recording.peas').read_text().splitlines()[1].split()[0]
        self.assertEqual(axe.clip_headers(actor),axe.clip_headers((body/first).read_bytes()))
        self.assertEqual(result['attachment'],'mountMain');self.assertFalse(result['collision_authorized'])

    def test_optional_actual_wood_axe_and_exact_clocks(self):
        root=TOOLS.parent/'build/reference-assets';assets=root/'wood-axe'
        if not (assets/'manifest.json').exists():self.skipTest('Optional original wood axe unavailable')
        source=json.loads((assets/'manifest.json').read_text())
        self.assertEqual((source['item_id'],source['vertices'],source['triangles']),(26,275,228))
        self.assertEqual(source['source_equipment']['m_StaminaCost'],4)
        self.assertEqual(source['source_equipment']['m_AnimSpeed'],1)
        for tool_name,body_name,count in [('player-wood-axe','player',10),('player-wood-axe-cut','player-cut',3)]:
            output=root/tool_name;result=json.loads((output/'recording-manifest.json').read_text())
            self.assertEqual(result['item_id'],26);self.assertEqual(len(result['clips']),count)
            first=(root/body_name/'recording.peas').read_text().splitlines()[1].split()[0]
            self.assertEqual(axe.clip_headers((output/result['parts'][0]['file']).read_bytes()),
                             axe.clip_headers((root/body_name/first).read_bytes()))
            self.assertTrue(result['parts'][0]['resource'].endswith('/wood_ax'))

    def test_optional_actual_cutting_clocks_and_movement(self):
        root=TOOLS.parent/'build';output=root/'animator-bake/player-female/axe-cut-all'
        if not (output/'recording.peas').exists():self.skipTest('Optional cutting bake unavailable')
        result=json.loads((output/'recording-manifest.json').read_text())
        data=(output/result['parts'][0]['file']).read_bytes()
        body=root/'animator-bake/player-female-cut/all';first=(body/'recording.peas').read_text().splitlines()[1].split()[0]
        self.assertEqual(axe.clip_headers(data),axe.clip_headers((body/first).read_bytes()))
        _,version,nv,ni,nj,nc=struct.unpack_from('<4s5I',data)
        self.assertEqual((version,nv,nj,nc),(2,706,1,3));vertex=struct.unpack_from('<3f',data,24)
        offset=24+nv*56+ni*4+nj*48
        for wanted,frames,loop in [('Felling1',20,0),('Felling2',35,1),('Felling3',20,0)]:
            name,fps,count,duration,actual_loop=struct.unpack_from('<32sfIfI',data,offset);offset+=48
            self.assertEqual(name.split(b'\0')[0].decode(),wanted);self.assertEqual((count,actual_loop),(frames,loop))
            def posed(frame):
                matrix=struct.unpack_from('<12f',data,offset+frame*48)
                return [sum(matrix[r*4+j]*vertex[j] for j in range(3))+matrix[r*4+3] for r in range(3)]
            start,middle,end=posed(0),posed(count//2),posed(count-1)
            self.assertGreater(math.dist(start,middle),.1)
            if loop:self.assertLess(math.dist(start,end),.00001)
            offset+=count*48
        self.assertEqual(offset,len(data))

if __name__=='__main__':unittest.main()
