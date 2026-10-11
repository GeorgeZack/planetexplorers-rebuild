#!/usr/bin/env python3
"""Attachment/capsule contracts and optional recovered asset loader checks."""
import copy
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from read_reference_adventure import source_path
import export_starter_pair_assets as exporter
from export_reference_actor import inverse_affine


class PairAssetTests(unittest.TestCase):
    def test_output_cannot_replace_or_contain_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);source=root/'source';source.mkdir();p=source/'input';p.write_bytes(b'preserved')
            for output in (root,source,source/'output'):
                with self.subTest(output=output),self.assertRaises(ValueError):exporter.fresh_output(output,[source,p])
            self.assertEqual(exporter.fresh_output(root/'new',[source]),root/'new')
            self.assertEqual(p.read_bytes(),b'preserved')

    def test_rigid_parts_keep_mesh_transform_and_named_bone(self):
        visual=dict(resource='modern/shield',materials=[dict(obj_symbol='mat',path_id=1,diffuse_file='source.png')])
        mesh='v -.191 -.052 .006\nv 0 1 0\nv 0 0 1\nvt 0 0\nvn 0 0 1\nusemtl mat\nf 1/1/1 2/1/1 3/1/1\n'
        rig=exporter.rigid_rig(dict(sex='female',avatar={}),visual,mesh,'Bone L Forearm','Bip01/forearm/Bone L Forearm')
        p=rig['parts'][0];self.assertEqual(p['positions'][0],[-.191,-.052,.006])
        self.assertEqual(p['bone_names'],['Bone L Forearm']);self.assertEqual(p['inverse_binds'],[exporter.IDENTITY])
        self.assertTrue(all(s['weight[0]']==1 and s['boneIndex[0]']==0 for s in p['skin']))
        with self.assertRaises(ValueError):exporter.rigid_rig(dict(sex='female',avatar={}),visual,mesh.replace('1/1/1','99/1/1'),'mountMain','hand')

    def mount_fixture(self,name):
        path='Bip01/'+name;flat=[v for row in exporter.IDENTITY for v in row]
        report=dict(rig_mapping={'bones':{name:dict(index=0,relative_path=path)}},skeleton=dict(nodes=[dict(initialLocal=flat)]))
        chain=[dict(name=name,position=dict(x=0.,y=0.,z=0.),rotation=dict(x=0.,y=0.,z=0.,w=1.),scale=dict(x=1.,y=1.,z=1.))]
        player=dict(parts=[dict(bone_names=[name])],avatar=dict(m_TOS=[(123,path)],m_Avatar={'m_SkeletonNameIDArray':[zlib.crc32(name.encode())],
                                                                                       'm_AvatarSkeleton':{'data':{'m_ID':[123]}}}))
        return report,chain,path,player

    def test_animated_forearm_may_differ_from_bind_without_substitution(self):
        report,chain,path,player=self.mount_fixture('Bone L Forearm')
        # A legal animated rotation differs from arbitrary initial/bind pose.
        report['skeleton']['nodes'][0]['initialLocal']=[1.,0.,0.,0.,0.,0.,-1.,0.,0.,1.,0.,0.,0.,0.,0.,1.]
        before=copy.deepcopy(report)
        result=exporter.validate_mount(report,'Bone L Forearm',chain,path,player)
        self.assertEqual(result['kind'],'animated-avatar-skin-joint');self.assertEqual(report,before)
        self.assertEqual(result['initial_vs_prefab_bind_matrix_error'],1.)
        for change in ('skin','avatar','path'):
            p=copy.deepcopy(player);r=copy.deepcopy(report)
            if change=='skin':p['parts'][0]['bone_names']=[]
            if change=='avatar':p['avatar']['m_TOS']=[(123,'wrong')]
            if change=='path':r['rig_mapping']['bones']['Bone L Forearm']['relative_path']='wrong'
            with self.subTest(change=change),self.assertRaises(ValueError):exporter.validate_mount(r,'Bone L Forearm',chain,path,p)

    def test_unanimated_back_marker_must_still_match_source_local(self):
        report,chain,path,player=self.mount_fixture('mountBack')
        self.assertEqual(exporter.validate_mount(report,'mountBack',chain,path,player)['kind'],'unanimated-mount-marker')
        report['skeleton']['nodes'][0]['initialLocal'][3]=.1
        with self.assertRaises(ValueError):exporter.validate_mount(report,'mountBack',chain,path,player)

    def test_source_material_slots_cannot_silently_drop_tint_or_uv_transform(self):
        tree=dict(m_SavedProperties={'m_Colors':[('_Color',dict(r=1.,g=1.,b=1.,a=1.))],
                                    'm_TexEnvs':[('_MainTex',dict(m_Texture={'m_FileID':0,'m_PathID':1},m_Scale={'x':1.,'y':1.},m_Offset={'x':0.,'y':0.}))]})
        exporter.validate_material(tree)
        for kind in ('tint','scale','missing'):
            changed=copy.deepcopy(tree)
            if kind=='tint':changed['m_SavedProperties']['m_Colors'][0][1]['r']=.5
            if kind=='scale':changed['m_SavedProperties']['m_TexEnvs'][0][1]['m_Scale']['x']=2
            if kind=='missing':changed['m_SavedProperties']['m_TexEnvs']=[]
            with self.subTest(kind=kind),self.assertRaises(ValueError):exporter.validate_material(changed)

    def test_capture_name_id_plan_and_loop_contract(self):
        path=Path('/capture.jsonl');clip=dict(name='RunSwordAttack',sourcePathId=408,isLooping=False,length=1.8,sourceFrameRate=30)
        report=dict(skeleton=dict(gender='Female',avatar='FemaleAvatar'),header=dict(sampleRate=30),clips=[clip])
        launch=dict(schema='pe-animator-bake-launch-1',capture=str(path),plan=dict(avatar='FemaleAvatar',gender='Female',sample_rate=30,
                    clips=[dict(name='RunSwordAttack',path_id=408,loop=False,length=1.8,frame_rate=30)]))
        exporter.validate_capture(report,launch,path,('RunSwordAttack',))
        for kind in ('identity','clock','duplicate','loop','wrong-group'):
            r=copy.deepcopy(report);l=copy.deepcopy(launch);names=('RunSwordAttack',)
            if kind=='identity':r['clips'][0]['sourcePathId']=419
            if kind=='clock':l['plan']['sample_rate']=60
            if kind=='duplicate':r['clips'].append(copy.deepcopy(clip))
            if kind=='loop':l['plan']['clips'][0]['loop']=True
            if kind=='wrong-group':names=tuple(exporter.SWORD_CLIPS)
            with self.subTest(kind=kind),self.assertRaises(ValueError):exporter.validate_capture(r,l,path,names)

    def test_backward_capture_requires_exact_pair_and_source_ids(self):
        capture=Path('/backward.jsonl');clips=[dict(name=n,sourcePathId=p,isLooping=True,length=.8,sourceFrameRate=30) for n,p in exporter.SWORD_BACKWARD_CLIPS.items()]
        report=dict(skeleton=dict(gender='Female',avatar='FemaleAvatar'),header=dict(sampleRate=30),clips=clips)
        launch=dict(schema='pe-animator-bake-launch-1',capture=str(capture),plan=dict(avatar='FemaleAvatar',gender='Female',sample_rate=30,
            clips=[dict(name=c['name'],path_id=c['sourcePathId'],loop=True,length=.8,frame_rate=30) for c in clips]))
        exporter.validate_capture(report,launch,capture,tuple(exporter.SWORD_BACKWARD_CLIPS))
        for kind in ('normal','order','missing'):
            r=copy.deepcopy(report)
            if kind=='normal':r['clips'][0]['sourcePathId']=547
            if kind=='order':r['clips'].reverse()
            if kind=='missing':r['clips'].pop()
            with self.subTest(kind=kind),self.assertRaises(ValueError):exporter.validate_capture(r,launch,capture,tuple(exporter.SWORD_BACKWARD_CLIPS))

    def stage(self):
        stage=Path(os.environ.get('PE_STARTER_PAIR_STAGE',str(exporter.ROOT/'rebuild/build/starter-pair-assets-01')))
        if not (stage/'starter-pair-manifest.json').is_file():self.skipTest('Fresh recovered starter-pair stage unavailable')
        return stage

    def test_recovered_stage_hashes_and_exact21_body_clocks(self):
        stage=self.stage();manifest=json.loads((stage/'starter-pair-manifest.json').read_text())
        self.assertFalse(manifest['installed']);self.assertFalse(manifest['shield_block_pose_recovered'])
        self.assertEqual(manifest['groups'],list(exporter.GROUPS))
        actual=exporter.file_hashes(stage);actual.pop('starter-pair-manifest.json')
        self.assertEqual(actual,manifest['files'])
        body=source_path(next(p for p in manifest['sources'] if p['path'].endswith('recording.peas')))
        exporter.validate_stage(stage,body,manifest['clips'])
        visual=json.loads((stage/'visual/visual-manifest.json').read_text())
        self.assertEqual(visual['items']['sword']['component_class'],'PeSword')
        self.assertEqual(visual['items']['shield']['component_class'],'PESheild')
        self.assertEqual(visual['items']['sword']['mesh_id'],6335);self.assertEqual(visual['items']['shield']['mesh_id'],6440)
        for group in exporter.GROUPS:
            m=json.loads((stage/group/'recording-manifest.json').read_text())
            self.assertEqual(m['attachment'],exporter.GROUPS[group][1]);self.assertFalse(m['collision_authorized'])
        hitbox=(stage/'player-wood-sword-drawn/wood-sword-drawn.pehb').read_bytes()
        r=exporter.RECORD.unpack_from(hitbox,exporter.HEADER.size)
        self.assertEqual(r[0].rstrip(b'\0'),b'SingleHandSword');self.assertAlmostEqual(r[-1],0.)
        self.assertFalse(list((stage/'player-wood-shield').glob('*.pehb')))

    def test_every_attachment_palette_matches_the_actual_rendered_body_joint(self):
        stage=self.stage();manifest=json.loads((stage/'starter-pair-manifest.json').read_text())
        body=source_path(next(p for p in manifest['sources'] if p['path'].endswith('recording.peas')))
        rig_path=source_path(next(p for p in manifest['sources'] if p['path'].endswith('-rig.json')))
        rig=json.loads(rig_path.read_text());body_manifest=json.loads((body.parent/'recording-manifest.json').read_text())
        def palettes(data,joint):
            exporter.clip_headers(data)
            _,_,nv,ni,nj,nc=struct.unpack_from('<4s5I',data);at=24+nv*56+ni*4+nj*48;result=[]
            self.assertLess(joint,nj)
            for _ in range(nc):
                frames=struct.unpack_from('<I',data,at+36)[0];at+=48
                for f in range(frames):
                    values=struct.unpack_from('<12f',data,at+(f*nj+joint)*48)
                    result.append([list(values[r:r+4]) for r in (0,4,8)]+[[0.,0.,0.,1.]])
                at+=frames*nj*48
            return result
        for group,(_,mount) in exporter.GROUPS.items():
            attachment=json.loads((stage/group/'recording-manifest.json').read_text())
            anchor={'mountMain':'Bip01 R Hand','mountBack':'Bip01 Spine3','Bone L Forearm':'Bone L Forearm'}[mount]
            index,part=next((i,p) for i,p in enumerate(rig['parts']) if anchor in p['bone_names'])
            joint=part['bone_names'].index(anchor)
            body_part=next(p for p in body_manifest['parts'] if p['part_index']==index and p['resource']==part['resource'])
            bpal=palettes((body.parent/body_part['file']).read_bytes(),joint)
            apal=palettes((stage/group/attachment['parts'][0]['file']).read_bytes(),0)
            self.assertEqual(len(bpal),len(apal))
            # Body palettes contain inverse bind; rigid attachment palettes don't.
            bone_bind=inverse_affine(part['inverse_binds'][joint]);offset=exporter.IDENTITY
            if mount!='Bone L Forearm':
                s=attachment['source_mount'];offset=exporter.trs(s['position'],s['rotation'],s['scale'])
            for actual,bone in zip(apal,bpal):
                expected=exporter.multiply(exporter.multiply(bone,bone_bind),offset)
                self.assertLess(exporter.pose.matrix_error(actual,expected),.001)

    @unittest.skipUnless(shutil.which('cc'),'C compiler required')
    def test_native_actor_scene_capsule_load_and_shared_clocks(self):
        stage=self.stage();src=exporter.ROOT/'rebuild/src'
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);code=root/'check.c';binary=root/'check'
            code.write_text('''#include "pe_actor_scene.h"
#include "pe_hitbox.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(int argc,char**argv){if(argc!=5)return 1;char err[256];
PeActorSceneManifest*s[3]={0};int bad=0;
for(int k=0;k<3;k++){s[k]=pe_actor_scene_manifest_load(argv[k+1],err,sizeof err);if(!s[k]){puts(err);return 2;}
if(pe_actor_scene_part_count(s[k])!=1)return 3;
const PeActor*a=pe_actor_scene_actor(s[k],0);if(pe_actor_stats(a).joints!=1)return 4;
for(size_t i=0;i<pe_actor_scene_clip_count(s[k]);i++){const PeActorClip*c=pe_actor_scene_clip_at(s[k],i),*b=pe_actor_scene_clip_at(s[0],i);
if(!b||strcmp(c->name,b->name)||c->fps!=b->fps||c->frames!=b->frames||c->duration!=b->duration||c->loop!=b->loop||!c->terminal_at_duration)return 5;
PeActorVertex v;bad|=!pe_actor_pose_vertex(a,i,c->duration*.5,0,&v);
for(int n=0;n<3;n++)bad|=!isfinite(v.position[n])||fabsf(v.position[n])>5;
}}
const PeActor*a=pe_actor_scene_actor(s[0],0);PeHitboxes*h=pe_hitboxes_load(argv[4],a,err,sizeof err);if(!h){puts(err);return 6;}
const PeHitboxDefinition*d=pe_hitboxes_get(h,0);bad|=pe_hitboxes_count(h)!=1||d->kind!=PE_HITBOX_ATTACK||d->group!=PE_HITBOX_RIGHT;
bad|=fabsf(d->bind_points[0][2]-.1f)>1e-6f||fabsf(d->bind_points[1][2]-1.5f)>1e-6f||fabsf(d->bind_points[3][0]-.2f)>1e-6f;
for(size_t i=0;i<pe_actor_stats(a).clips;i++){PeHitboxCapsule cap;bad|=!pe_hitbox_pose(h,a,0,i,pe_actor_clip(a,i)->duration*.5,(PeVec3){0},37,1,&cap);
bad|=!isfinite(cap.radius)||cap.radius<.15f||cap.radius>.25f;}
pe_hitboxes_free(h);for(int k=0;k<3;k++)pe_actor_scene_manifest_free(s[k]);return bad;}
''')
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(src),str(code),
                            str(src/'pe_actor.c'),str(src/'pe_actor_scene_manifest.c'),str(src/'pe_hitbox.c'),'-lm','-o',str(binary)],check=True,capture_output=True)
            subprocess.run([str(binary),*[str(stage/g/'recording.peas') for g in exporter.GROUPS],
                            str(stage/'player-wood-sword-drawn/wood-sword-drawn.pehb')],check=True,capture_output=True)


if __name__=='__main__':unittest.main()
