#!/usr/bin/env python3
"""Qualify entity-center inputs from the source player model subtrees, offline."""
from pathlib import Path
import argparse
import hashlib
import json
import math
import re

ROOT = Path(__file__).resolve().parents[2]
SOURCES = {
    'Assets/Scripts/Assist/PEUtil.cs': ['GetLocalColliderBoundsInChildren', 'obj.GetComponentsInChildren<Collider>()', 'if (!collider.isTrigger)', 'bounds.center = obj.transform.InverseTransformPoint(bounds.center);'],
    'Assets/Scripts/PeEntity/Cmpt/PeTrans.cs': ['PEUtil.GetLocalColliderBoundsInChildren(mModel.gameObject)', 'mTrans.TransformPoint(mLocalBounds.center)', 'SetModel(modelObj.transform);'],
    'Assets/Scripts/PeEntity/PeEntityPartial.cs': ['mPeTrans != null ? mPeTrans.center : Vector3.zero'],
    'Assets/Scripts/PeEntity/Cmpt/BiologyViewCmpt.cs': ['Entity.SendMsg(EMsg.View_Model_Build, monoModelCtrlr.gameObject, biologyViewRoot);', 'newObj.transform.parent = obj.transform;'],
    'Assets/Scripts/PeEntity/Cmpt/AvatarCmpt.cs': ['Prefab/PlayerPrefab/MaleModel', 'Prefab/PlayerPrefab/FemaleModel'],
    'Assets/Scripts/Attack/PEAttackTrigger.cs': ['null != m_PEEntity ? m_PEEntity.centerPos : Vector3.zero'],
}
META = {'model': 'Assets/Scripts/Assist/PEModelController.cs.meta',
        'view': 'Assets/Scripts/PeEntity/Cmpt/BiologyViewRoot.cs.meta'}

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def pointer(value):
    if not isinstance(value, dict) or value.get('m_FileID') != 0 or not isinstance(value.get('m_PathID'), int):
        raise ValueError('Expected local source pointer')
    return value['m_PathID']

def vector(value, keys='xyz'):
    result = [value[k] for k in keys]
    if not all(isinstance(x, (float, int)) and not isinstance(x, bool) and math.isfinite(x) for x in result):
        raise ValueError('Invalid source vector')
    return result

def identity(node):
    if vector(node['m_LocalPosition']) != [0, 0, 0] or vector(node['m_LocalScale']) != [1, 1, 1] or vector(node['m_LocalRotation'], 'xyzw') != [0, 0, 0, 1]:
        raise ValueError('Unsupported nonidentity player model transform')

def read_prefab(path):
    import UnityPy
    if path.stat().st_size > 16*1024*1024:
        raise ValueError('Oversized prefab')
    result = {}
    for obj in UnityPy.load(str(path)).objects:
        data = obj.read_typetree()
        guid = None
        if obj.type.name == 'MonoBehaviour':
            ref = data['m_Script']
            if not 1 <= ref['m_FileID'] <= len(obj.assets_file.externals):
                raise ValueError('Expected external source script')
            raw = bytes(obj.assets_file.externals[ref['m_FileID']-1].guid).hex()
            guid = ''.join(raw[i:i+2][::-1] for i in range(0, 32, 2))
        result[obj.path_id] = {'kind': obj.type.name, 'data': data, 'script_guid': guid}
    return result

def qualify(objects, sex, guids):
    def one(kind):
        matches = [(pid, obj['data']) for pid, obj in objects.items()
                   if obj['kind'] == 'MonoBehaviour' and obj.get('script_guid') == guids[kind]]
        if len(matches) != 1:
            raise ValueError('Expected one '+kind+' controller')
        if not matches[0][1]['m_Enabled']:
            raise ValueError('Disabled controller')
        return matches[0]
    model_id, model = one('model')
    view_id, view = one('view')
    if pointer(view['modelController']) != model_id:
        raise ValueError('View root does not bind qualified model controller')
    model_go = pointer(model['m_GameObject']); view_go = pointer(view['m_GameObject'])
    transforms = {pid: obj['data'] for pid, obj in objects.items() if obj['kind'] == 'Transform'}
    by_go = {pointer(t['m_GameObject']): pid for pid, t in transforms.items()}
    if len(by_go) != len(transforms):
        raise ValueError('Duplicate GameObject transforms')
    model_trans, view_trans = by_go[model_go], by_go[view_go]
    if pointer(transforms[model_trans]['m_Father']) != view_trans or pointer(transforms[view_trans]['m_Father']):
        raise ValueError('Unexpected model/view hierarchy')
    for tid in (model_trans, view_trans):
        identity(transforms[tid])
        if not objects[pointer(transforms[tid]['m_GameObject'])]['data']['m_IsActive']:
            raise ValueError('Inactive model/view root')
    if objects[model_go]['data']['m_Name'] != 'Model' or objects[view_go]['data']['m_Name'] != sex+'Model':
        raise ValueError('Unexpected source model name')
    def under(go):
        seen = set(); tid = by_go[go]
        while tid:
            if tid in seen or tid not in transforms:
                raise ValueError('Cyclic or missing transform')
            seen.add(tid)
            if tid == model_trans:
                return True
            tid = pointer(transforms[tid]['m_Father'])
        return False
    solid = []; excluded = []; triggers = []
    for pid, obj in objects.items():
        if not obj['kind'].endswith('Collider'):
            continue
        data = obj['data']
        if not under(pointer(data['m_GameObject'])):
            if not data['m_IsTrigger']:
                excluded.append(pid)
            continue
        if data['m_IsTrigger']:
            triggers.append(pid); continue
        solid.append((pid, obj))
    if len(solid) != 1:
        raise ValueError('Model subtree must have exactly one nontrigger collider')
    collider_id, obj = solid[0]; col = obj['data']
    if obj['kind'] != 'CapsuleCollider' or pointer(col['m_GameObject']) != model_go or not col['m_Enabled'] or col['m_Direction'] != 1:
        raise ValueError('Expected enabled model-root Y capsule')
    if collider_id not in [pointer(x) for x in model['m_Colliders']]:
        raise ValueError('Model controller omits root capsule')
    center = vector(col['m_Center']); radius, height = col['m_Radius'], col['m_Height']
    if not all(isinstance(x, (int,float)) and math.isfinite(x) and x > 0 for x in [radius,height]) or height < 2*radius:
        raise ValueError('Invalid capsule dimensions')
    if center != [0, .875, 0] or height != 1.75 or abs(radius-.37) > 1e-7:
        raise ValueError('Supported player root capsule changed')
    return {'sex': sex.lower(), 'model_controller': model_id, 'view_root': view_id,
            'model_transform': model_trans, 'collider': collider_id,
            'local_center': center, 'radius': radius, 'height': height,
            'ignored_trigger_colliders': sorted(triggers),
            'nontrigger_colliders_outside_model': sorted(excluded),
            'interpretation': 'Only Model subtree is passed to PeTrans.InitBound; sibling ragdoll/OutCollider geometry is excluded.'}

def build(root):
    sources = []; guids = {}
    for path, tokens in SOURCES.items():
        data = (root/path).read_text(errors='replace')
        if not all(token in data for token in tokens):
            raise ValueError('Source center contract changed: '+path)
        sources.append({'path': path, 'sha256': sha(root/path)})
    for name, path in META.items():
        match = re.search(r'^guid: ([0-9a-f]{32})$', (root/path).read_text(), re.M)
        if not match:
            raise ValueError('Missing source script GUID')
        guids[name] = match[1]; sources.append({'path': path, 'sha256': sha(root/path)})
    actors = []
    for sex in ['Female', 'Male']:
        path = f'Assets/Resources/Prefab/PlayerPrefab/{sex}Model.prefab'
        actors.append(qualify(read_prefab(root/path), sex, guids))
        sources.append({'path': path, 'sha256': sha(root/path)})
    return {'schema': 'pe-player-collider-center-1', 'sources': sources, 'actors': actors,
            'limits': ['Source-prefab supported living model at unit scale; no live component capture.',
                       'World center must use the actual entity position/yaw/scale.',
                       'No ragdoll, altered collider hierarchy, arbitrary scaling, or incomplete model bounds qualification.']}

def header(report):
    rows = []
    for actor in report['actors']:
        center = ', '.join(f'{float(x):.9g}f' if '.' in f'{float(x):.9g}' else f'{float(x):.9g}.0f' for x in actor['local_center'])
        rows.append('    {{'+center+'}}')
    return ('/* Generated by export_player_collider_center.py; do not edit. */\n'
            '#ifndef PE_PLAYER_COLLIDER_CENTER_SOURCE_H\n#define PE_PLAYER_COLLIDER_CENTER_SOURCE_H\n'
            'typedef struct { float local_center[3]; } PePlayerColliderCenterSource;\n'
            '/* Female=0, Male=1; source Model nontrigger collider-bounds center. */\n'
            'static const PePlayerColliderCenterSource pe_player_collider_center_source[2] = {\n'+',\n'.join(rows)+'\n};\n#endif\n')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=ROOT)
    parser.add_argument('--output',type=Path,default=ROOT/'rebuild/generated')
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args();report=build(args.root)
    outputs={'pe_player_collider_center_source.json':json.dumps(report,indent=2,sort_keys=True)+'\n',
             'pe_player_collider_center_source.h':header(report)}
    for name, data in outputs.items():
        path=args.output/name
        if args.check:
            if not path.is_file() or path.read_text()!=data:
                raise SystemExit('Stale generated player center: '+str(path))
        else:
            args.output.mkdir(parents=True,exist_ok=True);path.write_text(data)
    print('Player collider center '+('check passed' if args.check else 'exported')+': female/male (0, .875, 0)')

if __name__ == '__main__':
    main()
