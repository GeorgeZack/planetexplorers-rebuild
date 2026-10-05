"""Synthetic attachment exports plus the real C loader; no Unity/graphics."""
import copy
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]/'tools'; sys.path.insert(0, str(TOOLS))
import export_baked_knife as knife
from export_reference_knife_collision import rigid_peac
from test_reference_bake_analysis import bake_fixture
from test_reference_pose_analysis import matrix, write


def fixture(root):
    rows, rig = bake_fixture(); rows[1]['nodes'][1]['path'] = 'mountWaist'
    first = copy.deepcopy(rows[2:-1]); rows = rows[:-1]
    # Different second clip, including loop and pose, preserves ordered metadata.
    first[0].update(index=1, name='NormalIdle', sourcePathId=180, runtimeId=9, isLooping=True)
    first[-1]['clip'] = 1
    for row in first[1:-1]:
        row.update(clip=1, frame=row['frame']+10, realtime=row['realtime']+1)
        row['local'][1][3] += 10; row['world'][1][3] += 10
    rows[1]['plannedSamples'] = 6; rows += first+[dict(type='end', reason='complete', samples=6)]
    bake_path = root/'bake.jsonl'; write(bake_path, rows)
    rig_path = root/'rig.json'; rig_path.write_text(json.dumps(rig))
    assets = root/'assets'; assets.mkdir()
    mesh = b'v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvn 0 0 1\nusemtl knife\nf 1/1/1 2/1/1 3/1/1\n'
    png = knife.pose.solid_png(dict(r=1,g=1,b=1,a=1))
    files = {}
    for name, data in [('training-knife.obj', mesh), ('knife.png', png)]:
        (assets/name).write_bytes(data); files[name] = dict(bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
    attachment = dict(m_PutOffBone='mountWaist')
    root_transform = dict(m_LocalPosition=dict(x=0,y=0,z=0), m_LocalRotation=dict(x=0,y=0,z=0,w=1), m_LocalScale=dict(x=1,y=1,z=1))
    visual = dict(files=files, assets={'training-knife':dict(materials=[dict(obj_symbol='knife',path_id=1,diffuse_file='knife.png')],
        placement_provenance=dict(item_id=33,attachment=attachment,root_override=root_transform))})
    (assets/'training-knife-manifest.json').write_text(json.dumps(visual))
    mount = dict(name='mountWaist',position=dict(x=0,y=2,z=0),rotation=dict(x=0,y=0,z=0,w=1),scale=dict(x=1,y=1,z=1))
    collision = dict(source=dict(item_id=33,attachment=attachment,source_mounts=[dict(mount_chains=[[
        dict(name='MaleModel'),dict(name='Model'),mount]])]))
    collision_path = root/'collision.json'; collision_path.write_text(json.dumps(collision))
    body = root/'body'; body.mkdir(); (body/'recording.peas').write_text('PEAS1\nbody.peac knife.png\n'); (body/'knife.png').write_bytes(png)
    geometry, indices = knife.obj_parts(mesh.decode())['knife']; binaries = []
    for name, loop in [('Attack6',0), ('NormalIdle',1)]:
        data = rigid_peac(geometry,indices,[knife.IDENTITY]*3,2,.75)
        offset = 24+len(geometry)*56+len(indices)*4+48
        struct.pack_into('<32s',data,offset,name.encode());struct.pack_into('<I',data,offset+44,loop)
        binaries.append(data)
    combined = bytearray(binaries[0]);struct.pack_into('<I',combined,20,2);combined.extend(binaries[1][offset:])
    (body/'body.peac').write_bytes(combined)
    return (bake_path,rig_path,collision_path,assets,body/'recording.peas',root/'out'), rows


def drawn_fixture(root):
    args, rows = fixture(root)
    rows[1]['nodes'][1]['path'] = 'mountMain'; write(args[0], rows)
    collision = json.loads(args[2].read_text()); source = collision['source']
    source['attachment']['m_PutOnBone'] = 'mountMain'
    source['source_mounts'][0]['mount_chains'][0][-1]['name'] = 'mountMain'
    transform = dict(m_Father=dict(m_FileID=0,m_PathID=41),m_GameObject=dict(m_FileID=0,m_PathID=8),
        m_LocalPosition=dict(x=0,y=0,z=0),m_LocalRotation=dict(x=0,y=0,z=0,w=1),m_LocalScale=dict(x=1,y=1,z=1))
    source.update(source_transform=42,source_transform_fields=transform,attack_form=0,attack_layer_mask=34154496,
        capsule=dict(trans=dict(m_FileID=0,m_PathID=42),radius=.3,heigh=1.5,axis=2,offset=dict(x=0,y=0,z=-.1)))
    source['local_points'] = knife.local_points(source['capsule']);args[2].write_text(json.dumps(collision))
    path=args[3]/'training-knife-manifest.json';visual=json.loads(path.read_text());placement=visual['assets']['training-knife']['placement_provenance']
    placement['attachment']['m_PutOnBone']='mountMain';placement['prefab_transforms']=[transform]
    placement['root_override']['m_Children']=[dict(m_FileID=0,m_PathID=42)];path.write_text(json.dumps(visual))
    return args, rows


class BakedKnifeTests(unittest.TestCase):
    def test_ordered_clocks_rigid_pose_terminal_and_native_loader(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d); args,_=fixture(root); result=knife.export(*args)
            self.assertEqual(result['attachment'],'mountWaist');self.assertFalse(result['collision_authorized'])
            self.assertEqual([c['clip'] for c in result['clips']],['Attack6','NormalIdle'])
            self.assertEqual(result['parts'][0]['file'],'knife-stowed-part-0-material-0.peac')
            data=(args[-1]/result['parts'][0]['file']).read_bytes()
            self.assertEqual(knife.clip_headers(data),knife.clip_headers((args[4].parent/'body.peac').read_bytes()))
            if not shutil.which('cc'): self.skipTest('C compiler unavailable')
            source=root/'check.c';source.write_text('''#include "pe_actor_scene.h"
#include <math.h>
int main(int argc,char**argv){char e[256];if(argc!=2)return 1;
PeActorSceneManifest*s=pe_actor_scene_manifest_load(argv[1],e,sizeof e);if(!s)return 2;
const PeActor*a=pe_actor_scene_actor(s,0);PeActorVertex p;
int bad=pe_actor_scene_clip_count(s)!=2||pe_actor_scene_find_clip(s,"NormalIdle")!=1;
bad|=!pe_actor_scene_clip_at(s,1)->terminal_at_duration;
bad|=!pe_actor_pose_vertex(a,0,.75,0,&p)||fabsf(p.position[0]-.75f)>1e-6f||fabsf(p.position[1]-2)>1e-6f;
bad|=!pe_actor_pose_vertex(a,1,.5,0,&p)||fabsf(p.position[0]-10.5f)>1e-6f;
pe_actor_scene_manifest_free(s);return bad;}
''')
            src=TOOLS.parent/'src';binary=root/'check'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(src),str(source),str(src/'pe_actor.c'),str(src/'pe_actor_scene_manifest.c'),'-lm','-o',str(binary)],check=True)
            subprocess.run([str(binary),str(args[-1]/'recording.peas')],check=True)

    def test_drawn_capsule_native_pose_and_exact_geometry_fingerprint(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);args,_=drawn_fixture(root);result=knife.export(*args,attachment='drawn')
            self.assertEqual(result['attachment_mode'],'drawn');self.assertEqual(result['attachment'],'mountMain')
            self.assertEqual(result['hitbox']['actor'],result['parts'][0]['file'])
            self.assertFalse(result['activation_observed']);self.assertFalse(result['collision_authorized'])
            if not shutil.which('cc'):self.skipTest('C compiler unavailable')
            original=args[-1]/result['parts'][0]['file'];wrong=root/'wrong.peac';data=bytearray(original.read_bytes())
            struct.pack_into('<f',data,24,1);wrong.write_bytes(data)
            source=root/'check.c';source.write_text('''#include "pe_hitbox.h"
#include <math.h>
int main(int argc,char**argv){if(argc!=4)return 1;char e[256];PeActor*a=pe_actor_load(argv[1],e,sizeof e);if(!a)return 2;
PeHitboxes*b=pe_hitboxes_load(argv[2],a,e,sizeof e);if(!b)return 3;
const PeHitboxDefinition*d=pe_hitboxes_get(b,0);int bad=pe_hitboxes_count(b)!=1||d->kind!=PE_HITBOX_ATTACK||d->group!=PE_HITBOX_RIGHT||d->joint!=0||d->form!=0;
PeHitboxCapsule c;bad|=!pe_hitbox_pose(b,a,0,0,.75,(PeVec3){0},0,1,&c);
bad|=fabsf(c.a.x-.75f)>1e-6f||fabsf(c.a.y-2)>1e-6f||fabsf(c.a.z-.2f)>1e-6f||fabsf(c.b.z-1.1f)>1e-6f||fabsf(c.radius-.3f)>1e-6f;
bad|=!pe_hitbox_pose(b,a,0,0,.75,(PeVec3){10,20,30},90,2,&c);
bad|=fabsf(c.a.x-10.4f)>1e-5f||fabsf(c.a.y-24)>1e-5f||fabsf(c.a.z-28.5f)>1e-5f||fabsf(c.radius-.6f)>1e-6f;
PeActor*wrong=pe_actor_load(argv[3],e,sizeof e);if(!wrong)return 4;PeHitboxes*rejected=pe_hitboxes_load(argv[2],wrong,e,sizeof e);bad|=rejected!=0;
pe_hitboxes_free(rejected);pe_actor_free(wrong);pe_hitboxes_free(b);pe_actor_free(a);return bad;}
''')
            src=TOOLS.parent/'src';binary=root/'check'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(src),str(source),str(src/'pe_actor.c'),str(src/'pe_hitbox.c'),'-lm','-o',str(binary)],check=True)
            subprocess.run([str(binary),str(original),str(args[-1]/result['hitbox']['file']),str(wrong)],check=True)

    def test_drawn_requires_exact_source_capsule_and_parent(self):
        for changed in ('axis','radius','points','transform','parent','form'):
            with self.subTest(changed=changed), tempfile.TemporaryDirectory() as d:
                args,_=drawn_fixture(Path(d));data=json.loads(args[2].read_text());source=data['source']
                if changed=='axis':source['capsule']['axis']=1
                if changed=='radius':source['capsule']['radius']=.5
                if changed=='points':source['local_points'][0][2]=9
                if changed=='transform':source['source_transform']=99
                if changed=='parent':source['source_transform_fields']['m_Father']['m_PathID']=99
                if changed=='form':source['attack_form']=1
                args[2].write_text(json.dumps(data))
                with self.assertRaises(ValueError):knife.export(*args,attachment='drawn')
                self.assertFalse(args[-1].exists())

    def test_incomplete_or_wrong_mount_rejected_without_output(self):
        for changed in ('incomplete','path','local'):
            with self.subTest(changed=changed), tempfile.TemporaryDirectory() as d:
                args,rows=fixture(Path(d))
                if changed=='incomplete':rows=rows[:-1]
                elif changed=='path':rows[1]['nodes'][1]['path']='wrongMount'
                else:rows[1]['nodes'][1]['initialLocal'][7]=3
                write(args[0],rows)
                with self.assertRaises(ValueError):knife.export(*args)
                self.assertFalse(args[-1].exists())

    def test_source_hash_body_clock_and_existing_destination(self):
        for changed in ('hash','clock','existing'):
            with self.subTest(changed=changed),tempfile.TemporaryDirectory() as d:
                args,_=fixture(Path(d))
                if changed=='hash':(args[3]/'knife.png').write_bytes(b'changed')
                elif changed=='clock':
                    actor=args[4].parent/'body.peac';data=bytearray(actor.read_bytes());data[252]=ord('X');actor.write_bytes(data)
                else:args[-1].mkdir();(args[-1]/'keep').write_text('untouched')
                with self.assertRaises(ValueError):knife.export(*args)
                if changed=='existing':self.assertEqual((args[-1]/'keep').read_text(),'untouched')
                else:self.assertFalse(args[-1].exists())

    def test_ambiguous_source_mount_and_nonidentity_knife_root(self):
        for changed in ('ambiguous','root'):
            with self.subTest(changed=changed),tempfile.TemporaryDirectory() as d:
                args,_=fixture(Path(d))
                if changed=='ambiguous':
                    path=args[2];data=json.loads(path.read_text());data['source']['source_mounts']*=2
                else:
                    path=args[3]/'training-knife-manifest.json';data=json.loads(path.read_text());data['assets']['training-knife']['placement_provenance']['root_override']['m_LocalPosition']['x']=1
                path.write_text(json.dumps(data))
                with self.assertRaises(ValueError):knife.export(*args)
                self.assertFalse(args[-1].exists())

if __name__=='__main__':unittest.main()
