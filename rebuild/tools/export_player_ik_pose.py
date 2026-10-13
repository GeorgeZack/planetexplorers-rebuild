#!/usr/bin/env python3
"""Stage the female solver bones' animated local poses from completed
isolated-Animator bakes, for the disconnected pe_ik_pose loader. Offline; reads
existing captures and never launches anything."""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import shutil
import struct
import sys
import analyze_reference_bake as bake
from read_reference_adventure import source_label

ROOT=Path(__file__).resolve().parents[2]
RIG=ROOT/'rebuild/generated/pe_fbbik_source.json'
STARTER=ROOT/'rebuild/build/reference-player-starter/player-female-starter-rig.json'
ASSETS=ROOT/'rebuild/build/reference-assets'
GROUP='player-ik-pose';FILE='player-ik-pose.peik';MANIFEST='player-ik-pose.json'
# The solver's root objects sit above the baked Animator root and carry no animation.
STATIC=('FemaleModel','Model','IK')
MAX_CLIPS=64;MAX_FRAMES=20000

def require(value,message):
    if not value:raise ValueError(message)
def digest(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
def multiply(a,b):return [[sum(a[i][k]*b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]
def rigid_inverse(m):
    r=[[m[j][i] for j in range(3)] for i in range(3)];t=[-sum(r[i][k]*m[k][3] for k in range(3)) for i in range(3)]
    return [r[0]+[t[0]],r[1]+[t[1]],r[2]+[t[2]],[0.,0.,0.,1.]]
def decompose(m,name):
    """Position and unit quaternion of a rigid row-major matrix; refuses scale or shear."""
    r=[row[:3] for row in m[:3]]
    for i in range(3):
        for j in range(3):
            require(abs(sum(r[k][i]*r[k][j] for k in range(3))-(i==j))<2e-4,'Scaled or sheared solver bone: '+name)
    det=(r[0][0]*(r[1][1]*r[2][2]-r[1][2]*r[2][1])-r[0][1]*(r[1][0]*r[2][2]-r[1][2]*r[2][0])+r[0][2]*(r[1][0]*r[2][1]-r[1][1]*r[2][0]))
    require(det>0,'Mirrored solver bone: '+name)
    trace=r[0][0]+r[1][1]+r[2][2]
    if trace>0:
        s=math.sqrt(trace+1)*2;q=((r[2][1]-r[1][2])/s,(r[0][2]-r[2][0])/s,(r[1][0]-r[0][1])/s,s/4)
    elif r[0][0]>r[1][1] and r[0][0]>r[2][2]:
        s=math.sqrt(1+r[0][0]-r[1][1]-r[2][2])*2;q=(s/4,(r[0][1]+r[1][0])/s,(r[0][2]+r[2][0])/s,(r[2][1]-r[1][2])/s)
    elif r[1][1]>r[2][2]:
        s=math.sqrt(1+r[1][1]-r[0][0]-r[2][2])*2;q=((r[0][1]+r[1][0])/s,s/4,(r[1][2]+r[2][1])/s,(r[0][2]-r[2][0])/s)
    else:
        s=math.sqrt(1+r[2][2]-r[0][0]-r[1][1])*2;q=((r[0][2]+r[2][0])/s,(r[1][2]+r[2][1])/s,s/4,(r[1][0]-r[0][1])/s)
    n=math.sqrt(sum(x*x for x in q));values=[m[0][3],m[1][3],m[2][3]]+[x/n for x in q]
    require(all(math.isfinite(x) for x in values) and max(abs(x) for x in values[:3])<100,'Nonfinite or distant solver bone: '+name)
    return values

def animated_names(rig):return [b['name'] for b in rig['bones'] if b['name'] not in STATIC]
def bake_rig(starter,rig):
    """The starter rig's Avatar with one part naming every animated solver bone."""
    value=copy.deepcopy(starter);value['parts']=[dict(bone_names=animated_names(rig))];return value

def local_frames(rig,samples):
    """Per sample, the rig-ordered local transforms from model-space bone matrices."""
    identity=[[1.,0.,0.,0.],[0.,1.,0.,0.],[0.,0.,1.,0.],[0.,0.,0.,1.]];frames=[];previous=None
    for sample in samples:
        world=[identity if b['name'] in STATIC else sample['bones'][b['name']] for b in rig['bones']];frame=[]
        for bone,matrix in zip(rig['bones'],world):
            local=matrix if bone['parent']<0 else multiply(rigid_inverse(world[bone['parent']]),matrix)
            frame.append(decompose(local,bone['name']))
        if previous:   # keep each bone's quaternion in its previous sample's hemisphere
            for values,before in zip(frame,previous):
                if sum(a*b for a,b in zip(values[3:],before[3:]))<0:values[3:]=[-x for x in values[3:]]
        frames.append(frame);previous=frame
    return frames

def encode(rig,clips):
    """clips: (name, fps, duration, loop, frames). PEIK1, little-endian."""
    require(1<=len(clips)<=MAX_CLIPS and len({c[0] for c in clips})==len(clips),'Invalid or duplicate IK pose clips')
    bones=rig['bones'];data=bytearray(b'PEIK')+struct.pack('<III',1,len(bones),len(clips))
    data+=struct.pack('<%db'%len(bones),*[b['parent'] for b in bones])+b'\0'*(-len(bones)%4)
    for name,fps,duration,loop,frames in clips:
        require(name.isascii() and name.isalnum() and len(name)<32,'Invalid clip name: '+name)
        require(2<=len(frames)<=MAX_FRAMES and fps>0 and (len(frames)-2)/fps<duration<=(len(frames)-1)/fps+1e-3,'Invalid clip timing: '+name)
        data+=name.encode().ljust(32,b'\0')+struct.pack('<fIfI',fps,len(frames),duration,int(bool(loop)))
        for frame in frames:
            require(len(frame)==len(bones),'Frame bone count differs')
            for values in frame:data+=struct.pack('<7f',*values)
    return bytes(data)

def export(captures,stage,rig_path=RIG,starter_path=STARTER):
    rig=json.loads(Path(rig_path).read_text())['rig'];starter=json.loads(Path(starter_path).read_text())
    require(not stage.exists(),'Stage directory must be fresh');clips=[];records=[];sources=[]
    for capture,wanted in captures:
        report,kept=bake.analyze(capture,bake_rig(starter,rig),retain=True)
        require(wanted is None or set(wanted)<={c['name'] for c in report['clips']},'Selected clip is not in '+capture.name)
        require(not report['rig_mapping']['missing_bones'],'Bake lacks solver bones: '+', '.join(report['rig_mapping']['missing_bones']))
        nodes=report['skeleton']['nodes'];mapping=report['rig_mapping']['bones']
        for bone in rig['bones']:   # the baked hierarchy must be the solver's
            if bone['name'] in STATIC:continue
            parent=rig['bones'][bone['parent']]['name'];node=nodes[mapping[bone['name']]['index']]
            require(node['parent']==(0 if parent in STATIC else mapping[parent]['index']),'Bake parent differs for '+bone['name'])
        rate=report['header']['sampleRate'];names=[]
        for clip in report['clips']:
            if wanted is not None and clip['name'] not in wanted:continue
            frames=local_frames(rig,kept[clip['name']]);clips.append((clip['name'],float(rate),clip['runtime_length_f32'],clip['isLooping'],frames))
            records.append(dict(name=clip['name'],fps=rate,duration=clip['runtime_length_f32'],frames=len(frames),loop=clip['isLooping']));names.append(clip['name'])
        sources.append(dict(path=source_label(capture),sha256=digest(capture),clips=names))
    data=encode(rig,clips);stage.mkdir(parents=True);(stage/FILE).write_bytes(data)
    manifest=dict(schema='pe-player-ik-pose-1',format='PEIK1',file=FILE,sha256=hashlib.sha256(data).hexdigest(),bytes=len(data),
        bones=[b['name'] for b in rig['bones']],parents=[b['parent'] for b in rig['bones']],clips=records,
        coordinate_space='local to the solver parent; FemaleModel, Model and IK are identity',
        sources=sources+[dict(path=source_label(Path(rig_path)),sha256=digest(rig_path)),dict(path=source_label(Path(starter_path)),sha256=digest(starter_path))],
        limitations=['Poses are isolated-Animator bakes of single clips at the bake sample rate: no controller blending, procedural IK or root motion.',
            'Rotations are stored per sample; the loader interpolates between samples, which the original Animator does not do.',
            'The live character\'s Model object moves inside FemaleModel; here both are identity and world placement is the caller\'s.'])
    (stage/MANIFEST).write_text(json.dumps(manifest,indent=2)+'\n');return manifest

def install(stage,assets=ASSETS):
    manifest=json.loads((stage/MANIFEST).read_text());target=assets/GROUP
    require(manifest.get('schema')=='pe-player-ik-pose-1' and digest(stage/FILE)==manifest['sha256'],'Stage files changed')
    require(assets.is_dir() and not target.exists() and not target.is_symlink(),'Target group already exists; this installer never replaces one')
    shutil.copytree(stage,target);require(digest(target/FILE)==manifest['sha256'],'Copied stage changed');return target

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--capture',action='append',default=[],metavar='PATH[=CLIP,CLIP]',help='a completed bake, optionally limited to named clips')
    p.add_argument('--stage',type=Path,required=True);p.add_argument('--install',action='store_true')
    a=p.parse_args(argv)
    try:
        stage=a.stage.expanduser().resolve()
        if a.install:
            require(not a.capture,'Installation takes an existing stage, without --capture');print('Installed new group:',install(stage))
        else:
            require(a.capture,'Export requires --capture');captures=[]
            for item in a.capture:
                path,_,names=item.partition('=');captures.append((Path(path).expanduser().resolve(),names.split(',') if names else None))
            manifest=export(captures,stage)
            print('Staged %d clips, %d bytes: %s'%(len(manifest['clips']),manifest['bytes'],stage))
    except (OSError,ValueError,KeyError,TypeError) as error:print('IK pose export: %s'%error,file=sys.stderr);return 1
    return 0
if __name__=='__main__':raise SystemExit(main())
