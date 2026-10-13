#!/usr/bin/env python3
"""Qualify installed female AnimFollow drive profiles and ordered rig; no execution."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
import export_animfollow as base
import cil_evidence
from export_content import ROOT, describe_source
from export_hit_reaction import f

PROFILES=('PTorqueProfile','PForceProfile','maxTorqueProfile','maxForceProfile',
          'maxJointTorqueProfile','jointDampingProfile','forceErrorWeightProfile')
SCALARS=('PTorque','DTorque','PForce','DForce','jointDamping','maxTorque','maxForce','maxJointTorque','drag','angularDrag')

def require(value,message):
    if not value:raise ValueError(message)

def finite_tree(value):
    if isinstance(value,float) and not math.isfinite(value):
        require(not math.isnan(value),'NaN installed joint setting')
        return '+Infinity' if value>0 else '-Infinity'
    if isinstance(value,dict):return {k:finite_tree(v) for k,v in value.items()}
    if isinstance(value,list):return [finite_tree(v) for v in value]
    return value

def recover_rig(values,assets):
    file=assets.file('resources.assets');masters=values['masterTransforms'];slaves=values['slaveTransforms']
    require(len(masters)==len(slaves)==77,'Pinned female transform correspondence changed')
    require(not values['slaveExcludeTransforms'],'Unexpected excluded female transforms')
    refs=values['slaveRigidbodies'];require(len(refs)==11,'Pinned female body count changed')
    def local(ref):
        require(ref['file']==0 and ref['id'] in file.objects,'Missing/nonlocal rig reference')
        return file.objects[ref['id']]
    by_transform={}
    for ref in refs:
        body=local(ref);require(body.type.name=='Rigidbody','Body reference has wrong class')
        raw=body.read_typetree();go=assets.resolve(body,raw['m_GameObject']);transforms=assets.components(go,'Transform')
        require(len(transforms)==1,'Body requires one transform')
        require(transforms[0].path_id not in by_transform,'Duplicate body transform')
        by_transform[transforms[0].path_id]=(body,go)
    rows=[];nonrigids=[]
    for slot,(master,slave) in enumerate(zip(masters,slaves)):
        mt=local(master);st=local(slave)
        require(mt.type.name==st.type.name=='Transform','Invalid rig transform pair')
        mtree=mt.read_typetree();stree=st.read_typetree();go=assets.resolve(st,stree['m_GameObject']);g=go.read_typetree()
        # Awake admits listed bodies for activeSelf objects and GetComponent
        # Rigidbody for inactive objects. Reject a changed qualification branch.
        if g['m_IsActive']:is_rigid=st.path_id in by_transform
        else:is_rigid=bool(assets.components(go,'Rigidbody'))
        if not is_rigid:
            nonrigids.append(dict(slot=slot,master=mt.path_id,slave=st.path_id,name=g['m_Name']))
            continue
        require(st.path_id in by_transform,'Inactive body missing from serialized body owner')
        body,body_go=by_transform[st.path_id];require(go.path_id==body_go.path_id,'Body/transform owner differs')
        joints=assets.components(go,'ConfigurableJoint');require(len(joints)<2,'Ambiguous configurable joint')
        index=len(rows);require(index==0 or len(joints)==1,'Nonroot body lacks source-required joint')
        joint=joints[0] if joints else None
        row=dict(index=index,slot=slot,name=g['m_Name'],master_transform=mt.path_id,slave_transform=st.path_id,
                 game_object=go.path_id,rigidbody=body.path_id,active_self=g['m_IsActive'],
                 tag=g['m_Tag'],layer=g['m_Layer'],colliders=[],
                 body=body.read_typetree(),master_local_rotation=mtree['m_LocalRotation'],
                 slave_local_rotation=stree['m_LocalRotation'],joint=None)
        for kind in ('BoxCollider','SphereCollider','CapsuleCollider'):
            for collider in assets.components(go,kind):
                tree=collider.read_typetree();material=tree['m_Material'];resolved=None
                if material['m_PathID']:
                    obj=assets.resolve(collider,material)
                    require(obj.assets_file.name in base.ASSETS,'Unqualified external physics material')
                    require(obj.type.name=='PhysicMaterial','Wrong collider material class')
                    resolved=dict(file=obj.assets_file.name,id=obj.path_id,data=finite_tree(obj.read_typetree()),raw_sha256=hashlib.sha256(obj.get_raw_data()).hexdigest())
                row['colliders'].append(dict(kind=kind,id=collider.path_id,data=finite_tree(tree),material=resolved,raw_sha256=hashlib.sha256(collider.get_raw_data()).hexdigest()))
        require(len(row['colliders'])==1,'Pinned body requires one source collider')
        if joint:row['joint']=dict(id=joint.path_id,data=finite_tree(joint.read_typetree()),raw_sha256=hashlib.sha256(joint.get_raw_data()).hexdigest())
        row['body_raw_sha256']=hashlib.sha256(body.get_raw_data()).hexdigest();rows.append(row)
    require([r['rigidbody'] for r in rows]==[r['id'] for r in refs],'Awake traversal/body-array order differs')
    require(len(nonrigids)==66 and rows[0]['joint'] is None,'Pinned female rig/root changed')
    for name in PROFILES:
        require(len(values[name])==12 and all(math.isfinite(v) and v>=0 for v in values[name]),'Invalid installed profile '+name)
    for name in SCALARS:require(math.isfinite(values[name]) and values[name]>=0,'Invalid drive scalar '+name)
    return rows,nonrigids

def build_outputs(root=ROOT,data=base.DATA,managed=base.MANAGED):
    evidence=base.load_evidence(root);cil_evidence.qualify_installed(evidence,base.ASSEMBLIES,managed)
    components=base.recover_components(evidence,data);values=components['AnimFollow_AF']['values']
    rows,nonrigids=recover_rig(values,base.Assets(data));n=len(rows)
    header='/* Generated by export_animfollow_drive.py; do not edit. */\n#ifndef PE_ANIMFOLLOW_DRIVE_SOURCE_H\n#define PE_ANIMFOLLOW_DRIVE_SOURCE_H\n'
    header+='#define PE_AF_DRIVE_SOURCE_BODIES %du\n'%n
    header+='typedef struct {unsigned master,slave,body,joint;float '+','.join(PROFILES)+';} PeAfDriveSourceBody;\n'
    header+='static const PeAfDriveSourceBody pe_af_drive_source_bodies[]={\n'
    for r in rows:
        header+='{'+','.join(str(r[k]) for k in ('master_transform','slave_transform','rigidbody'))+','+str(r['joint']['id'] if r['joint'] else 0)+','+','.join(f(values[p][r['index']]) for p in PROFILES)+'},\n'
    header+='};\n'
    for name in SCALARS:header+='static const float pe_af_drive_source_%s=%s;\n'%(name,f(values[name]))
    last=rows[-1]['joint']['data']['m_SlerpDrive']
    header+='static const float pe_af_drive_source_initial_damper=%s;\n'%f(last['positionDamper'])
    header+='static const float pe_af_drive_source_maximum_force=%s;\n'%f(last['maximumForce'])
    header+='static const unsigned pe_af_drive_source_nonrigids=%du;\n'%len(nonrigids)
    header+='#endif\n'
    metadata=dict(schema=1,evidence=describe_source(root/base.EVIDENCE),rea_evidence_id=evidence['assemblies']['Assembly-CSharp.dll']['rea_evidence_id'],
        artifact_sha256=base.ASSEMBLIES['Assembly-CSharp.dll'][0],methods_digest=base.METHODS_DIGEST,
        installed_sources=[dict(path='installed/PE_Client_Data/'+name,sha256=sha) for name,sha in base.ASSETS.items()],
        source_component=dict(id=324352,raw_sha256=base.BODY_DIGESTS['AnimFollow_AF']),
        scalars={k:values[k] for k in SCALARS},serialized_profiles={k:values[k] for k in PROFILES},
        profile_resize='Awake Array.Resize to11 bodies retains first11 entries; trailing twelfth is discarded.',
        rigid_bodies=rows,nonrigid_pairs=nonrigids,
        requests=dict(force_mode=2,joint_drive_mode=1,limits_enabled_motion=1,limits_disabled_motion=2,
            pd='signal = P * (error + (D * (error - lastError)) * reciprocalFixedDeltaTime); lastError = error',
            angular='FixEuler subtracts360 once only if angle>180; exact angle360 clears signal and preserves lastError.',
            target_rotation='inverse(localToJointSpace) * inverse(master.localRotation) * startLocalRotation',
            start_local_rotation='slave.localRotation * localToJointSpace; localToJointSpace = LookRotation(Cross(axis,secondaryAxis), secondaryAxis)',
            sync='For each body, only if body and master transform exist: copy master world position then rotation to body transform; catch throws a new exception named after this object.',
            nonrigid='On secondary tick, refresh indices2 through count-2 when mimicNonRigids. On even frameCounter, write those indices regardless of mimicNonRigids; Quaternion.Lerp factor2/secondaryUpdate only when secondaryUpdate>2.',
            controller_order='DoRagdollControl precedes active test; active drive resets totals and increments frameCounter only after drive.'),
        scope='Disconnected numerical drive and indexed joint requests. Rig metadata is not a physics implementation.',
        limitations=evidence['limitations']+['Engine Quaternion/Vector3 operations and actual Rigidbody/ConfigurableJoint consumption are not executed or qualified bit-exact.',
            'Force mode2 and joint motion/drive mode integers are retained literally; no mass-dependent engine solve is supplied.',
            'All component bytes are bound by whole installed asset hashes; Infinity joint break values are represented as explicit strings.',
            'Collider material references are retained; a null material uses an unqualified engine default. Numeric GameObject tags are not resolved to tag names.'])
    return {'pe_animfollow_drive_source.h':header.encode(),'pe_animfollow_drive_source.json':(json.dumps(metadata,indent=2,allow_nan=False)+'\n').encode()}

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');p.add_argument('--output',type=Path,default=ROOT/'rebuild/generated');a=p.parse_args(argv)
    try:
        for name,data in build_outputs().items():
            if a.check:require((a.output/name).is_file() and (a.output/name).read_bytes()==data,'Stale or missing '+name)
            else:a.output.mkdir(parents=True,exist_ok=True);(a.output/name).write_bytes(data)
    except (OSError,ValueError,KeyError,ImportError,IndexError) as error:print(error,file=sys.stderr);return 1
    return 0
if __name__=='__main__':raise SystemExit(main())
