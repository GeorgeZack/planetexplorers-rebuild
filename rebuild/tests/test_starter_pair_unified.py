"""Unified source-pose composition and unchanged input contracts; no graphics."""
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from read_reference_adventure import source_path
import export_starter_pair_unified as tool
from pe_actor_limits import PEAC_MAX_CLIPS


def actor(names):
    raw=struct.pack('<4s5I',b'PEAC',2,1,3,1,len(names))+bytes(56+12+48)
    for name in names:raw+=struct.pack('<32sfIfI',name.encode(),30,2,1/30,1)+bytes(96)
    return raw


class UnifiedTests(unittest.TestCase):
    def test_offline_native_bound_matches_and_sampler_limit_stays_independent(self):
        header=(tool.ROOT/'rebuild/src/pe_actor.h').read_text()
        self.assertEqual(int(re.search(r'#define PE_ACTOR_MAX_CLIPS (\d+)u',header)[1]),PEAC_MAX_CLIPS)
        self.assertEqual(PEAC_MAX_CLIPS,64)
        import reference_animator_bake
        self.assertEqual(reference_animator_bake.MAX_CLIPS,32)

    def test_appended40_and64_payloads_preserved_65_and_truncation_rejected(self):
        for count in (40,64):
            before=actor(['old'+str(i) for i in range(30)])
            more=actor(['new'+str(i) for i in range(count-30)])
            result=tool.append_actor(before,more)
            self.assertEqual(len(tool.clip_headers(result)),count)
            self.assertEqual(result[:20],before[:20]);self.assertEqual(result[24:len(before)],before[24:])
            for size in (1,48,95):
                with self.assertRaises(ValueError):tool.append_actor(before,more[:-size])
        with self.assertRaises(ValueError):tool.append_actor(actor(['a'+str(i) for i in range(40)]),actor(['b'+str(i) for i in range(25)]))
        with self.assertRaises(ValueError):tool.append_actor(actor(['duplicate']),actor(['duplicate']))
        before=actor(['a']);changed=bytearray(actor(['b']));changed[30]=1
        with self.assertRaises(ValueError):tool.append_actor(before,changed)

    def stage(self):
        path=Path(os.environ.get('PE_STARTER_UNIFIED_STAGE',str(tool.ROOT/'rebuild/build/starter-pair-unified-02')))
        if not (path/'unified-manifest.json').is_file():self.skipTest('Fresh unified starter-pair stage unavailable')
        return path

    def test_recovered_union_hashes_clocks_and_lossless30_defence_prefix(self):
        stage=self.stage();m=json.loads((stage/'unified-manifest.json').read_text())
        files=tool.pair.file_hashes(stage);files.pop('unified-manifest.json')
        self.assertEqual(files,m['files']);self.assertFalse(m['installed']);self.assertFalse(m['shield_block_pose_recovered'])
        assets=tool.ROOT/'rebuild/build/reference-assets'
        self.assertEqual(tool.validate(stage,assets/'player',assets/'player-defence'),dict(body_clips=31,defence_clips=40,defence_actor_clip_counts=[40]))
        self.assertEqual(m['existing_defence_files'],tool.pair.file_hashes(assets/'player-defence'))
        old=json.loads((assets/'player-defence/player-defence.json').read_text());new=json.loads((stage/'player-defence/player-defence.json').read_text())
        self.assertEqual(new['clips'][:30],old['clips']);self.assertEqual(new['clip_captures'][:-1],old['clip_captures'])
        self.assertEqual(new['clip_captures'][-1]['clips'],list(tool.SWORD))
        for name,info in new['files'].items():
            p=stage/'player-defence'/name;self.assertEqual(info,dict(bytes=p.stat().st_size,sha256=tool.pair.sha(p)))
        for p in m['sources']:self.assertEqual(tool.pair.sha(source_path(p)),p['sha256'])

    def test_all_body_and_attachment_payloads_are_exact_concatenations(self):
        stage=self.stage();assets=tool.ROOT/'rebuild/build/reference-assets'
        normal=tool.ROOT/'rebuild/build/starter-pair-assets-01';sword=tool.ROOT/'rebuild/build/starter-pair-sword-assets-01'
        pairs=[('player',assets/'player',tool.ROOT/'rebuild/build/sword-body-01/all')]
        pairs.extend((g,normal/g,sword/g) for g in tool.pair.GROUPS)
        for group,old,new in pairs:
            manifest=json.loads((old/'recording-manifest.json').read_text())
            for part in manifest['parts']:
                before=(old/part['file']).read_bytes();extra=(new/part['file']).read_bytes()
                self.assertEqual((stage/group/part['file']).read_bytes(),tool.append_actor(before,extra))
                self.assertEqual((stage/group/part['texture']).read_bytes(),(old/part['texture']).read_bytes())
            if manifest.get('hitbox'):
                name=manifest['hitbox']['file'];self.assertEqual((stage/group/name).read_bytes(),(old/name).read_bytes())

    @unittest.skipUnless(shutil.which('cc'),'C compiler required')
    def test_native31_body_wood_and40_defence_all_frames(self):
        self.native_stage(self.stage())

    def native_stage(self,stage):
        src=tool.ROOT/'rebuild/src'
        manifest=json.loads((stage/'unified-manifest.json').read_text());bc=manifest['counts']['body_clips'];dc=manifest['counts']['defence_clips']
        self.assertIn((bc,dc),((31,40),(33,42)))
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);code=root/'check.c';binary=root/'check'
            code.write_text(r'''#include "pe_actor_scene.h"
#include "pe_hitbox.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv){assert(argc==7);char err[256];PeActorSceneManifest*s[4];
for(int k=0;k<4;k++){s[k]=pe_actor_scene_manifest_load(argv[k+1],err,sizeof err);if(!s[k]){puts(err);return 2;}
assert(pe_actor_scene_clip_count(s[k])==BODY_COUNT);
for(size_t i=0;i<BODY_COUNT;i++){const PeActorClip*a=pe_actor_scene_clip_at(s[k],i),*b=pe_actor_scene_clip_at(s[0],i);
assert(!strcmp(a->name,b->name)&&a->fps==b->fps&&a->duration==b->duration&&a->frames==b->frames&&a->loop==b->loop);}}
PeActor*d=pe_actor_load(argv[5],err,sizeof err);if(!d){puts(err);return 3;}assert(pe_actor_stats(d).clips==DEFENCE_COUNT);
PeHitboxes*h=pe_hitboxes_load(argv[6],d,err,sizeof err);if(!h){puts(err);return 4;}assert(pe_hitboxes_count(h)==16);
for(size_t i=0;i<BODY_COUNT;i++){const PeActorClip*b=pe_actor_scene_clip_at(s[0],i);size_t j=pe_actor_find_clip(d,b->name);const PeActorClip*a=pe_actor_clip(d,j);
assert(a&&!strcmp(a->name,b->name)&&a->fps==b->fps&&a->frames==b->frames&&a->duration==b->duration&&a->loop==b->loop);}
for(size_t i=0;i<DEFENCE_COUNT;i++){const PeActorClip*c=pe_actor_clip(d,i);for(size_t f=0;f<c->frames;f++){
double t=f==c->frames-1?c->duration:f/c->fps;
for(size_t j=0;j<16;j++){PeHitboxCapsule cap;assert(pe_hitbox_pose(h,d,j,i,t,(PeVec3){0},37,1,&cap));assert(isfinite(cap.radius)&&cap.radius>0);}}}
pe_hitboxes_free(h);pe_actor_free(d);for(int k=0;k<4;k++)pe_actor_scene_manifest_free(s[k]);return 0;}
''')
            code.write_text(code.read_text().replace('BODY_COUNT',str(bc)).replace('DEFENCE_COUNT',str(dc)))
            args=['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(src),str(code),str(src/'pe_actor.c'),str(src/'pe_actor_scene_manifest.c'),str(src/'pe_hitbox.c'),'-lm','-o',str(binary)]
            if os.environ.get('PE_TEST_SANITIZE'):args[1:1]=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']
            subprocess.run(args,check=True,capture_output=True)
            subprocess.run([str(binary),*[str(stage/g/'recording.peas') for g in ('player',*tool.pair.GROUPS)],str(stage/'player-defence/player-defence.peac'),str(stage/'player-defence/player-defence.pehb')],check=True,capture_output=True)

    def backward_stage(self):
        stage=tool.ROOT/'rebuild/build/starter-pair-unified-03'
        if not (stage/'unified-manifest.json').is_file():self.skipTest('Backward33/42 stage unavailable')
        return stage

    def test_backward33_42_preserves_entire31_40_stage_and_new_exact_payloads(self):
        stage=self.backward_stage();manifest=json.loads((stage/'unified-manifest.json').read_text())
        self.assertEqual(manifest['schema'],'pe-starter-pair-unified-backward-1')
        before=source_path(Path(manifest['base_stage']['path'])/'unified-manifest.json',manifest['base_stage']['manifest_sha256']).parent
        self.assertEqual(tool.pair.file_hashes(before),manifest['base_stage']['files'])
        self.assertEqual(tool.validate(stage,before/'player',before/'player-defence',backward=True),dict(body_clips=33,defence_clips=42,defence_actor_clip_counts=[42]))
        current=tool.pair.file_hashes(stage);current.pop('unified-manifest.json');self.assertEqual(current,manifest['files'])
        for p in manifest['sources']:self.assertEqual(tool.pair.sha(source_path(p)),p['sha256'])
        self.assertFalse(manifest['installed']);self.assertFalse(manifest['shield_block_pose_recovered'])
        fresh=tool.ROOT/'rebuild/build/starter-pair-sword-backward-assets-02'
        for group in ('player',*tool.pair.GROUPS):
            extra=tool.ROOT/'rebuild/build/sword-backward-body-02/assets/all' if group=='player' else fresh/group
            old=json.loads((before/group/'recording-manifest.json').read_text())
            for part in old['parts']:
                self.assertEqual((stage/group/part['file']).read_bytes(),tool.append_actor((before/group/part['file']).read_bytes(),(extra/part['file']).read_bytes()))
            if old.get('hitbox'):
                name=old['hitbox']['file'];self.assertEqual((stage/group/name).read_bytes(),(before/group/name).read_bytes())
        old=json.loads((before/'player-defence/player-defence.json').read_text());new=json.loads((stage/'player-defence/player-defence.json').read_text())
        self.assertEqual(new['clips'][:40],old['clips']);self.assertEqual(new['clip_captures'][:-1],old['clip_captures'])
        self.assertEqual(new['clip_captures'][-1]['clips'],['SwordWB','SwordRB'])
        for key,value in new['files'].items():
            file=stage/'player-defence'/key;self.assertEqual(value,dict(bytes=file.stat().st_size,sha256=tool.pair.sha(file)))

    @unittest.skipUnless(shutil.which('cc'),'C compiler required')
    def test_native33_body_wood_and42_defence_all_frames(self):
        self.native_stage(self.backward_stage())


if __name__=='__main__':unittest.main()
