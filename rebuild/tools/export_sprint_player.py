#!/usr/bin/env python3
"""Append only qualified SprintSwordAttack to the installed21/30 Knife group.

Fresh stage with optional checked installation; no launch or gameplay admission.
Existing actor
payloads, hitboxes, PNGs and material sidecars are retained literally.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import shutil
import struct
import tempfile
import export_running_player as running
from export_baked_knife import clip_headers, read_json, check_body
from read_reference_adventure import source_label

ROOT=Path(__file__).resolve().parents[2]
NAME='SprintSwordAttack'
GROUPS=('player','player-knife-drawn','player-knife-stowed','player-defence')
SOURCES=(ROOT/'rebuild/build/starter-pair-reaction-01/player',ROOT/'rebuild/build/knife-reaction-01-drawn',
         ROOT/'rebuild/build/knife-reaction-01-stowed',ROOT/'rebuild/build/starter-pair-reaction-01/player-defence')


def select_actor(data):
    headers=clip_headers(data)
    _,_,nv,ni,nj,_=struct.unpack_from('<4s5I',data)
    prefix=24+nv*56+ni*4+nj*48;offset=prefix;selected=[]
    for header in headers:
        end=offset+48+struct.unpack_from('<I',header,36)[0]*nj*48
        if header[:32].rstrip(b'\0')==NAME.encode():selected.append(data[offset:end])
        offset=end
    if len(selected)!=1:raise ValueError('Exactly one sprint leaf required')
    header=selected[0][:48]
    rate,frames,duration,flags=struct.unpack_from('<fIfI',header,32)
    if (duration!=1.8000068664550781 or frames!=56 or rate!=30 or flags!=0):
        raise ValueError('Exact nonloop terminal female401 clock required')
    result=bytearray(data[:prefix]);struct.pack_into('<I',result,20,1)
    return bytes(result)+selected[0]


def selected_scene(source,target):
    meta=read_json(source/'recording-manifest.json')
    for part in meta['parts']:
        if not running.bake.pose.safe_basename(part['file'],'.peac') or not running.bake.pose.safe_basename(part['texture'],'.png'):
            raise ValueError('Unsafe scene part basename')
    declared=['PEAS1']+[part['file']+' '+part['texture'] for part in meta['parts']]
    if (source/'recording.peas').read_text('ascii').splitlines()!=declared:
        raise ValueError('Scene/manifest part order differs')
    chosen=[c for c in meta['clips'] if c['clip']==NAME]
    if len(chosen)!=1:raise ValueError('Sprint source metadata missing/duplicated')
    expected=None
    for part in meta['parts']:
        data=select_actor((source/part['file']).read_bytes())
        headers=clip_headers(data)
        if expected is not None and headers!=expected:raise ValueError('Multipart sprint clocks differ')
        expected=headers
    check_body(source/'recording.peas',clip_headers((source/meta['parts'][0]['file']).read_bytes()))
    shutil.copytree(source,target)
    for part in meta['parts']:(target/part['file']).write_bytes(select_actor((source/part['file']).read_bytes()))
    meta['clips']=chosen
    meta['selection_provenance']={'manifest':source_label(source/'recording-manifest.json'),
                                 'sha256':running.digest(source/'recording-manifest.json'),'clip':NAME}
    (target/'recording-manifest.json').write_text(json.dumps(meta,indent=2)+'\n')


def snapshot(root):
    return {str(p.relative_to(root)):running.digest(p) for g in GROUPS for p in sorted((root/g).rglob('*')) if p.is_file()}


def build(base,output):
    before=snapshot(base)
    for i,g in enumerate(GROUPS):
        file=base/g/('player-defence.peac' if i==3 else read_json(base/g/'recording-manifest.json')['parts'][0]['file'])
        headers=clip_headers(file.read_bytes())
        if len(headers)!=(30 if i==3 else 21) or NAME in running.names(headers):
            raise ValueError('Require retained21-clip body/Knife and30-clip defence baseline')
    output.mkdir()
    with tempfile.TemporaryDirectory(prefix='.sprint-select-',dir=output.parent) as d:
        for i,g in enumerate(GROUPS[:3]):
            selected=Path(d)/g;selected_scene(SOURCES[i],selected)
            meta=running.append_scene(base/g,selected,output/g)
            meta['append_provenance'][1]={'path':source_label(SOURCES[i]/'recording-manifest.json'),
                                          'sha256':running.digest(SOURCES[i]/'recording-manifest.json'),'selected_clip':NAME}
            meta['limitations'][-1]='SprintSwordAttack is a source-baked isolated leaf; motion/contact integration and acceptance are separate.'
            (output/g/'recording-manifest.json').write_text(json.dumps(meta,indent=2)+'\n')
    old=read_json(base/'player-defence/player-defence.json');new=read_json(SOURCES[3]/'player-defence.json')
    for key in ('sex','entries','joint_names','skeleton_hash','source_trigger'):
        if old[key]!=new[key]:raise ValueError('Defence source identity differs: '+key)
    if (base/'player-defence/player-defence.pehb').read_bytes()!=(SOURCES[3]/'player-defence.pehb').read_bytes():
        raise ValueError('Defence capsule binding differs')
    dest=output/'player-defence';shutil.copytree(base/'player-defence',dest)
    data=running.append_actor((base/'player-defence/player-defence.peac').read_bytes(),select_actor((SOURCES[3]/'player-defence.peac').read_bytes()))
    (dest/'player-defence.peac').write_bytes(data)
    old['clips'] += [c for c in new['clips'] if c['name']==NAME]
    captures=[dict(v,clips=[NAME]) for v in new['sources'] if v['path'].endswith('.jsonl')]
    if len(captures)!=1:raise ValueError('One closed sprint capture provenance required')
    old['clip_captures'] += captures
    old['sources'] += new['sources'];old['extension']='Retained30 exact defence payloads plus sprint401'
    old['files']={p.name:dict(bytes=p.stat().st_size,sha256=running.digest(p)) for p in (dest/'player-defence.peac',dest/'player-defence.pehb')}
    (dest/'player-defence.json').write_text(json.dumps(old,indent=2)+'\n')
    body=clip_headers((output/'player'/read_json(output/'player/recording-manifest.json')['parts'][0]['file']).read_bytes())
    defence=clip_headers(data)
    if body[-1]!=defence[-1]:raise ValueError('Sprint body/defence clocks differ')
    if snapshot(base)!=before:raise ValueError('Baseline changed during export')
    report={'schema':'pe-sprint-player-1','groups':list(GROUPS),'body_clips':22,'defence_clips':31,
            'baseline_sha256':before,'files_sha256':snapshot(output),'installed':False,
            'exporter_sha256':running.digest(Path(__file__)),
            'dependency_sha256':{name:running.digest(Path(__file__).with_name(name)) for name in ('export_running_player.py','export_baked_knife.py')},
            'source_sha256':{source_label(p):running.digest(p) for source in SOURCES for p in sorted(source.rglob('*')) if p.is_file()},
            'limitations':'Append-only isolated poses; no gameplay admission or motion asset.'}
    (output/'sprint-manifest.json').write_text(json.dumps(report,indent=2)+'\n')


def install(base,stage,motion,destination,backup):
    """Prepared whole groups, checked old baseline and rollback on a failed rename."""
    if backup.exists() or (destination/'player-sprint-motion').exists():
        raise ValueError('Fresh backup and absent sprint motion installation required')
    if snapshot(destination)!=snapshot(base):raise ValueError('Installed baseline changed; refusing replacement')
    backup.mkdir()
    with tempfile.TemporaryDirectory(prefix='.sprint-install-',dir=destination.parent) as d:
        prepared=Path(d)
        for g in GROUPS:shutil.copytree(stage/g,prepared/g)
        shutil.copytree(motion,prepared/'player-sprint-motion')
        old_moved=[];new_moved=[]
        try:
            for g in (*GROUPS,'player-sprint-motion'):
                if g in GROUPS:
                    (destination/g).rename(backup/g);old_moved.append(g)
                (prepared/g).rename(destination/g);new_moved.append(g)
        except BaseException:
            for g in reversed(new_moved):(destination/g).rename(prepared/g)
            for g in reversed(old_moved):(backup/g).rename(destination/g)
            raise
    report={'schema':1,'baseline_sha256':snapshot(backup),'installed_sha256':snapshot(destination),
            'motion_sha256':{p.name:running.digest(p) for p in (destination/'player-sprint-motion').iterdir() if p.is_file()}}
    (backup/'installation.json').write_text(json.dumps(report,indent=2)+'\n')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--base',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--check',action='store_true');p.add_argument('--install',type=Path,metavar='FRESH_BACKUP',help='after --check, install complete groups with checked baseline and rollback')
    a=p.parse_args()
    try:
        base=a.base.resolve();out=a.output.resolve()
        if not out.is_relative_to(ROOT/'rebuild/build'):raise ValueError('Stage belongs under rebuild/build')
        if a.check:
            with tempfile.TemporaryDirectory(prefix='.sprint-check-',dir=out.parent) as d:
                fresh=Path(d)/'group';build(base,fresh)
                if snapshot(fresh)!=snapshot(out):raise ValueError('Stale sprint stage files')
                # Temporary selection paths vary; the actual source hashes and
                # payload identities are compared independently below.
                report=read_json(out/'sprint-manifest.json');expected=read_json(fresh/'sprint-manifest.json')
                if report!=expected:raise ValueError('Stale sprint stage manifest')
            if a.install:
                backup=a.install.resolve()
                if not backup.is_relative_to(ROOT/'rebuild/build'):raise ValueError('Backup belongs under rebuild/build')
                motion=ROOT/'rebuild/build/sprint-source-motion-01'
                report=read_json(motion/'sprint-root.json')
                if report.get('sprint_source_extraction_qualified') is not True or running.digest(motion/'sprint-root.pesr')!=report['output']['sha256']:
                    raise ValueError('Qualified sprint root group required')
                install(base,out,motion,ROOT/'rebuild/build/reference-assets',backup)
        else:
            if a.install:raise ValueError('--install requires --check')
            if out.exists():raise ValueError('Fresh output required')
            build(base,out)
    except (OSError,ValueError,KeyError,struct.error) as e:p.exit(1,str(e)+'\n')
    if a.install:print('Sprint player installed; previous complete groups preserved in '+str(a.install))
    else:print('Sprint player '+('checked' if a.check else 'staged')+'; installed inputs unchanged')

if __name__=='__main__':main()
