#!/usr/bin/env python3
"""Stage RunSwordAttack body/knife/defence extensions without installing them.

Uses the preserved directional capture and the fresh closed running-pose bake.
Existing installed actors and every retained clip payload remain unchanged.
No graphics, launch, root-motion generation or runtime combat activation.
"""
import argparse
import copy
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys

import analyze_reference_bake as bake
from export_baked_knife import clip_headers, check_body
from export_directional_player import ROOT, TOOLS, NAMES, EXTRAS, digest, scene_clips
from pe_actor_limits import PEAC_MAX_CLIPS

RUN_NAME='RunSwordAttack'
GROUPS=('player','player-knife-drawn','player-knife-stowed','player-defence')


def names(headers):return tuple(c[:32].rstrip(b'\0').decode('ascii') for c in headers)


def append_actor(old, new):
    """Append only after exact geometry/bind compatibility and complete bounds."""
    old_headers=clip_headers(old);new_headers=clip_headers(new)
    if len(old_headers)+len(new_headers)>PEAC_MAX_CLIPS or set(names(old_headers))&set(names(new_headers)):
        raise ValueError('Combined clips exceed bound or duplicate names')
    def prefix_end(data):
        _,_,nv,ni,nj,_=struct.unpack_from('<4s5I',data)
        return 24+nv*56+ni*4+nj*48
    at=prefix_end(old);bt=prefix_end(new)
    if old[:20]!=new[:20] or old[24:at]!=new[24:bt]:raise ValueError('Geometry/skeleton/inverse binds differ')
    prefix=bytearray(old[:at]);struct.pack_into('<I',prefix,20,len(old_headers)+len(new_headers))
    result=bytes(prefix)+old[at:]+new[bt:]
    if len(result)>bake.pose.MAX_BYTES:raise ValueError('Combined actor too large')
    return result


def append_scene(old_dir, new_dir, output):
    if output.exists():raise ValueError('Fresh combined scene directory required')
    old=json.loads((old_dir/'recording-manifest.json').read_text());new=json.loads((new_dir/'recording-manifest.json').read_text())
    if len(old['parts'])!=len(new['parts']):raise ValueError('Scene part count differs')
    expected=scene_clips(old_dir/'recording.peas')+scene_clips(new_dir/'recording.peas')
    actors={};total=0
    for a,b in zip(old['parts'],new['parts']):
        for key in ('file','texture','texture_sha256'):
            if a[key]!=b[key]:raise ValueError('Scene material identity differs')
        if (old_dir/a['texture']).read_bytes()!=(new_dir/b['texture']).read_bytes():raise ValueError('Scene texture bytes differ')
        data=append_actor((old_dir/a['file']).read_bytes(),(new_dir/b['file']).read_bytes())
        if clip_headers(data)!=expected:raise ValueError('Part clip clocks differ')
        total+=len(data)
        if total>512*1024*1024:raise ValueError('Combined scene exceeds byte bound')
        actors[a['file']]=data
    if (old_dir/'recording.peas').read_bytes()!=(new_dir/'recording.peas').read_bytes():raise ValueError('Scene part list differs')
    if old.get('hitbox') or new.get('hitbox'):
        if not old.get('hitbox') or not new.get('hitbox') or old['hitbox']['file']!=new['hitbox']['file']:
            raise ValueError('Hitbox binding differs')
        if (old_dir/old['hitbox']['file']).read_bytes()!=(new_dir/new['hitbox']['file']).read_bytes():
            raise ValueError('Authored hitbox geometry differs')
    result=copy.deepcopy(old);result['clips']=old['clips']+new['clips']
    result['sources']=old.get('sources',[])+new.get('sources',[])
    result['append_provenance']=[dict(path=str(p.resolve()),sha256=digest(p)) for p in (old_dir/'recording-manifest.json',new_dir/'recording-manifest.json')]
    result['limitations']=old.get('limitations',[])+['RunSwordAttack is a sampled pose extension only; root displacement and moving attack gameplay require separate validation.']
    output.parent.mkdir(parents=True,exist_ok=True);shutil.copytree(old_dir,output)
    for name,data in actors.items():(output/name).write_bytes(data)
    (output/'recording-manifest.json').write_text(json.dumps(result,indent=2)+'\n')
    check_body(output/'recording.peas',expected)
    return result


def hashes(root):
    result={}
    for group in GROUPS:
        for p in sorted((root/group).rglob('*')):
            if p.is_symlink():raise ValueError('Linked staged asset')
            # PEAM1 sidecars are installed and verified by export_actor_materials.py,
            # not by this group; every file this exporter writes stays pinned.
            if p.is_file() and not p.name.endswith('.peas.materials'):result[str(p.relative_to(root))]=digest(p)
    return result


def validate(stage, assets):
    body=scene_clips(stage/'player/recording.peas');old=scene_clips(assets/'player/recording.peas')
    if names(old)!=NAMES or names(body)!=NAMES+(RUN_NAME,) or body[:-1]!=old:
        raise ValueError('Expected preserved20 plus one running body clip')
    for group in GROUPS[:3]:
        check_body(stage/group/'recording.peas',body)
        manifest=json.loads((assets/group/'recording-manifest.json').read_text())
        for part in manifest['parts']:
            before=(assets/group/part['file']).read_bytes();after=(stage/group/part['file']).read_bytes()
            # Header clip count is the only retained-prefix alteration.
            if before[:20]!=after[:20] or before[24:]!=after[24:len(before)]:raise ValueError('Retained actor bytes changed')
    old_def=(assets/'player-defence/player-defence.peac').read_bytes();new_def=(stage/'player-defence/player-defence.peac').read_bytes()
    old_headers=clip_headers(old_def);new_headers=clip_headers(new_def)
    if names(old_headers)!=NAMES+EXTRAS or names(new_headers)!=NAMES+EXTRAS+(RUN_NAME,):raise ValueError('Defence clip order differs')
    if old_def[:20]!=new_def[:20] or old_def[24:]!=new_def[24:len(old_def)]:raise ValueError('Retained defence bytes changed')
    if new_headers[-1]!=body[-1]:raise ValueError('Running body and defence clocks differ')
    if (assets/'player-defence/player-defence.pehb').read_bytes()!=(stage/'player-defence/player-defence.pehb').read_bytes():
        raise ValueError('Defence capsule binding changed')


def export(capture, base_capture, extras, rig, assets, stage):
    if stage.exists():raise ValueError('Fresh running stage required')
    if len(extras)!=3:raise ValueError('Require ordered Felling/Spade/Mine captures')
    report,_=bake.analyze(capture,json.loads(rig.read_text()))
    clips=report['clips']
    if report['skeleton']['gender']!='Female' or len(clips)!=1 or clips[0]['name']!=RUN_NAME or clips[0]['sourcePathId']!=408:
        raise ValueError('Expected exact female RunSwordAttack408 capture')
    if not capture.with_suffix('.launch.json').is_file():raise ValueError('Matching launch provenance required')
    base,_=bake.analyze(base_capture,json.loads(rig.read_text()))
    if tuple(c['name'] for c in base['clips'])!=NAMES:raise ValueError('Base capture must be exact20 directional group')
    if names(scene_clips(assets/'player/recording.peas'))!=NAMES:raise ValueError('Installed20-clip base required')
    before=hashes(assets);stage.mkdir(parents=True)
    def run(tool,*args):subprocess.run([sys.executable,str(TOOLS/tool),*[str(a) for a in args]],check=True)
    run('analyze_reference_bake.py',capture,'--rig',rig,'--report',stage/'running-report.json','--export-dir',stage/'running-body')
    fresh=stage/'running-body/all';append_scene(assets/'player',fresh,stage/'player')
    for attachment in ('drawn','stowed'):
        output=stage/('running-knife-'+attachment)
        run('export_baked_knife.py',capture,'--rig',rig,'--knife-collision',assets/'knife-collision.json','--visual-assets',assets,
            '--body-scene',fresh/'recording.peas','--attachment',attachment,'--output',output)
        append_scene(assets/('player-knife-'+attachment),output,stage/('player-knife-'+attachment))
    args=[v for p in [*extras,capture] for v in ('--extra-capture',p)]
    run('export_player_defence.py',base_capture,'--rig',rig,'--actor-dir',stage/'player',*args,'--output',stage/'player-defence')
    validate(stage,assets)
    if hashes(assets)!=before:raise ValueError('Installed assets changed during export')
    manifest=dict(schema='pe-running-player-1',groups=list(GROUPS),body_clips=list(NAMES+(RUN_NAME,)),defence_clips=list(NAMES+EXTRAS+(RUN_NAME,)),
                  sources=[dict(path=str(p),sha256=digest(p)) for p in (capture,capture.with_suffix('.launch.json'),base_capture,rig,*extras)],
                  installed_base_files=before,files=hashes(stage),installed=False,
                  limitations=['Poses only: no authoritative root displacement, moving attack admission, skill change or trajectory.'])
    (stage/'running-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');return manifest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--capture',type=Path,required=True);p.add_argument('--base-capture',type=Path,required=True)
    p.add_argument('--extra-capture',type=Path,action='append',default=[])
    p.add_argument('--rig',type=Path,default=ROOT/'rebuild/build/reference-player-starter/player-female-starter-rig.json')
    p.add_argument('--assets',type=Path,default=ROOT/'rebuild/build/reference-assets');p.add_argument('--stage',type=Path,required=True)
    a=p.parse_args()
    try:
        export(a.capture.expanduser().resolve(),a.base_capture.expanduser().resolve(),[p.expanduser().resolve() for p in a.extra_capture],
               a.rig.expanduser().resolve(),a.assets.expanduser().resolve(),a.stage.expanduser().resolve())
        print('Staged21-clip body/drawn/stowed Knife and30-clip defence. Installed assets unchanged; no moving attack activation.')
    except (OSError,ValueError,KeyError,TypeError,subprocess.CalledProcessError) as e:p.exit(1,str(e)+'\n')
if __name__=='__main__':main()
