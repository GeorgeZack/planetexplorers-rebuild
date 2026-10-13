#!/usr/bin/env python3
"""Stage, for every installed player actor part, which female solver bone moves
each skin joint: the joint's own bone, or its nearest solver ancestor. Offline;
reads staged groups, the starter rig and one bake's skeleton row, and never
launches anything."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import sys
from read_reference_adventure import source_label

ROOT=Path(__file__).resolve().parents[2]
RIG=ROOT/'rebuild/generated/pe_fbbik_source.json'
STARTER=ROOT/'rebuild/build/reference-player-starter/player-female-starter-rig.json'
ASSETS=ROOT/'rebuild/build/reference-assets'
GROUP='player-ik-drivers';FILE='player-ik-drivers.peid';MANIFEST='player-ik-drivers.json'
DEFENCE='player-defence'
MAX_ENTRIES=256;MAX_JOINTS=256;NAME=48

def require(value,message):
    if not value:raise ValueError(message)
def digest(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def actor(path):
    """Joint count, pe_actor_skeleton_hash and inverse binds of a PEAC file."""
    data=Path(path).read_bytes();require(data[:4]==b'PEAC' and len(data)>=24,'Not a PEAC actor: '+path.name)
    vertices,indices,joints,_=struct.unpack('<4I',data[8:24]);end=24+vertices*56+indices*4+joints*48
    require(0<joints<=MAX_JOINTS and end<=len(data),'Malformed PEAC actor: '+path.name);value=14695981039346656037
    for byte in data[8:20]+data[24:end]:value=((value^byte)*1099511628211)&0xffffffffffffffff
    return joints,value,struct.unpack('<%df'%(joints*12),data[end-joints*48:end])

def skeleton_paths(capture):
    """Bone name -> hierarchy path from a bake's skeleton row."""
    with Path(capture).open() as stream:
        for line in stream:
            row=json.loads(line)
            if row.get('type')=='skeleton':
                paths={}
                for node in row['nodes']:
                    name=node['path'].rsplit('/',1)[-1];paths[name]=None if name in paths else node['path']
                return paths
    raise ValueError('Bake has no skeleton row')

def driver(path,bones,what):
    """Nearest solver bone on a hierarchy path, the last component included."""
    require(path,'No unique hierarchy path for '+what)
    for name in reversed(path.split('/')):
        if name in bones:return bones[name]
    raise ValueError('No solver ancestor for '+what)

def scene_parts(folder):
    lines=(folder/'recording.peas').read_text().split('\n');require(lines[0]=='PEAS1','Not a PEAS1 scene: '+folder.name)
    return [line.split(' ')[0] for line in lines[1:] if line]

def group_entries(folder,bones,paths,starter):
    """(part file, joint names, drivers) for one staged scene."""
    manifest=json.loads((folder/'recording-manifest.json').read_text());files=scene_parts(folder)
    require([p['file'] for p in manifest['parts']]==files,'Scene and manifest parts differ: '+folder.name);entries=[]
    for part in manifest['parts']:
        joints,_,binds=actor(folder/part['file'])
        if manifest.get('source_mount_path'):   # one rigid joint baked at a mount transform
            require(joints==1,'Mounted part is not rigid: '+part['file']);mount=manifest['source_mount_path']
            require(paths.get(mount.rsplit('/',1)[-1])==mount,'Mount path is not in the bake: '+mount)
            entries.append((part['file'],[mount.rsplit('/',1)[-1]],[driver(mount,bones,mount)]));continue
        source=[p for p in starter['parts'] if p['resource']==part['resource']]
        require(len(source)==1 and len(source[0]['bone_names'])==joints,'No starter rig part for '+part['resource'])
        flat=[x for matrix in source[0]['inverse_binds'] for row in matrix[:3] for x in row]   # joint order is the mesh bone order
        require(all(abs(a-b)<=1e-6*max(1,abs(a)) for a,b in zip(flat,binds)),'Bind matrices differ from the starter rig: '+part['file'])
        names=source[0]['bone_names'];entries.append((part['file'],names,[driver(paths.get(n),bones,n) for n in names]))
    return entries

def encode(rig,entries):
    """entries: (group, part index, skeleton hash, drivers). PEID1, little-endian."""
    bones=rig['bones'];require(0<len(entries)<=MAX_ENTRIES and len({e[:2] for e in entries})==len(entries),'Invalid or duplicate driver entries')
    data=bytearray(b'PEID')+struct.pack('<II',1,len(bones))+struct.pack('<%dI'%len(bones),*[b['id'] for b in bones])+struct.pack('<I',len(entries))
    for group,part,skeleton,drivers in entries:
        require(group.isascii() and 0<len(group)<NAME and all(c.isalnum() or c=='-' for c in group),'Invalid group name: '+group)
        require(0<len(drivers)<=MAX_JOINTS and all(0<=d<len(bones) for d in drivers),'Invalid drivers: '+group)
        data+=group.encode().ljust(NAME,b'\0')+struct.pack('<IQI',part,skeleton,len(drivers))+bytes(drivers)+b'\0'*(-len(drivers)%4)
    return bytes(data)

def export(capture,stage,assets=ASSETS,rig_path=RIG,starter_path=STARTER):
    rig=json.loads(Path(rig_path).read_text())['rig'];starter=json.loads(Path(starter_path).read_text())
    require(not stage.exists(),'Stage directory must be fresh');bones={b['name']:b['index'] for b in rig['bones']};paths=skeleton_paths(capture)
    entries=[];records=[];sources=[]
    for folder in sorted(p for p in assets.glob('player*') if (p/'recording-manifest.json').is_file() and (p/'recording.peas').is_file()):
        for index,(file,names,drivers) in enumerate(group_entries(folder,bones,paths,starter)):
            joints,skeleton,_=actor(folder/file);entries.append((folder.name,index,skeleton,drivers))
            records.append(dict(group=folder.name,part=index,file=file,sha256=digest(folder/file),skeleton_hash='%016x'%skeleton,
                joints=names,drivers=[rig['bones'][d]['name'] for d in drivers]))
        sources.append(dict(path=source_label(folder/'recording-manifest.json'),sha256=digest(folder/'recording-manifest.json')))
    defence=assets/DEFENCE
    if (defence/'player-defence.json').is_file():   # one actor, no scene list
        names=json.loads((defence/'player-defence.json').read_text())['joint_names'];joints,skeleton,_=actor(defence/'player-defence.peac')
        require(joints==len(names),'Defence joint names differ from its actor');drivers=[driver(paths.get(n),bones,n) for n in names]
        entries.append((DEFENCE,0,skeleton,drivers))
        records.append(dict(group=DEFENCE,part=0,file='player-defence.peac',sha256=digest(defence/'player-defence.peac'),skeleton_hash='%016x'%skeleton,
            joints=names,drivers=[rig['bones'][d]['name'] for d in drivers]))
        sources.append(dict(path=source_label(defence/'player-defence.json'),sha256=digest(defence/'player-defence.json')))
    data=encode(rig,entries);stage.mkdir(parents=True);(stage/FILE).write_bytes(data)
    manifest=dict(schema='pe-player-ik-drivers-1',format='PEID1',file=FILE,sha256=hashlib.sha256(data).hexdigest(),bytes=len(data),
        bones=[b['name'] for b in rig['bones']],bone_ids=[b['id'] for b in rig['bones']],entries=records,
        rule='A joint is moved by its own bone when the solver owns it, otherwise by its nearest solver ancestor in the baked hierarchy.',
        sources=sources+[dict(path=source_label(Path(p)),sha256=digest(p)) for p in (capture,rig_path,starter_path)],
        limitations=['Fingers, toes, twist, face and mount transforms follow their solver ancestor rigidly; the original keeps their own animated local motion, which the baked palettes already contain.',
            'Entries bind a group folder and part order to the exact actor geometry; a reinstalled or changed group needs a new export.',
            'Body joint order is checked against the starter rig bind matrices; mounted parts rest on the mount path recorded by their own exporter.'])
    (stage/MANIFEST).write_text(json.dumps(manifest,indent=2)+'\n');return manifest

def install(stage,assets=ASSETS):
    manifest=json.loads((stage/MANIFEST).read_text());target=assets/GROUP
    require(manifest.get('schema')=='pe-player-ik-drivers-1' and digest(stage/FILE)==manifest['sha256'],'Stage files changed')
    require(assets.is_dir() and not target.exists() and not target.is_symlink(),'Target group already exists; this installer never replaces one')
    shutil.copytree(stage,target);require(digest(target/FILE)==manifest['sha256'],'Copied stage changed');return target

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--capture',type=Path,help='a completed bake whose skeleton row gives the hierarchy')
    p.add_argument('--stage',type=Path,required=True);p.add_argument('--install',action='store_true')
    a=p.parse_args(argv)
    try:
        stage=a.stage.expanduser().resolve()
        if a.install:
            require(not a.capture,'Installation takes an existing stage, without --capture');print('Installed new group:',install(stage))
        else:
            require(a.capture,'Export requires --capture');manifest=export(a.capture.expanduser().resolve(),stage)
            print('Staged %d parts, %d bytes: %s'%(len(manifest['entries']),manifest['bytes'],stage))
    except (OSError,ValueError,KeyError,TypeError,IndexError) as error:print('IK driver export: %s'%error,file=sys.stderr);return 1
    return 0
if __name__=='__main__':raise SystemExit(main())
