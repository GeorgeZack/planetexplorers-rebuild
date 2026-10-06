#!/usr/bin/env python3
"""Stage a complete directional body/tool/defence group; optionally install it.

No Unity launch or desktop input. Installation preserves each old folder under
an explicitly fresh backup directory and rolls back directory moves on error.
Stop the native client before installation. Stage remains as export provenance.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

import analyze_reference_bake as bake
from export_baked_knife import clip_headers, check_body
from reference_animator_bake import DEFAULT_CLIPS
from inventory_reference_bake import CLIPS, DIRECTIONAL_CLIPS

ROOT=Path(__file__).resolve().parents[2]
TOOLS=Path(__file__).resolve().parent
GROUPS=('player','player-knife-stowed','player-shovel-stowed','player-shovel',
        'player-axe','player-wood-axe','player-pickaxe-stowed','player-pickaxe',
        'player-torch-stowed','player-torch','player-knife-drawn','player-defence')
NAMES=DEFAULT_CLIPS+tuple(DIRECTIONAL_CLIPS)
EXTRAS=('Felling1','Felling2','Felling3','Spade1','Spade2','Spade3','Mine1','Mine2','Mine3')


def digest(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def file_hashes(root):
    result={}
    for group in GROUPS:
        directory=root/group
        if directory.is_symlink() or not directory.is_dir():raise ValueError('Missing/linked group: '+group)
        for p in sorted(directory.rglob('*')):
            if p.is_symlink():raise ValueError('Linked output is not installable')
            if p.is_file():result[str(p.relative_to(root))]=digest(p)
    return result


def scene_clips(scene):
    lines=scene.read_text().splitlines()
    if len(lines)<2 or lines[0]!='PEAS1':raise ValueError('Invalid scene')
    return clip_headers((scene.parent/lines[1].split()[0]).read_bytes())


def validate_groups(stage):
    expected=scene_clips(stage/'player/recording.peas')
    if tuple(c[:32].rstrip(b'\0').decode('ascii') for c in expected)!=NAMES:raise ValueError('Directional body clip names/order differ')
    for group in GROUPS[:-1]:check_body(stage/group/'recording.peas',expected)
    defence=clip_headers((stage/'player-defence/player-defence.peac').read_bytes())
    if tuple(c[:32].rstrip(b'\0').decode('ascii') for c in defence)!=NAMES+EXTRAS or defence[:len(expected)]!=expected:
        raise ValueError('Defence clocks/order differ')
    if not (stage/'player-knife-drawn/knife-drawn.pehb').is_file() or not (stage/'player-defence/player-defence.pehb').is_file():
        raise ValueError('Missing authored hitbox files')
    return expected


def export(capture, rig, assets, extras, stage):
    if stage.exists():raise ValueError('Stage must be fresh')
    if len(extras)!=3:raise ValueError('Require Felling, Spade and Mine captures in that order')
    report,_=bake.analyze(capture,json.loads(rig.read_text()))
    if tuple(c['name'] for c in report['clips'])!=NAMES or any(c['sourcePathId']!=CLIPS[c['name']] for c in report['clips']):
        raise ValueError('Expected exact female normal-directional capture')
    if report['skeleton']['gender']!='Female' or report['skeleton']['avatar']!='FemaleAvatar':raise ValueError('Expected female source')
    # The CLI validates launch provenance before exporting. Keep it mandatory.
    if not capture.with_suffix('.launch.json').is_file():raise ValueError('Matching launch provenance required')
    for i,extra in enumerate(extras):
        extra_report,_=bake.analyze(extra,json.loads(rig.read_text()))
        if tuple(c['name'] for c in extra_report['clips'])!=EXTRAS[i*3:i*3+3]:raise ValueError('Unexpected tool capture')
    stage.mkdir(parents=True)
    def run(tool,*args):subprocess.run([sys.executable,str(TOOLS/tool),*[str(a) for a in args]],check=True)
    run('analyze_reference_bake.py',capture,'--rig',rig,'--report',stage/'body-report.json','--export-dir',stage/'body-export')
    shutil.copytree(stage/'body-export/all',stage/'player')
    body=stage/'player/recording.peas'
    shared=[capture,'--rig',rig,'--body-scene',body]
    for mount in ('stowed','drawn'):
        run('export_baked_knife.py',*shared,'--knife-collision',assets/'knife-collision.json','--visual-assets',assets,
            '--attachment',mount,'--output',stage/('player-knife-'+mount))
    for tool,visual in [('shovel','wood-shovel'),('pickaxe','wood-pickaxe')]:
        for mount in ('drawn','stowed'):
            name='player-'+tool+('-stowed' if mount=='stowed' else '')
            run('export_baked_'+tool+'.py',*shared,'--visual-assets',assets/visual,'--mode',mount,'--output',stage/name)
    for name,visual in [('player-axe','safety-axe'),('player-wood-axe','wood-axe')]:
        run('export_baked_axe.py',*shared,'--visual-assets',assets/visual,'--output',stage/name)
    for mount in ('held','stowed'):
        run('export_baked_torch.py',*shared,'--visual-assets',assets/'torch','--attachment',mount,
            '--output',stage/('player-torch'+('-stowed' if mount=='stowed' else '')))
    extra_args=[v for p in extras for v in ('--extra-capture',p)]
    run('export_player_defence.py',capture,'--rig',rig,'--actor-dir',stage/'player',*extra_args,'--output',stage/'player-defence')
    validate_groups(stage)
    manifest=dict(schema='pe-directional-player-1',groups=list(GROUPS),clips=list(NAMES),defence_clips=list(NAMES+EXTRAS),
                  sources=[dict(path=str(p),sha256=digest(p)) for p in (capture,capture.with_suffix('.launch.json'),rig,*extras)],
                  files=file_hashes(stage),limitations=['Pure sampled leaves; no full Animator blending or procedural IK.',
                    'Native fixed-facing side movement may select strafe leaves separately from original third-person turn branches.'])
    (stage/'directional-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return manifest


def install(stage, assets, backup):
    manifest=json.loads((stage/'directional-manifest.json').read_text())
    if manifest.get('schema')!='pe-directional-player-1' or manifest.get('groups')!=list(GROUPS):raise ValueError('Invalid stage manifest')
    if manifest.get('files')!=file_hashes(stage):raise ValueError('Stage files changed')
    validate_groups(stage)
    if backup.exists() or not assets.is_dir() or any((assets/g).is_symlink() for g in GROUPS):raise ValueError('Fresh backup and real target directories required')
    if any(p==q or p in q.parents or q in p.parents for p,q in ((stage,assets),(backup,assets),(stage,backup))):
        raise ValueError('Stage, assets and backup must be separate nonnested directories')
    backup.parent.mkdir(parents=True,exist_ok=True)
    if assets.stat().st_dev!=backup.parent.stat().st_dev:raise ValueError('Backup must share target filesystem')
    with tempfile.TemporaryDirectory(prefix='.directional-install-',dir=assets.parent) as tmp:
        incoming=Path(tmp)
        for group in GROUPS:shutil.copytree(stage/group,incoming/group)
        if file_hashes(incoming)!=manifest['files']:raise ValueError('Copied stage changed')
        backup.mkdir();moved=[];installed=[]
        try:
            for group in GROUPS:
                target=assets/group
                if target.exists():os.rename(target,backup/group);moved.append(group)
                os.rename(incoming/group,target);installed.append(group)
        except OSError:
            for group in reversed(installed):os.rename(assets/group,incoming/group)
            for group in reversed(moved):os.rename(backup/group,assets/group)
            raise
        receipt=dict(schema='pe-directional-install-1',stage=str(stage),assets=str(assets),backup=str(backup),groups=list(GROUPS),files=manifest['files'])
        (backup/'installation.json').write_text(json.dumps(receipt,indent=2)+'\n')
    return receipt


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--capture',type=Path);p.add_argument('--rig',type=Path,default=ROOT/'rebuild/build/reference-player-starter/player-female-starter-rig.json')
    p.add_argument('--assets',type=Path,default=ROOT/'rebuild/build/reference-assets')
    p.add_argument('--extra-capture',type=Path,action='append',default=[])
    p.add_argument('--stage',type=Path,required=True);p.add_argument('--install',action='store_true');p.add_argument('--backup',type=Path)
    a=p.parse_args()
    try:
        stage=a.stage.expanduser().resolve();assets=a.assets.expanduser().resolve()
        if a.install:
            if a.capture or not a.backup:raise ValueError('Installation requires existing stage and --backup, without --capture')
            install(stage,assets,a.backup.expanduser().resolve());print('Installed complete group; previous folders preserved in',a.backup)
        else:
            if not a.capture or a.backup:raise ValueError('Export requires --capture; --backup is only for installation')
            export(a.capture.expanduser().resolve(),a.rig.expanduser().resolve(),assets,[x.expanduser().resolve() for x in a.extra_capture],stage)
            print('Staged all11 body/attachment scenes and29-clip defence:',stage)
    except (OSError,ValueError,KeyError,TypeError,subprocess.CalledProcessError) as e:p.exit(1,str(e)+'\n')
if __name__=='__main__':main()
