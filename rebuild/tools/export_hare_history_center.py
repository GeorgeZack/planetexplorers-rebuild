#!/usr/bin/env python3
"""Qualify the hare PeTrans collider-bounds center; no Unity evaluation/writes."""
import argparse
import hashlib
import json
from pathlib import Path
import re
from export_reference_actor import BUNDLE, MODEL_ROOT_ID
from export_reference_assets import pointer

ROOT=Path(__file__).resolve().parents[2]
OUTPUT=ROOT/'rebuild/generated'
CONTROLLER=3316048974275031591
VIEW_ROOT=420381330858334883
MODEL=3647756363348019165
CAPSULE=6789482777293099034
COLLIDER_TRANSFORM=8416349922905001328
SOURCES={
 'Assets/Scripts/PeEntity/Cmpt/BiologyViewCmpt.cs':['Entity.SendMsg(EMsg.View_Model_Build,monoModelCtrlr.gameObject,biologyViewRoot);'],
 'Assets/Scripts/PeEntity/Cmpt/PeTrans.cs':['mLocalBounds=PETools.PEUtil.GetLocalColliderBoundsInChildren(mModel.gameObject);','returnmTrans.TransformPoint(mLocalBounds.center);','SetModel(modelObj.transform);'],
 'Assets/Scripts/Assist/PEUtil.cs':['Collider[]colliders=obj.GetComponentsInChildren<Collider>();','if(!collider.isTrigger)','bounds.center=obj.transform.InverseTransformPoint(bounds.center);'],
 'Assets/Scripts/Attack/PEAttackTrigger.cs':['m_PEEntity.centerPos'],
}

def identity(node):
    return node['m_LocalPosition']==dict(x=0.,y=0.,z=0.) and node['m_LocalRotation']==dict(x=0.,y=0.,z=0.,w=1.) and node['m_LocalScale']==dict(x=1.,y=1.,z=1.)

def qualify(objects):
    transforms={i:t for i,(kind,t) in objects.items() if kind=='Transform'}
    def within(i):
        seen=set()
        while i:
            if i in seen or i not in transforms:raise ValueError('Invalid hierarchy')
            if i==MODEL:return True
            seen.add(i);i=pointer(transforms[i]['m_Father'])
        return False
    controller=objects[CONTROLLER][1]
    view=objects[VIEW_ROOT][1]
    for node,name in ((controller,'PEModelController'),(view,'BiologyViewRoot')):
        script=objects.get(pointer(node['m_Script']))
        if not script or script[0]!='MonoScript' or script[1]['m_ClassName']!=name:raise ValueError('Model binding script changed')
    if pointer(view['modelController'])!=CONTROLLER:raise ValueError('Live view model binding changed')
    if pointer(controller['m_GameObject'])!=pointer(transforms[MODEL]['m_GameObject']):raise ValueError('Model controller owner changed')
    if pointer(transforms[MODEL_ROOT_ID]['m_Father'])!=MODEL:raise ValueError('Actor/model hierarchy changed')
    for tid in (MODEL,MODEL_ROOT_ID,COLLIDER_TRANSFORM):
        if not identity(transforms[tid]):raise ValueError('Nonidentity center hierarchy requires new derivation')
        if not objects[pointer(transforms[tid]['m_GameObject'])][1]['m_IsActive']:raise ValueError('Inactive center hierarchy')
    if pointer(transforms[COLLIDER_TRANSFORM]['m_Father'])!=MODEL:raise ValueError('Movement capsule reparented')
    by_go={pointer(t['m_GameObject']):i for i,t in transforms.items()}
    solid=[];excluded=[];triggers=[]
    for i,(kind,t) in objects.items():
        if kind not in ('CapsuleCollider','SphereCollider','BoxCollider','MeshCollider','TerrainCollider','CharacterController'):continue
        tid=by_go.get(pointer(t['m_GameObject']))
        if tid is None:raise ValueError('Collider transform missing')
        if not within(tid):excluded.append(i);continue
        if t.get('m_IsTrigger',False):triggers.append(i);continue
        solid.append(i)
    if solid!=[CAPSULE]:raise ValueError('Model must have exactly the qualified solid capsule')
    kind,c=objects[CAPSULE]
    if kind!='CapsuleCollider' or not c['m_Enabled'] or c['m_IsTrigger'] or c['m_Direction']!=1 or c['m_Center']!=dict(x=0.,y=.75,z=0.) or c['m_Height']!=1.5 or abs(c['m_Radius']-.6)>1e-7:raise ValueError('Authored movement capsule changed')
    if pointer(c['m_GameObject'])!=pointer(transforms[COLLIDER_TRANSFORM]['m_GameObject']):raise ValueError('Capsule owner changed')
    if CAPSULE not in [pointer(x) for x in controller['m_Colliders']]:raise ValueError('Model capsule membership changed')
    return dict(local_center=[0.,.75,0.],model_transform=MODEL,actor_transform=MODEL_ROOT_ID,model_controller=CONTROLLER,view_root=VIEW_ROOT,
        capsule=CAPSULE,collider_transform=COLLIDER_TRANSFORM,solid_colliders=solid,excluded_other_subtree_colliders=sorted(excluded),excluded_triggers=sorted(triggers))

def recover(bundle,root=ROOT):
    import UnityPy
    objects={o.path_id:(o.type.name,o.read_typetree()) for o in UnityPy.load(str(bundle)).objects if o.type.name in ('Transform','GameObject','MonoBehaviour','MonoScript','CapsuleCollider','SphereCollider','BoxCollider','MeshCollider','TerrainCollider','CharacterController')}
    result=qualify(objects);result['schema']='pe-hare-history-center-1';result['source_bundle']=BUNDLE;result['source_sha256']=hashlib.sha256(bundle.read_bytes()).hexdigest()
    result['sources']=[]
    for name,guards in SOURCES.items():
        data=(root/name).read_bytes();text=re.sub(r'\s+','',data.decode('utf-8-sig'))
        if any(guard not in text for guard in guards):raise ValueError('Source binding changed: '+name)
        result['sources'].append(dict(path=name,sha256=hashlib.sha256(data).hexdigest()))
    result['scope']='Source-derived live model collider center; ragdoll is a sibling, triggers excluded. Uniform runtime scale and entity yaw apply at query time. No live measurement.'
    return result

def render(report):
    x,y,z=report['local_center']
    return '/* Generated by export_hare_history_center.py; source capsule %s. */\n#ifndef PE_HARE_HISTORY_CENTER_SOURCE_H\n#define PE_HARE_HISTORY_CENTER_SOURCE_H\nstatic const float pe_hare_history_local_center[3]={%.9ff,%.9ff,%.9ff};\n#endif\n'%(report['capsule'],x,y,z)

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--bundle',type=Path,default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers'/BUNDLE);ap.add_argument('--output',type=Path,default=OUTPUT);ap.add_argument('--check',action='store_true');a=ap.parse_args()
    report=recover(a.bundle);outputs={'pe_hare_history_center_source.json':json.dumps(report,indent=2,sort_keys=True)+'\n','pe_hare_history_center_source.h':render(report)}
    for name,data in outputs.items():
        path=a.output/name
        if a.check:
            if not path.is_file() or path.read_text()!=data:raise SystemExit('Mismatch: '+str(path))
        else:a.output.mkdir(parents=True,exist_ok=True);path.write_text(data)
    print('hare entity center: (0, 0.75, 0); source/check OK')
if __name__=='__main__':main()
