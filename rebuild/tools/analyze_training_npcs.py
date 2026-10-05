#!/usr/bin/env python3
"""Validate all nine closed original-Avatar NPC bakes and publish native scenes.

Python stdlib only. Does not launch Unity or install/overwrite assets. Output is
fresh: one <NPC_ID>/recording.peas scene per dedicated original NPC appearance.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

TOOLS=Path(__file__).resolve().parent
IDS=(9004,9005,9006,9009,9020,9031,9041,9047,9049)


def digest(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def convert(base,assets,output):
    if output.exists():raise ValueError('Output must be fresh')
    if output==assets or output in assets.parents or output==base or output in base.parents:raise ValueError('Output cannot contain inputs')
    inputs=[]
    for identity in IDS:
        capture=base.with_name(base.stem+'.npc-'+str(identity)+'.jsonl')
        rig=assets/str(identity)/'rig.json';launch=capture.with_suffix('.launch.json')
        if not all(path.is_file() for path in (capture,rig,launch)):raise ValueError('Missing closed NPC capture/rig/provenance: '+str(identity))
        if rig.stat().st_size>64*1024*1024:raise ValueError('NPC rig too large')
        source=json.loads(rig.read_text())
        if source.get('npc',{}).get('entity_id')!=identity:raise ValueError('NPC rig identity mismatch')
        provenance=json.loads(launch.read_text())
        if provenance.get('plan',{}).get('npc_id')!=identity:raise ValueError('NPC launch identity mismatch')
        inputs.append((identity,capture,rig))
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.npc-scenes-',dir=output.parent) as temporary:
        staging=Path(temporary);work=staging/'work';work.mkdir();published=staging/'published';published.mkdir();rows=[]
        for identity,capture,rig in inputs:
            report=work/(str(identity)+'-report.json');export=work/str(identity)
            subprocess.run([sys.executable,str(TOOLS/'analyze_reference_bake.py'),str(capture),'--rig',str(rig),
                            '--report',str(report),'--export-dir',str(export),'--clip','NormalIdle'],check=True)
            evidence=json.loads(report.read_text())
            if not evidence['complete'] or evidence['rig_mapping']['missing_bones'] or len(evidence['clips'])!=1 or evidence['clips'][0]['name']!='NormalIdle':
                raise ValueError('NPC bake lacks complete mapped idle')
            if not evidence['runtime_motion_detected'] or evidence['clips'][0]['changing_bones']==0:raise ValueError('NPC idle did not change pose')
            target=published/str(identity);shutil.copytree(export/'all',target)
            shutil.copyfile(report,target/'analysis.json')
            scene=json.loads((target/'recording-manifest.json').read_text())
            scene['appearance']='Dedicated original tutorial NPC mesh, outfit and material slots'
            scene['npc_id']=identity
            scene['limitations']=['Original-Avatar clip-only idle, not live AI/controller/IK or dialogue animation.',
                                  'Diffuse shader approximation; no morphs, eye/hair effects or equipped weapons.',
                                  'Native affine palette interpolation between sampled authored poses.']
            (target/'recording-manifest.json').write_text(json.dumps(scene,indent=2,allow_nan=False)+'\n')
            rows.append(dict(npc_id=identity,scene=str(identity)+'/recording.peas',capture=str(capture),capture_sha256=digest(capture),
                             rig=str(rig),rig_sha256=digest(rig),samples=evidence['samples'],
                             avatar=evidence['skeleton']['avatar'],changing_bones=evidence['clips'][0]['changing_bones'],
                             maximum_hierarchy_error=evidence['maximum_hierarchy_matrix_error']))
        manifest=dict(format='PE-training-NPC-scenes-1',npcs=rows,coordinates='NPC Model local UnityXYZ; entity placement applied at draw',
                      root_motion_validated=False,live_controller_behavior=False)
        (published/'manifest.json').write_text(json.dumps(manifest,indent=2,allow_nan=False)+'\n')
        published.rename(output)
    return manifest


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('capture_base',type=Path)
    parser.add_argument('--assets',type=Path,default=TOOLS.parent/'build/reference-assets/npcs')
    parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    try:
        report=convert(args.capture_base.expanduser().resolve(),args.assets.expanduser().resolve(),args.output.expanduser().resolve())
        print('Validated/exported '+str(len(report['npcs']))+' original NPC idle scenes')
    except(OSError,ValueError,KeyError,TypeError,subprocess.CalledProcessError) as error:parser.exit(1,'NPC analysis: '+str(error)+'\n')

if __name__=='__main__':main()
