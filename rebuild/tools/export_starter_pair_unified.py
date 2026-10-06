#!/usr/bin/env python3
"""Stage31 body/equipment clips and one lossless40-clip defence actor.

Preserves all existing30 defence clip payloads and appends10 qualified Sword
leaves with matching clocks. No installation or gameplay activation.
"""
import argparse
import copy
import json
from pathlib import Path
import shutil
import tempfile

import export_player_defence as defence
import export_starter_pair_assets as pair
from export_baked_knife import read_json,clip_headers,check_body
from export_directional_player import NAMES,EXTRAS,scene_clips
from export_running_player import append_actor
from inventory_reference_bake import SWORD_CLIPS,SWORD_BACKWARD_CLIPS

ROOT=pair.ROOT
BASE=NAMES+('RunSwordAttack',)
SWORD=tuple(SWORD_CLIPS)
BODY=BASE+SWORD
BACKWARD=tuple(SWORD_BACKWARD_CLIPS)
DEFENCE=NAMES+EXTRAS+('RunSwordAttack',)
GROUPS=('player',*pair.GROUPS,'player-defence')


def names(headers):return tuple(h[:32].rstrip(b'\0').decode('ascii') for h in headers)


def checked_stage(path,expected):
    m=read_json(path/'starter-pair-manifest.json')
    if m['schema']!='pe-starter-pair-attachments-1' or m['groups']!=list(pair.GROUPS) or m['clips']!=list(expected) or m['installed'] or m['shield_block_pose_recovered']:
        raise ValueError('Unsupported/incorrect starter pair source stage')
    actual=pair.file_hashes(path);actual.pop('starter-pair-manifest.json')
    if m['files']!=actual:raise ValueError('Starter pair stage files changed')
    return m


def append_scene(old_dir,new_dir,output,old_names=BASE,new_names=SWORD):
    if output.exists():raise ValueError('Fresh scene output required')
    old=read_json(old_dir/'recording-manifest.json');new=read_json(new_dir/'recording-manifest.json')
    if names(scene_clips(old_dir/'recording.peas'))!=old_names or names(scene_clips(new_dir/'recording.peas'))!=new_names:
        raise ValueError('Unexpected retained/new source clip group')
    if len(old['parts'])!=len(new['parts']) or (old_dir/'recording.peas').read_bytes()!=(new_dir/'recording.peas').read_bytes():
        raise ValueError('Body/attachment scene material mapping differs')
    combined={}
    for a,b in zip(old['parts'],new['parts']):
        if any(a[k]!=b[k] for k in ('file','texture','texture_sha256','resource')):raise ValueError('Scene part identity differs')
        if (old_dir/a['texture']).read_bytes()!=(new_dir/b['texture']).read_bytes():raise ValueError('Source material bytes differ')
        before=(old_dir/a['file']).read_bytes();result=append_actor(before,(new_dir/b['file']).read_bytes())
        if result[:20]!=before[:20] or result[24:len(before)]!=before[24:]:raise ValueError('Retained clip/geometry bytes changed')
        combined[a['file']]=result
    if bool(old.get('hitbox'))!=bool(new.get('hitbox')):raise ValueError('Hitbox presence differs')
    if old.get('hitbox'):
        name=old['hitbox']['file']
        if name!=new['hitbox']['file'] or (old_dir/name).read_bytes()!=(new_dir/name).read_bytes():raise ValueError('Exact source capsule binding differs')
    result=copy.deepcopy(old);result['clips']=old['clips']+new['clips']
    result['sources']=old.get('sources',[])+new.get('sources',[])
    result['append_provenance']=[dict(path=str(p),sha256=pair.sha(p)) for p in (old_dir/'recording-manifest.json',new_dir/'recording-manifest.json')]
    result['pose_scope']=f'Preserved{len(old_names)} poses plus{len(new_names)} qualified Sword leaves'
    result['limitations']=[f'All{len(old_names)} original clip payloads preserved; exact{len(new_names)} source Sword leaves appended.',
                           'Controller transitions/blending and IK remain separate from sampled leaves.',
                           'Shield attachments are unblocked forearm poses; no masked block pose is claimed.',
                           'Staged data only; runtime selection, compatibility and installation are separate.']
    output.parent.mkdir(parents=True,exist_ok=True);shutil.copytree(old_dir,output)
    for name,data in combined.items():(output/name).write_bytes(data)
    (output/'recording-manifest.json').write_text(json.dumps(result,indent=2)+'\n')
    check_body(output/'recording.peas',scene_clips(old_dir/'recording.peas')+scene_clips(new_dir/'recording.peas'))
    return result


def validate(stage,body_source=None,defence_source=None,backward=False):
    extra=BACKWARD if backward else ()
    body=scene_clips(stage/'player/recording.peas')
    if names(body)!=BODY+extra:raise ValueError('Unified body clip names/order differ')
    for group in ('player',*pair.GROUPS):check_body(stage/group/'recording.peas',body)
    combined=clip_headers((stage/'player-defence/player-defence.peac').read_bytes())
    if names(combined)!=DEFENCE+SWORD+extra:raise ValueError('Defence union lost/duplicated source clips')
    if combined[:len(NAMES)]!=body[:len(NAMES)] or combined[len(DEFENCE)-1]!=body[len(NAMES)] or combined[len(DEFENCE):]!=body[len(BASE):]:
        raise ValueError('Body/defence source clocks differ')
    if body_source:
        source=read_json(body_source/'recording-manifest.json')
        for part in source['parts']:
            before=(body_source/part['file']).read_bytes();after=(stage/'player'/part['file']).read_bytes()
            if before[:20]!=after[:20] or before[24:]!=after[24:len(before)]:raise ValueError('Existing body bytes changed')
    if defence_source:
        before=(defence_source/'player-defence.peac').read_bytes();after=(stage/'player-defence/player-defence.peac').read_bytes()
        if before[:20]!=after[:20] or before[24:]!=after[24:len(before)]:raise ValueError('Existing defence bytes changed')
        if (defence_source/'player-defence.pehb').read_bytes()!=(stage/'player-defence/player-defence.pehb').read_bytes():
            raise ValueError('Defence geometry/skeleton/capsules changed')
    return dict(body_clips=len(body),defence_clips=len(combined),defence_actor_clip_counts=[len(combined)])


def append_defence(old_dir,new_dir,output,old_names=DEFENCE,new_names=SWORD):
    old=read_json(old_dir/'player-defence.json');new=read_json(new_dir/'player-defence.json')
    for key in ('sex','entries','joint_names','skeleton_hash','source_trigger'):
        if old[key]!=new[key]:raise ValueError('Defence source identity differs: '+key)
    if (old_dir/'player-defence.pehb').read_bytes()!=(new_dir/'player-defence.pehb').read_bytes():
        raise ValueError('Defence capsule binding differs')
    combined=append_actor((old_dir/'player-defence.peac').read_bytes(),(new_dir/'player-defence.peac').read_bytes())
    if names(clip_headers(combined))!=old_names+new_names:raise ValueError('Defence union clip order differs')
    shutil.copytree(old_dir,output)
    (output/'player-defence.peac').write_bytes(combined)
    result=copy.deepcopy(old);result['clips']=old['clips']+new['clips']
    captures=new.get('clip_captures') or [dict(source,clips=list(new_names)) for source in new['sources'] if source['path'].endswith('.jsonl')]
    if len(captures)!=1:raise ValueError('One exact Sword capture required')
    result['clip_captures']=old['clip_captures']+captures
    result['sources']=old['sources']+new['sources']
    result['maximum_point_error_m']=max(old['maximum_point_error_m'],new['maximum_point_error_m'])
    result['extension']=f'Retained{len(old_names)} exact defence payloads plus{len(new_names)} qualified Sword leaves'
    result['append_provenance']=[dict(path=str(p),sha256=pair.sha(p)) for p in (old_dir/'player-defence.json',new_dir/'player-defence.json')]
    result['files']={p.name:dict(bytes=p.stat().st_size,sha256=pair.sha(p)) for p in (output/'player-defence.peac',output/'player-defence.pehb')}
    (output/'player-defence.json').write_text(json.dumps(result,indent=2)+'\n')
    return result


def export(assets,normal_pair,sword_pair,sword_body,capture,rig,output):
    inputs=[assets,normal_pair,sword_pair,sword_body,capture,rig,ROOT/'Assets']
    output=pair.fresh_output(output,inputs)
    normal=checked_stage(normal_pair,BASE);sword=checked_stage(sword_pair,SWORD)
    if names(scene_clips(assets/'player/recording.peas'))!=BASE or names(scene_clips(sword_body/'recording.peas'))!=SWORD:
        raise ValueError('Exact21 and10 source bodies required')
    # Require this same capture to have supplied the checked Sword attachment stage.
    if not any(Path(s['path']).resolve()==capture.resolve() and s['sha256']==pair.sha(capture) for s in sword['sources']):
        raise ValueError('Sword stage did not use supplied capture')
    previous=pair.file_hashes(assets/'player-defence')
    sources=[normal_pair/'starter-pair-manifest.json',sword_pair/'starter-pair-manifest.json',
             assets/'player/recording-manifest.json',sword_body/'recording-manifest.json',capture,rig]
    source_hashes={p:pair.sha(p) for p in sources}
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.starter-unified-',dir=output.parent) as directory:
        stage=Path(directory)
        append_scene(assets/'player',sword_body,stage/'player')
        for group in pair.GROUPS:append_scene(normal_pair/group,sword_pair/group,stage/group)
        temporary=stage/'defence-intermediate';temporary.mkdir()
        result=defence.recover(ROOT/'Assets/Resources/Prefab/PlayerPrefab/FemaleModel.prefab',rig,capture,sword_body,temporary)
        append_defence(assets/'player-defence',temporary,stage/'player-defence')
        shutil.rmtree(temporary) # Only this exporter's scratch outputs.
        stats=validate(stage,assets/'player',assets/'player-defence')
        if previous!=pair.file_hashes(assets/'player-defence'):raise ValueError('Original defence changed')
        if source_hashes!={p:pair.sha(p) for p in sources}:raise ValueError('Input changed while composing')
        manifest=dict(schema='pe-starter-pair-unified-1',groups=list(GROUPS),body_clips=list(BODY),
                      defence_clips=list(DEFENCE+SWORD),counts=stats,defence_sword_maximum_point_error_m=result['maximum_point_error_m'],
                      files=pair.file_hashes(stage),sources=[dict(path=str(p),sha256=v) for p,v in source_hashes.items()],
                      existing_defence_files=previous,installed=False,shield_block_pose_recovered=False,
                      retained_body_prefix_clips=21,retained_defence_prefix_clips=30,source_pair_manifests=dict(normal=normal['schema'],sword=sword['schema']),
                      limitations=['Requires native PEAC clip limit64; file encoding and all other resource bounds unchanged.',
                                   'Defence shares each body clock by exact clip name; missing names fail closed.',
                                   'Original tool and Knife attachment groups are unchanged;31-body compatible-prefix validation is future host work.',
                                   'No gameplay activation, installation, masked shield block or draw/stow transitions.'])
        (stage/'unified-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');stage.rename(output)
    return manifest


def append_backward(base,pair_stage,body,capture,rig,output):
    """Only verified31/40 base plus exact two backward leaves; never install."""
    output=pair.fresh_output(output,[base,pair_stage,body,capture,rig,ROOT/'Assets'])
    old=read_json(base/'unified-manifest.json');before=pair.file_hashes(base)
    actual=dict(before);actual.pop('unified-manifest.json')
    if old['schema']!='pe-starter-pair-unified-1' or old['groups']!=list(GROUPS) or old['files']!=actual or old['installed'] or old['shield_block_pose_recovered']:
        raise ValueError('Invalid/changed unified31/40 source stage')
    validate(base)
    fresh=checked_stage(pair_stage,BACKWARD)
    if names(scene_clips(body/'recording.peas'))!=BACKWARD:raise ValueError('Exact backward body required')
    sources=[base/'unified-manifest.json',pair_stage/'starter-pair-manifest.json',body/'recording-manifest.json',capture,capture.with_suffix('.launch.json'),rig]
    hashes={p:pair.sha(p) for p in sources}
    if not any(Path(s['path']).resolve()==capture.resolve() and s['sha256']==hashes[capture] for s in fresh['sources']):
        raise ValueError('Backward stage did not use supplied capture')
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.starter-backward-',dir=output.parent) as directory:
        stage=Path(directory)
        append_scene(base/'player',body,stage/'player',BODY,BACKWARD)
        for group in pair.GROUPS:append_scene(base/group,pair_stage/group,stage/group,BODY,BACKWARD)
        temporary=stage/'defence-intermediate';temporary.mkdir()
        recovered=defence.recover(ROOT/'Assets/Resources/Prefab/PlayerPrefab/FemaleModel.prefab',rig,capture,body,temporary)
        append_defence(base/'player-defence',temporary,stage/'player-defence',DEFENCE+SWORD,BACKWARD)
        shutil.rmtree(temporary)
        stats=validate(stage,base/'player',base/'player-defence',backward=True)
        if before!=pair.file_hashes(base) or hashes!={p:pair.sha(p) for p in sources}:
            raise ValueError('Original stage/source changed during append')
        manifest=dict(schema='pe-starter-pair-unified-backward-1',groups=list(GROUPS),body_clips=list(BODY+BACKWARD),
            defence_clips=list(DEFENCE+SWORD+BACKWARD),counts=stats,
            defence_backward_maximum_point_error_m=recovered['maximum_point_error_m'],
            sources=[dict(path=str(p),sha256=h) for p,h in hashes.items()],files=pair.file_hashes(stage),
            base_stage=dict(path=str(base),manifest_sha256=hashes[base/'unified-manifest.json'],files=before),
            retained_body_prefix_clips=31,retained_defence_prefix_clips=40,
            installed=False,shield_block_pose_recovered=False,
            limitations=['Exact backward SwordWB441/SwordRB708 leaves appended; prior31/40 payloads and capsules preserved.',
                'No full controller blend graph, slope755, first-person Sword leaves or masked block pose.',
                'No runtime installation, gameplay activation or graphical claim.'])
        (stage/'unified-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');stage.rename(output)
    return manifest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--assets',type=Path,default=ROOT/'rebuild/build/reference-assets')
    p.add_argument('--normal-pair',type=Path);
    p.add_argument('--append-backward-to',type=Path,help='Existing qualified31/40 stage; Sword arguments then specify the separate two-clip backward group')
    p.add_argument('--sword-pair',type=Path,required=True)
    p.add_argument('--sword-body',type=Path,required=True);p.add_argument('--sword-capture',type=Path,required=True)
    p.add_argument('--rig',type=Path,default=ROOT/'rebuild/build/reference-player-starter/player-female-starter-rig.json')
    p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    try:
        if a.append_backward_to:
            if a.normal_pair:raise ValueError('Backward extension does not accept --normal-pair')
            r=append_backward(*[x.expanduser().resolve() for x in (a.append_backward_to,a.sword_pair,a.sword_body,a.sword_capture,a.rig)],a.output.expanduser())
        else:
            if not a.normal_pair:raise ValueError('Initial stage requires --normal-pair')
            r=export(*[x.expanduser().resolve() for x in (a.assets,a.normal_pair,a.sword_pair,a.sword_body,a.sword_capture,a.rig)],a.output.expanduser())
        print(f'Staged{r["counts"]["body_clips"]} body/wood clips; unified defence{r["counts"]["defence_clips"]} preserved; no install: {a.output}')
    except (OSError,ValueError,KeyError,TypeError,IndexError,ImportError) as error:p.exit(1,str(error)+'\n')


if __name__=='__main__':main()
