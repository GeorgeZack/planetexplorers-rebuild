"""Shared-asset integrity and actual Torch PEAC/body-clock checks; no graphics."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

TOOLS=Path(__file__).resolve().parents[1]/'tools';sys.path.insert(0,str(TOOLS))
import export_baked_torch as torch


class BakedTorchTests(unittest.TestCase):
    def shared(self,root):
        visual=dict(materials=[dict(obj_symbol='torch',path_id=1,diffuse_file='torch.png')])
        files={'torch.obj':b'v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvn 0 0 1\nusemtl torch\nf 1/1/1 2/1/1 3/1/1\n',
               'torch-visual.json':json.dumps(visual).encode(),'torch.png':b'synthetic texture hash fixture'}
        for name,data in files.items():(root/name).write_bytes(data)
        prefab='Assets/Resources/Prefab/Item/Equip/Other/torch.prefab'
        manifest=dict(source=dict(item_id=10,prefab=dict(path=prefab,sha256=torch.pose.sha256(torch.ROOT/prefab))),
                      files={name:hashlib.sha256(data).hexdigest() for name,data in files.items()})
        (root/'torch.json').write_text(json.dumps(manifest));return manifest

    def test_shared_geometry_keeps_item_coordinates_and_single_mount(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);self.shared(root);visual,mesh=torch.read_shared_assets(root)
            for name in ('mountMain','mountBack'):
                rig=torch.torch_rig(dict(sex='female',avatar={}),visual,mesh,name)
                part=rig['parts'][0]
                self.assertEqual(part['bone_names'],[name]);self.assertEqual(part['inverse_binds'],[torch.IDENTITY])
                self.assertEqual(part['positions'],[[0.,0.,0.],[1.,0.,0.],[0.,1.,0.]])
                self.assertTrue(all(v['weight[0]']==1 and v['boneIndex[0]']==0 for v in part['skin']))
            with self.assertRaises(ValueError):torch.torch_rig(dict(sex='female',avatar={}),visual,mesh,'mountWaist')

    def test_changed_shared_mesh_texture_prefab_or_identity_rejected(self):
        for changed in ('mesh','texture','prefab','identity','traversal'):
            with self.subTest(changed=changed),tempfile.TemporaryDirectory() as d:
                root=Path(d);manifest=self.shared(root)
                if changed=='mesh':(root/'torch.obj').write_bytes(b'changed')
                if changed=='texture':(root/'torch.png').write_bytes(b'changed')
                if changed=='prefab':manifest['source']['prefab']['sha256']='0'*64
                if changed=='identity':manifest['source']['item_id']=33
                if changed=='traversal':manifest['files']['../outside.png']='0'*64
                (root/'torch.json').write_text(json.dumps(manifest))
                with self.assertRaises(ValueError):torch.read_shared_assets(root)

    def test_sword_style_requires_exact_complete_source_group(self):
        report=dict(skeleton=dict(gender='Female'),clips=[dict(name=n,sourcePathId=i) for n,i in torch.SWORD_CLIPS.items()])
        torch.check_style(report,'sword')
        self.assertEqual(report['clips'][0]['name'],'AttackIdle')
        for kind in ('normal-substitution','missing','duplicate','wrong-id','wrong-sex'):
            import copy
            r=copy.deepcopy(report)
            if kind=='normal-substitution':r['clips'][0]=dict(name='NormalIdle',sourcePathId=684)
            if kind=='missing':r['clips'].pop()
            if kind=='duplicate':r['clips'].append(r['clips'][0])
            if kind=='wrong-id':r['clips'][0]['sourcePathId']=684
            if kind=='wrong-sex':r['skeleton']['gender']='Male'
            with self.subTest(kind=kind),self.assertRaises(ValueError):torch.check_style(r,'sword')
        torch.check_style(dict(),'normal') # Existing Normal exports remain supported.
        with self.assertRaises(ValueError):torch.check_style(report,'imagined')

    def test_installed_scenes_native_loader_clocks_and_emitter(self):
        assets=TOOLS.parent/'build/reference-assets'
        paths=[assets/name/'recording.peas' for name in ('player','player-torch','player-torch-stowed')]
        # Compare this historical tool export with its preserved 21-clip body.
        baseline=TOOLS.parent/'build/sprint-base-01/player/recording.peas'
        if baseline.is_file():paths[0]=baseline
        if not all(p.is_file() for p in paths):self.skipTest('Recovered player/Torch scenes unavailable')
        if not shutil.which('cc'):self.skipTest('C compiler unavailable')
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);source=root/'check.c';binary=root/'check'
            source.write_text('''#include "pe_actor_scene.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char**argv){if(argc!=4)return 1;char e[256];
PeActorSceneManifest*body=pe_actor_scene_manifest_load(argv[1],e,sizeof e);if(!body)return 2;
int bad=0;for(int k=2;k<4;k++){PeActorSceneManifest*s=pe_actor_scene_manifest_load(argv[k],e,sizeof e);if(!s){puts(e);return 3;}
size_t tool_count=pe_actor_scene_clip_count(s),body_count=pe_actor_scene_clip_count(body);
/* The body may append the Knife-only408 pose. Torch still requires every
 * original shared clip, in the same order and with identical metadata. */
if(body_count==tool_count+1){const PeActorClip*extra=pe_actor_scene_clip_at(body,tool_count);
bad|=strcmp(extra->name,"RunSwordAttack")||extra->frames!=57||extra->fps!=30||extra->loop||!extra->terminal_at_duration||fabs(extra->duration-1.8333396911621094)>1e-6;
}else bad|=body_count!=tool_count;
bad|=pe_actor_scene_part_count(s)!=1;
const PeActor*a=pe_actor_scene_actor(s,0);bad|=pe_actor_stats(a).joints!=1;
for(size_t i=0;i<pe_actor_scene_clip_count(s);i++){const PeActorClip*c=pe_actor_scene_clip_at(s,i),*b=pe_actor_scene_clip_at(body,i);
if(!b){bad=1;continue;}
bad|=strcmp(c->name,b->name)||c->duration!=b->duration||c->fps!=b->fps||c->frames!=b->frames||c->loop!=b->loop||!c->terminal_at_duration;
float p[]={0,0,.449999988f},out[3];bad|=!pe_actor_pose_joint_point(a,i,c->duration*.5,0,p,out);
for(int n=0;n<3;n++)bad|=!isfinite(out[n])||fabsf(out[n])>5;
}pe_actor_scene_manifest_free(s);}pe_actor_scene_manifest_free(body);return bad;}
''')
            src=TOOLS.parent/'src'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(src),str(source),str(src/'pe_actor.c'),str(src/'pe_actor_scene_manifest.c'),'-lm','-o',str(binary)],check=True)
            subprocess.run([str(binary),*map(str,paths)],check=True)


if __name__=='__main__':unittest.main()
