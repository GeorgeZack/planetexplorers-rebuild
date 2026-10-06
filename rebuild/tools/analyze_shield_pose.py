#!/usr/bin/env python3
"""Read-only simultaneous source shield/base-layer evidence, never pose export.

Runs the established strict hierarchy/rig analyzer before classifying frames.
A captured pose is observational evidence, not a qualified authored loop.
"""
import argparse
import json
from pathlib import Path

from analyze_reference_pose import analyze, decode, MAX_LINE, MAX_BYTES, finite_number
from inventory_shield_layers import sha, MASK

GAITS = ('AttackIdle', 'SwordWF', 'SwordRF')
BLOCK_STATE = 439445811


def u32(value):
    return value & 0xffffffff


def positive_clips(layer):
    # Unity reports unused blend-tree leaves at zero. Preserve actual positive
    # contributors, including tiny weights; only qualification uses tolerance.
    return [{'name':c['name'], 'id':c['id'], 'weight':c['weight'], 'length':c['length']}
            for c in layer['currentClips'] if c['weight'] > 0.]


def classify(frame):
    if frame['gender'] != 'Female':
        raise ValueError('Shield capture must be female')
    matches = [a for a in frame['animators'] if a['enabled'] and a['activeInHierarchy']
               and a['avatar']=='FemaleAvatar' and a['controller']=='FemaleAnimator2']
    if len(matches)!=1:
        raise ValueError('Expected one active source female Animator')
    a, = matches
    if len(a['layers'])<4 or a['layers'][0]['name']!='Base Layer' or a['layers'][3]['name']!='LArmOnly':
        raise ValueError('Source layer ordering differs')
    base, block = a['layers'][0], a['layers'][3]
    p = a['parameters']
    if type(p.get('SheildBlock')) is not bool:
        raise ValueError('Missing shield boolean')
    style = finite_number(p['MoveStyle'], 'MoveStyle')
    base_clips, block_clips = positive_clips(base), positive_clips(block)
    overlays = [{'index':index, 'name':layer['name'], 'weight':layer['weight'],
                 'current_clips':positive_clips(layer),
                 'next_clips':[c for c in layer['nextClips'] if c['weight']>0.]}
                for index,layer in enumerate(a['layers']) if index not in (0,3) and layer['weight']>1e-4
                and (positive_clips(layer) or (layer['inTransition'] and any(c['weight']>0. for c in layer['nextClips'])))]
    # These strict steady-frame labels supplement all raw clip/state spans.
    # They never force partially blended walking into a fake source leaf.
    block_weight = sum(c['weight'] for c in block_clips if c['name']=='SwordSheildBlock')
    other_block_weight = sum(c['weight'] for c in block_clips if c['name']!='SwordSheildBlock')
    stable_block = (p['SheildBlock'] and abs(style-1.)<=1e-4 and
                    not block['inTransition'] and u32(block['current']['fullPathHash'])==BLOCK_STATE and
                    abs(block['weight']-1.)<=1e-4 and block_weight>=.9999 and other_block_weight<=1e-4)
    gait = None
    if stable_block and not base['inTransition'] and not overlays:
        for name in GAITS:
            own = sum(c['weight'] for c in base_clips if c['name']==name)
            others = sum(c['weight'] for c in base_clips if c['name']!=name)
            if own>=.9999 and others<=1e-4:
                gait=name
    return {'animator_id':a['id'], 'animator_path':a['path'], 'shield_bool':p['SheildBlock'],
            'move_style':style, 'steady_sword_block':bool(stable_block), 'steady_composition':gait,
            'base_clips':base_clips, 'block_clips':block_clips,
            'other_active_layers':overlays,
            'base_state':u32(base['current']['fullPathHash']), 'block_state':u32(block['current']['fullPathHash']),
            'block_layer_weight':block['weight'], 'base_transition':base['inTransition'],
            'block_transition':block['inTransition'],
            'base_normalized_time':base['current']['normalizedTime'],
            'block_normalized_time':block['current']['normalizedTime']}


def report(capture, rig_path, plan_path):
    if not capture.is_file() or capture.stat().st_size>MAX_BYTES:
        raise ValueError('Missing or oversized capture')
    capture_hash=sha(capture)
    plan=decode(plan_path.read_bytes())
    if plan.get('schema')!='pe-shield-layer-plan-1' or plan['layer_index']!=3 or plan['layer']['m_BodyMask']!=MASK:
        raise ValueError('Wrong source layer plan')
    asset=Path(plan['asset'])
    if sha(asset)!=plan['asset_sha256']:
        raise ValueError('Source asset changed after inventory')
    rig=decode(rig_path.read_bytes())
    structural,_=analyze(capture,rig)
    if structural['rig_mapping']['missing_bones']:
        raise ValueError('Missing source rig bones')
    counts={g:0 for g in GAITS};examples={};edges=[];last=None;steady=0;frames=0;end=None
    phases=[];current=None;equipment_paths=set()
    with capture.open('rb') as stream:
        while line:=stream.readline(MAX_LINE+1):
            if len(line)>MAX_LINE: raise ValueError('Oversized line')
            if stream.tell()>MAX_BYTES: raise ValueError('Capture byte bound exceeded')
            row=decode(line)
            if row['type']=='end':end=row;continue
            if row['type']!='frame':continue
            evidence=classify(row);frames+=1
            if evidence['steady_sword_block']:steady+=1
            if evidence['steady_composition']:
                name=evidence['steady_composition'];counts[name]+=1
                examples.setdefault(name,{'sample':row['sample'],'frame':row['frame'],**evidence})
            if last is None or evidence['shield_bool']!=last:
                edges.append({'sample':row['sample'],'frame':row['frame'],'time':row['time'],
                              'realtime':row['realtime'],'SheildBlock':evidence['shield_bool']})
            last=evidence['shield_bool']
            signature=(evidence['base_state'],evidence['block_state'],evidence['base_transition'],
                       evidence['block_transition'],evidence['shield_bool'],evidence['steady_composition'])
            if current is None or signature!=current['_signature']:
                current={'_signature':signature,'first_sample':row['sample'],'last_sample':row['sample'],
                         'first':evidence,'last':evidence};phases.append(current)
            else:current['last_sample']=row['sample'];current['last']=evidence
            for t in row['transforms']:
                if t['activeInHierarchy'] and t['path'].rsplit('/',1)[-1] in ('wood_shield','wood_shield(Clone)','wood_sword','wood_sword(Clone)'):
                    equipment_paths.add(t['path'])
    if frames!=structural['frames'] or end is None or end['reason'] not in ('capture-limit','stop-file','application-quit'):
        raise ValueError('Capture ended incompletely or with an error')
    if sha(capture)!=capture_hash:
        raise ValueError('Capture changed while analyzing')
    for phase in phases:del phase['_signature']
    manifest_path=capture.with_suffix('.launch.json')
    if not manifest_path.is_file():raise ValueError('Launch manifest missing')
    manifest=decode(manifest_path.read_bytes())
    if Path(manifest['capture']).resolve()!=capture.resolve():raise ValueError('Launch capture path differs')
    return {'schema':'pe-shield-live-evidence-1','capture':str(capture),'capture_sha256':capture_hash,
            'launch_manifest_sha256':sha(manifest_path),'source_plan_sha256':sha(plan_path),
            'source_asset_sha256':plan['asset_sha256'],'rig_sha256':sha(rig_path),
            'complete':True,'end':end,'frames':frames,'steady_sword_block_frames':steady,
            'steady_composition_frames':counts,'all_three_steady_compositions_observed':all(counts.values()),
            'examples':examples,'shield_boolean_edges':edges,'phases':phases,
            'active_named_equipment_paths':sorted(equipment_paths),
            'mask_evidence':'source-bound controller layer; mask is not introspected by live observer',
            'layer_source':plan['layer'],'source_block_state':BLOCK_STATE,
            'structural_validation':structural,'runtime_pose_export_ready':False,
            'limitations':['Observer records evaluated live poses including IK/transitions, not pure authored loops.',
                           'Named attachment paths are visual evidence, not an inventory-instance or buff/attribute trace.',
                           'Launch manifest has no at-launch asset hash; current installed source identity is separately recorded.',
                           'Original responsiveness and teardown require separate operator evidence.']}


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('capture',type=Path)
    p.add_argument('--rig',type=Path,default=Path('rebuild/build/reference-player-starter/player-female-starter-rig.json'))
    p.add_argument('--plan',type=Path,default=Path('rebuild/build/shield-layer/plan.json'))
    p.add_argument('--report',required=True,type=Path)
    a=p.parse_args(argv)
    try:
        if a.report.resolve() in {p.resolve() for p in (a.capture,a.rig,a.plan,a.capture.with_suffix('.launch.json'))}:
            raise ValueError('Report must not overwrite an input')
        result=report(a.capture,a.rig,a.plan)
        a.report.parent.mkdir(parents=True,exist_ok=True)
        a.report.write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
    except (OSError,ValueError,KeyError,IndexError) as e:p.error(str(e))
    print(json.dumps({k:result[k] for k in ('frames','steady_sword_block_frames','steady_composition_frames','all_three_steady_compositions_observed')}))


if __name__=='__main__':main()
