"""Running-pose append integrity and optional real native asset validation."""
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
TOOLS=Path(__file__).resolve().parents[1]/'tools';sys.path.insert(0,str(TOOLS))
import export_running_player as tool
from test_directional_player import actor


class AppendTests(unittest.TestCase):
    def fixture(self,root):
        for name,clips in [('old',tool.NAMES),('new',(tool.RUN_NAME,))]:
            p=root/name;p.mkdir();(p/'actor.peac').write_bytes(actor(clips));(p/'texture.png').write_bytes(b'texture')
            (p/'recording.peas').write_text('PEAS1\nactor.peac texture.png\n')
            manifest=dict(parts=[dict(file='actor.peac',texture='texture.png',texture_sha256=tool.digest(p/'texture.png'))],
                          clips=[dict(clip=c) for c in clips],sources=[])
            (p/'recording-manifest.json').write_text(json.dumps(manifest))
        return root/'old',root/'new',root/'combined'

    def test_append_preserves_geometry_and_every_prior_payload(self):
        old=actor(tool.NAMES);new=actor((tool.RUN_NAME,));combined=tool.append_actor(old,new)
        self.assertEqual(combined[:20],old[:20]);self.assertEqual(combined[24:len(old)],old[24:])
        self.assertEqual(tool.names(tool.clip_headers(combined)),tool.NAMES+(tool.RUN_NAME,))
        self.assertEqual(len(combined),len(old)+len(new)-(24+56+12+48))

    def test_duplicate_overflow_truncation_or_changed_bind_rejected(self):
        old=actor(tool.NAMES);good=actor((tool.RUN_NAME,))
        bad=bytearray(good);bad[24+56+12]^=1
        for candidate in (actor((tool.NAMES[0],)),actor(tuple('Other'+str(i) for i in range(tool.PEAC_MAX_CLIPS-len(tool.NAMES)+1))),good[:-1],bytes(bad)):
            with self.assertRaises(ValueError):tool.append_actor(old,candidate)

    def test_scene_preserves_inputs_and_retains_both_capture_provenances(self):
        with tempfile.TemporaryDirectory() as d:
            old,new,out=self.fixture(Path(d));before=(old/'actor.peac').read_bytes();source=(new/'actor.peac').read_bytes()
            result=tool.append_scene(old,new,out)
            self.assertEqual([c['clip'] for c in result['clips']],list(tool.NAMES+(tool.RUN_NAME,)))
            self.assertEqual(len(result['append_provenance']),2)
            self.assertEqual((old/'actor.peac').read_bytes(),before);self.assertEqual((new/'actor.peac').read_bytes(),source)
            with self.assertRaises(ValueError):tool.append_scene(old,new,out)

    def test_texture_or_hitbox_change_does_not_create_combined_directory(self):
        for kind in ('texture','hitbox'):
            with self.subTest(kind=kind),tempfile.TemporaryDirectory() as d:
                old,new,out=self.fixture(Path(d))
                if kind=='texture':(new/'texture.png').write_bytes(b'changed')
                else:
                    for p,value in [(old,b'old'),(new,b'new')]:
                        m=json.loads((p/'recording-manifest.json').read_text());m['hitbox']=dict(file='knife.pehb')
                        (p/'recording-manifest.json').write_text(json.dumps(m));(p/'knife.pehb').write_bytes(value)
                with self.assertRaises(ValueError):tool.append_scene(old,new,out)
                self.assertFalse(out.exists())


class ActualRunningTests(unittest.TestCase):
    def test_optional_staged_native_running_pose_and_retained_clips(self):
        stage=Path(os.environ.get('PE_RUNNING_STAGE',str(TOOLS.parent/'build/running-player-20261006')))
        assets=TOOLS.parent/'build/reference-assets'
        if not (stage/'running-manifest.json').is_file() or not shutil.which('cc'):
            self.skipTest('Optional staged running-pose group and C compiler required')
        manifest=json.loads((stage/'running-manifest.json').read_text())
        # Installation preserves the original20/29 group in a separate backup;
        # never weaken retained-prefix verification to compare against itself.
        candidates=([Path(os.environ['PE_RUNNING_BASE'])] if 'PE_RUNNING_BASE' in os.environ else
                    [assets,TOOLS.parent/'build/player-before-running-20261006'])
        base=next((p for p in candidates if tool.hashes(p)==manifest['installed_base_files']),None)
        self.assertIsNotNone(base,'Retained original base required; set PE_RUNNING_BASE to its verified backup')
        tool.validate(stage,base)
        self.assertEqual(manifest['files'],tool.hashes(stage))
        checked=stage
        if tool.names(tool.scene_clips(assets/'player/recording.peas'))==tool.NAMES+(tool.RUN_NAME,):
            self.assertEqual(manifest['files'],tool.hashes(assets))
            checked=assets
        tool.validate(checked,base)
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);source=root/'validate.c';source.write_text(r'''#include "pe_actor_scene.h"
#include "pe_hitbox.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv){
 assert(argc==2);char path[4096],error[256];
 const char *groups[]={"player","player-knife-drawn","player-knife-stowed"};
 for(size_t i=0;i<3;i++){
  snprintf(path,sizeof path,"%s/%s/recording.peas",argv[1],groups[i]);
  PeActorSceneManifest *s=pe_actor_scene_manifest_load(path,error,sizeof error);assert(s);
  assert(pe_actor_scene_clip_count(s)==21&&!strcmp(pe_actor_scene_clip_at(s,20)->name,"RunSwordAttack"));
  if(i==1){
   snprintf(path,sizeof path,"%s/player-knife-drawn/knife-drawn.pehb",argv[1]);
   const PeActor*a=pe_actor_scene_actor(s,0);PeHitboxes*h=pe_hitboxes_load(path,a,error,sizeof error);assert(h);
   const PeActorClip*k=pe_actor_clip(a,20);
   for(size_t f=0;f<k->frames;f++){PeHitboxCapsule p;double t=f+1==k->frames?k->duration:(double)f/k->fps;
    assert(pe_hitbox_pose(h,a,0,20,t,(PeVec3){0,0,0},0,1,&p)&&isfinite(p.a.x)&&p.radius>0);}
   pe_hitboxes_free(h);
  }
  pe_actor_scene_manifest_free(s);
 }
 snprintf(path,sizeof path,"%s/player-defence/player-defence.peac",argv[1]);
 PeActor*a=pe_actor_load(path,error,sizeof error);assert(a&&pe_actor_stats(a).clips==30&&pe_actor_stats(a).joints==23);
 assert(!strcmp(pe_actor_clip(a,29)->name,"RunSwordAttack")&&pe_actor_clip(a,29)->frames==57);
 snprintf(path,sizeof path,"%s/player-defence/player-defence.pehb",argv[1]);PeHitboxes*h=pe_hitboxes_load(path,a,error,sizeof error);assert(h&&pe_hitboxes_count(h)==16);
 for(size_t c=0;c<30;c++){const PeActorClip*k=pe_actor_clip(a,c);
  for(size_t f=0;f<k->frames;f++)for(size_t i=0;i<16;i++){PeHitboxCapsule p;double t=f+1==k->frames?k->duration:(double)f/k->fps;
   assert(pe_hitbox_pose(h,a,i,c,t,(PeVec3){0,0,0},0,1,&p)&&isfinite(p.a.x)&&isfinite(p.a.y)&&isfinite(p.a.z)&&p.radius>0&&p.radius<1);}
 }
 pe_hitboxes_free(h);pe_actor_free(a);return 0;
}
''')
            src=TOOLS.parent/'src';exe=root/'validate'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',
                '-I',str(src),str(source),str(src/'pe_actor.c'),str(src/'pe_hitbox.c'),str(src/'pe_actor_scene_manifest.c'),'-lm','-o',str(exe)],check=True,capture_output=True)
            subprocess.run([str(exe),str(checked)],check=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1'})

if __name__=='__main__':unittest.main()
