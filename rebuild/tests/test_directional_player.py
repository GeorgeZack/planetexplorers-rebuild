"""Coherent offline actor group and backup/rollback contracts; no graphics."""
import json
import os
from pathlib import Path
import struct
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
TOOLS=Path(__file__).resolve().parents[1]/'tools';sys.path.insert(0,str(TOOLS))
import export_directional_player as tool


def actor(names):
    raw=struct.pack('<4s5I',b'PEAC',2,1,3,1,len(names))+bytes(56+12+48)
    for name in names:raw+=struct.pack('<32sfIfI',name.encode(),30,2,1/30,1)+bytes(2*48)
    return raw


class DirectionalTests(unittest.TestCase):
    def fixture(self,root):
        stage=root/'stage';stage.mkdir();assets=root/'assets';assets.mkdir();backup=root/'backup'
        for group in tool.GROUPS:
            p=stage/group;p.mkdir();old=assets/group;old.mkdir();(old/'original').write_text(group)
            if group=='player-defence':
                (p/'player-defence.peac').write_bytes(actor(tool.NAMES+tool.EXTRAS));(p/'player-defence.pehb').write_bytes(b'test geometry')
            else:
                (p/'actor.peac').write_bytes(actor(tool.NAMES));(p/'recording.peas').write_text('PEAS1\nactor.peac texture.png\n');(p/'texture.png').write_bytes(b'test texture')
                if group=='player-knife-drawn':(p/'knife-drawn.pehb').write_bytes(b'test geometry')
        self.manifest(stage)
        return stage,assets,backup

    def manifest(self,stage):
        (stage/'directional-manifest.json').write_text(json.dumps(dict(schema='pe-directional-player-1',groups=list(tool.GROUPS),files=tool.file_hashes(stage))))

    def unchanged(self,assets):
        for group in tool.GROUPS:self.assertEqual((assets/group/'original').read_text(),group)

    def test_complete_install_preserves_old_groups_and_staged_provenance(self):
        with tempfile.TemporaryDirectory() as d:
            stage,assets,backup=self.fixture(Path(d));before=tool.file_hashes(stage)
            tool.install(stage,assets,backup)
            self.assertEqual(tool.file_hashes(assets),before);self.assertEqual(tool.file_hashes(stage),before)
            self.unchanged(backup);self.assertTrue((backup/'installation.json').is_file())
            with self.assertRaises(ValueError):tool.install(stage,assets,backup)

    def test_missing_changed_or_mismatched_clocks_leave_targets_untouched(self):
        for kind in ('hash','missing','clock','linked'):
            with self.subTest(kind=kind),tempfile.TemporaryDirectory() as d:
                stage,assets,backup=self.fixture(Path(d));p=stage/'player-torch/actor.peac'
                if kind=='hash':p.write_bytes(p.read_bytes()+b'changed')
                if kind=='missing':p.unlink()
                if kind=='clock':
                    names=list(tool.NAMES);names[10],names[11]=names[11],names[10];p.write_bytes(actor(names));self.manifest(stage)
                if kind=='linked':p.unlink();p.symlink_to(stage/'player/actor.peac')
                with self.assertRaises((ValueError,OSError)):tool.install(stage,assets,backup)
                self.unchanged(assets);self.assertFalse(backup.exists())

    def test_mid_install_failure_restores_all_old_folders(self):
        with tempfile.TemporaryDirectory() as d:
            stage,assets,backup=self.fixture(Path(d));rename=os.rename;calls=0
            def fail_once(a,b):
                nonlocal calls
                calls+=1
                if calls==8:raise OSError('simulated rename failure')
                rename(a,b)
            with mock.patch.object(tool.os,'rename',side_effect=fail_once),self.assertRaises(OSError):tool.install(stage,assets,backup)
            self.unchanged(assets);self.assertEqual(list(backup.iterdir()),[])

    def test_all64_body_clips_combine_and65_rejected(self):
        # Offline/native format64; sampler graph lifetime bound stays32.
        for count in (40,64):tool.clip_headers(actor(['Clip'+str(i) for i in range(count)]))
        with self.assertRaises(ValueError):tool.clip_headers(actor(['Clip'+str(i) for i in range(65)]))

class ActualDirectionalTests(unittest.TestCase):
    def test_optional_staged_native_scenes_and_all_defence_poses(self):
        stage=Path(os.environ.get('PE_DIRECTIONAL_STAGE',str(TOOLS.parent/'build/directional-player-20261006')))
        if not (stage/'directional-manifest.json').is_file() or not shutil.which('cc'):
            self.skipTest('Optional complete directional capture/export and C compiler required')
        tool.validate_groups(stage)
        manifest=json.loads((stage/'directional-manifest.json').read_text())
        self.assertEqual(manifest['files'],tool.file_hashes(stage))
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);source=root/'validate.c';source.write_text(r'''#include "pe_actor_scene.h"
#include "pe_hitbox.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv){
 assert(argc==2);char path[4096],error[256];
 const char *groups[]={"player","player-knife-stowed","player-shovel-stowed","player-shovel","player-axe","player-wood-axe","player-pickaxe-stowed","player-pickaxe","player-torch-stowed","player-torch","player-knife-drawn"};
 for(size_t i=0;i<11;i++){
  snprintf(path,sizeof path,"%s/%s/recording.peas",argv[1],groups[i]);
  PeActorSceneManifest *s=pe_actor_scene_manifest_load(path,error,sizeof error);
  if(!s){fprintf(stderr,"%s: %s\n",path,error);return 2;}
  assert(pe_actor_scene_clip_count(s)==20);
  assert(!strcmp(pe_actor_scene_clip_at(s,10)->name,"NormalWB"));
  if(i==10){
   snprintf(path,sizeof path,"%s/player-knife-drawn/knife-drawn.pehb",argv[1]);
   PeHitboxes *h=pe_hitboxes_load(path,pe_actor_scene_actor(s,0),error,sizeof error);assert(h);pe_hitboxes_free(h);
  }
  pe_actor_scene_manifest_free(s);
 }
 snprintf(path,sizeof path,"%s/player-defence/player-defence.peac",argv[1]);
 PeActor *a=pe_actor_load(path,error,sizeof error);assert(a&&pe_actor_stats(a).clips==29&&pe_actor_stats(a).joints==23);
 snprintf(path,sizeof path,"%s/player-defence/player-defence.pehb",argv[1]);
 PeHitboxes *h=pe_hitboxes_load(path,a,error,sizeof error);assert(h&&pe_hitboxes_count(h)==16);
 for(size_t c=0;c<29;c++){
  const PeActorClip *k=pe_actor_clip(a,c);assert(k);
  for(size_t f=0;f<k->frames;f++)for(size_t i=0;i<16;i++){
   double t=f+1==k->frames?k->duration:(double)f/k->fps;PeHitboxCapsule p;
   assert(pe_hitbox_pose(h,a,i,c,t,(PeVec3){0,0,0},0,1,&p));
   assert(isfinite(p.a.x)&&isfinite(p.a.y)&&isfinite(p.a.z)&&p.radius>0&&p.radius<1);
  }
 }
 pe_hitboxes_free(h);pe_actor_free(a);return 0;
}
''')
            src=TOOLS.parent/'src';exe=root/'validate'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',
                '-I',str(src),str(source),str(src/'pe_actor.c'),str(src/'pe_hitbox.c'),str(src/'pe_actor_scene_manifest.c'),'-lm','-o',str(exe)],check=True,capture_output=True)
            subprocess.run([str(exe),str(stage)],check=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1'})


if __name__=='__main__':unittest.main()
