"""Source-bound rigid shovel export validation, no Unity or graphics needed."""
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
import export_baked_shovel as shovel
from test_baked_knife import drawn_fixture


def fixture(root):
    args,rows=drawn_fixture(root)
    visual=json.loads((args[3]/'training-knife-manifest.json').read_text())['assets']['training-knife']
    collision=json.loads(args[2].read_text())['source'];placement=visual['placement_provenance']
    mesh=(args[3]/'training-knife.obj').read_bytes();(args[3]/'wood-shovel.obj').write_bytes(mesh)
    files={n:dict(bytes=(args[3]/n).stat().st_size,sha256=hashlib.sha256((args[3]/n).read_bytes()).hexdigest()) for n in ('wood-shovel.obj','knife.png')}
    data=dict(format='PE-source-wood-shovel-1',item_id=12,resource='Prefab/Item/Equip/Weapon/Spade/wood_spade',
        source_equipment=dict(m_HandChangeAttr=dict(m_PutOnBone='mountMain',m_PutOffBone='mountBack')),source_root=placement['root_override'],
        source_mounts=collision['source_mounts'],materials=visual['materials'],files=files)
    (args[3]/'manifest.json').write_text(json.dumps(data))
    return (args[0],args[1],args[3],args[4],args[5]),rows


class BakedShovelTests(unittest.TestCase):
    def test_exact_clocks_and_native_rigid_pose(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);args,_=fixture(root);result=shovel.export(*args)
            self.assertEqual(result['item_id'],12);self.assertEqual(result['attachment'],'mountMain')
            self.assertFalse(result['activation_observed']);self.assertFalse(result['collision_authorized'])
            actor=args[-1]/result['parts'][0]['file']
            self.assertEqual(shovel.clip_headers(actor.read_bytes()),shovel.clip_headers((args[3].parent/'body.peac').read_bytes()))
            if not shutil.which('cc'):self.skipTest('C compiler unavailable')
            source=root/'check.c';source.write_text('''#include "pe_actor_scene.h"
#include <math.h>
int main(int argc,char**argv){if(argc!=2)return 1;char e[256];PeActorSceneManifest*s=pe_actor_scene_manifest_load(argv[1],e,sizeof e);if(!s)return 2;
const PeActor*a=pe_actor_scene_actor(s,0);PeActorVertex p;int bad=pe_actor_scene_clip_count(s)!=2;
bad|=!pe_actor_pose_vertex(a,0,.75,0,&p)||fabsf(p.position[0]-.75f)>1e-6f||fabsf(p.position[1]-2)>1e-6f;
bad|=!pe_actor_pose_vertex(a,1,.5,0,&p)||fabsf(p.position[0]-10.5f)>1e-6f;pe_actor_scene_manifest_free(s);return bad;}
''');src=TOOLS.parent/'src';binary=root/'check'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(src),str(source),str(src/'pe_actor.c'),str(src/'pe_actor_scene_manifest.c'),'-lm','-o',str(binary)],check=True)
            subprocess.run([str(binary),str(args[-1]/'recording.peas')],check=True)

    def test_wrong_item_mount_root_and_geometry_hash_are_atomic(self):
        for field in ('item','mount','root','geometry'):
            with self.subTest(field=field),tempfile.TemporaryDirectory() as temp:
                args,_=fixture(Path(temp));path=args[2]/'manifest.json';data=json.loads(path.read_text())
                if field=='item':data['item_id']=33
                elif field=='mount':data['source_equipment']['m_HandChangeAttr']['m_PutOnBone']='mountWaist'
                elif field=='root':data['source_root']['m_LocalPosition']['x']=1
                else:(args[2]/'wood-shovel.obj').write_text('changed')
                path.write_text(json.dumps(data))
                with self.assertRaises(ValueError):shovel.export(*args)
                self.assertFalse(args[-1].exists())

    def test_body_clock_and_incomplete_bake_reject_without_output(self):
        for field in ('clock','incomplete','existing'):
            with self.subTest(field=field),tempfile.TemporaryDirectory() as temp:
                args,_=fixture(Path(temp))
                if field=='clock':
                    p=args[3].parent/'body.peac';data=bytearray(p.read_bytes());data[252]=ord('X');p.write_bytes(data)
                elif field=='incomplete':args[0].write_text('\n'.join(args[0].read_text().splitlines()[:-1])+'\n')
                else:args[-1].mkdir();(args[-1]/'keep').write_text('keep')
                with self.assertRaises(ValueError):shovel.export(*args)
                if field=='existing':self.assertEqual((args[-1]/'keep').read_text(),'keep')
                else:self.assertFalse(args[-1].exists())

    def test_optional_actual_shovel(self):
        root=TOOLS.parent/'build';assets=root/'reference-assets/wood-shovel';output=root/'animator-bake/player-female/shovel-all'
        if not (output/'recording.peas').exists():self.skipTest('Optional original assets unavailable')
        source=json.loads((assets/'manifest.json').read_text());self.assertEqual((source['vertices'],source['triangles']),(211,140))
        result=json.loads((output/'recording-manifest.json').read_text());self.assertEqual(len(result['parts']),1);self.assertEqual(len(result['clips']),10)
        actor=(output/result['parts'][0]['file']).read_bytes()
        body=root/'animator-bake/player-female/all';first=(body/'recording.peas').read_text().splitlines()[1].split()[0]
        self.assertEqual(shovel.clip_headers(actor),shovel.clip_headers((body/first).read_bytes()))
        self.assertEqual(result['attachment'],'mountMain');self.assertFalse(result['collision_authorized'])

    def test_stowed_mount_back_and_invalid_mode(self):
        with tempfile.TemporaryDirectory() as temp:
            args,rows=fixture(Path(temp));data=json.loads((args[2]/'manifest.json').read_text())
            data['source_mounts'][0]['mount_chains'][0][-1]['name']='mountBack'
            (args[2]/'manifest.json').write_text(json.dumps(data))
            rows[1]['nodes'][1]['path']='mountBack'
            args[0].write_text(''.join(json.dumps(row)+'\n' for row in rows))
            with self.assertRaises(ValueError):shovel.export(*args,mode='unsupported')
            self.assertFalse(args[-1].exists())
            result=shovel.export(*args,mode='stowed')
            self.assertEqual(result['attachment'],'mountBack')
            self.assertEqual(result['attachment_mode'],'stowed')
            actor=args[-1]/result['parts'][0]['file']
            self.assertEqual(shovel.clip_headers(actor.read_bytes()),shovel.clip_headers((args[3].parent/'body.peac').read_bytes()))

    def test_optional_actual_dig_scenes_share_source_clocks(self):
        root=TOOLS.parent/'build/animator-bake';body=root/'player-female-dig/all'
        paths=[root/'player-female'/name for name in ('shovel-dig-all','shovel-stowed-dig-all')]
        if not all((p/'recording.peas').exists() for p in [body,*paths]):self.skipTest('Optional shovel bake unavailable')
        first=(body/'recording.peas').read_text().splitlines()[1].split()[0]
        expected=shovel.clip_headers((body/first).read_bytes());origins=[]
        for output,mount in zip(paths,('mountMain','mountBack')):
            result=json.loads((output/'recording-manifest.json').read_text())
            self.assertEqual(result['attachment'],mount)
            data=(output/result['parts'][0]['file']).read_bytes()
            self.assertEqual(shovel.clip_headers(data),expected)
            _,version,nv,ni,nj,nc=struct.unpack_from('<4s5I',data)
            self.assertEqual((version,nv,nj,nc),(2,211,1,3))
            offset=24+nv*56+ni*4+48
            for name,frames,loop in [('Spade1',22,0),('Spade2',42,1),('Spade3',27,0)]:
                label,fps,count,duration,actual_loop=struct.unpack_from('<32sfIfI',data,offset);offset+=48
                self.assertEqual(label.rstrip(b'\0').decode(),name)
                self.assertEqual((count,actual_loop),(frames,loop))
                start=struct.unpack_from('<12f',data,offset)
                middle=struct.unpack_from('<12f',data,offset+(count//2)*48)
                self.assertTrue(all(math.isfinite(x) for x in (*start,*middle)))
                self.assertGreater(max(abs(a-b) for a,b in zip(start,middle)),.01)
                if name=='Spade1':origins.append([start[i] for i in (3,7,11)])
                offset+=count*48
            self.assertEqual(offset,len(data))
        self.assertGreater(math.dist(*origins),.1)

if __name__=='__main__':unittest.main()
