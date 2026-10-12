#!/usr/bin/env python3
"""Recover the female player's HumanPhyCtrl request/force contract without starting Unity.

HumanPhyCtrl has no checked-in C# source. Its rules are read from the installed
Assembly-CSharp CIL, bound by artifact and method-body hashes to a committed
static excerpt, and combined with the serialized female model and project
physics settings.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

import cil_disasm
from export_content import ROOT, describe_source
from export_capsule_history import DEFAULT_UNITY, UNITY_SHA256
from export_hit_reaction import MODEL, f

MANAGED = DEFAULT_UNITY.parent
EVIDENCE = 'docs/reference/human-phy-ctrl-il.json'
TIME = 'ProjectSettings/TimeManager.asset'
DYNAMICS = 'ProjectSettings/DynamicsManager.asset'
SOURCES = (MODEL, TIME, DYNAMICS, EVIDENCE)
ASSEMBLIES = {
    'Assembly-CSharp.dll': ('56d90949e732331be7644fc9185dc088c78ad4888a100f60d96b7aa093f4d7d3',
                            '7ddcc464-5353-4750-b969-fe628ee474da'),
    'Assembly-CSharp-firstpass.dll': ('9fb0f052d0458840220be7cde5f1696ab3e1502154ba9cbcd826c1f3dbe399df',
                                      '7b178142-9afb-4aa1-ad2b-b1f8d0d56dee'),
    'UnityEngine.dll': (UNITY_SHA256, '5cf16eb2-32c9-483e-8fb7-9072104d5826'),
}
# Bodies whose reading the C owner depends on. The combined digest below covers
# every excerpted method, including the trivial accessors.
METHOD_IL = {
    'HumanPhyCtrl::.ctor': '4c68dce5b0a5bff0c6561a08044f0df1f4e80eb8e9a6328cd0747eb556e22ffa',
    'HumanPhyCtrl::FixedUpdate': '82f0f6f4263f091c',
    'HumanPhyCtrl::Update': '233879b1bf2be2b5',
    'HumanPhyCtrl::LateUpdate': '31c241bbe124867e',
    'HumanPhyCtrl::UpdateGroundState': '827acd9c75603b52',
    'HumanPhyCtrl::UpdateWaterState': 'e6b4f5f78e65808f',
    'HumanPhyCtrl::UpdateDrag': '5da3597a8c193ece',
    'HumanPhyCtrl::CheckGround': '7f9d4b36d71bcd50',
    'HumanPhyCtrl::AddCollision': '89ff1373a479dd1e',
    'HumanPhyCtrl::ApplyMoveRequest': '9d0a5d911f272b2a6ac95fbdffb271fa932c7da495b9152a876b1a337814bf60',
    'HumanPhyCtrl::CancelMoveRequest': 'bc911732a244c54d',
    'HumanPhyCtrl::ApplyImpact': '0bc1216e88ced71d',
    'HumanPhyCtrl::ResetSpeed': '5a0106ca75a5694a',
    'HumanPhyCtrl::get_desiredVelocity': 'f95e0f958cc8093b',
    'HumanPhyCtrl::get_impacting': '47b3bdcfe1fe2f9e',
    'HumanPhyCtrl::set_velocity': 'f8880c50921c25cf',
    'Pathea.Action_Repulsed::Update': '32ae79858841bf5c',
    'Util::ProjectOntoPlane': 'ca3550f70111a10dcd20681ddadad491560167a1fc712b598e401885374afe9f',
    'UnityEngine.Vector3::op_Equality': 'ae0d16da6689',
    'UnityEngine.Vector3::Normalize': '42f7bb97933b393992990f5ad60c0edcda1ba770de0674c566d2f1ed18f58b25',
    'UnityEngine.Vector3::Project': 'c82c8c3f2cb2',
}
EXCERPT = {
    'Assembly-CSharp.dll': lambda m: m['declaring_type']=='HumanPhyCtrl' or (
        m['declaring_type']=='Pathea.Action_Repulsed' and m['name'] in ('Update','EndImmediately')),
    'Assembly-CSharp-firstpass.dll': lambda m: m['declaring_type']=='Util' and m['name']=='ProjectOntoPlane',
    'UnityEngine.dll': lambda m: (m['declaring_type']=='UnityEngine.Vector3' and m['name'] in (
        'op_Equality','op_Inequality','Normalize','get_normalized','Magnitude','get_magnitude','SqrMagnitude',
        'get_sqrMagnitude','Dot','Project','ProjectOnPlane','Angle')) or (
        m['declaring_type']=='UnityEngine.Mathf' and m['name'] in ('Lerp','Clamp01')),
}
METHODS_DIGEST = 'bbf00d2c9ab6a324563c9407047043bcbd737533997f3318468ffd27e4bb6625'
CTOR_DEFAULTS = {
    'mSpeedTimes': 1.0, 'm_CheckGroundRadius': 0.30000001192092896, 'm_CheckGroundOffsetHeight': 0.25,
    'm_HuamnHeight': 1.75, 'm_SpineHeight': 1.2999999523162842, 'm_FeetHeight': 1.0,
    'm_SpineFixedHeight': 0.05000000074505806, 'm_StopSpeed': 0.10000000149011612, 'm_MoveSpeed': 5.0,
    'm_FlotageAcc': 12.0, 'm_FlotageMaxSpeed': 1.0, 'm_GroundDrag': 0.5, 'm_WaterDrag': 0.1599999964237213,
    'm_AirDrag': 0.006000000052154064, 'm_IsContrler': 1, 'MaxSubSpeed': 10.0,
    'm_LeaveGroundTime': 0.05000000074505806, 'm_MaxGroundBalanceAngle': 60.0, 'forwardAngleLerpSpeed': 3.0,
    'netMoveSpeedScale': 1.0, 'maxSpeed': 50.0,
}
# Literals read inside FixedUpdate/ResetSpeed/UpdateGroundState, in first-use order.
BODY_LITERALS = {
    'HumanPhyCtrl::FixedUpdate': (0.0, 0.0, 1.1920928955078125e-07, 0.0, 10000.0, -10000.0, -10.0),
    'HumanPhyCtrl::ResetSpeed': (0.10000000149011612, 0.10000000149011612),
    'HumanPhyCtrl::UpdateGroundState': (90.0, 90.0, 0.0),
}
HUMAN_PHY_GUID = '67b4d01020fbe104eb36ed0d3c6900ad'
SERIALIZED = {
    'mSpeedTimes': 1.0, 'm_GroundLayer': {'m_Bits': 2751232}, 'm_WaterLayer': {'m_Bits': 16},
    'm_CheckGroundRadius': 0.4699999988079071, 'm_CheckGroundOffsetHeight': 0.25, 'm_HuamnHeight': 1.75,
    'm_SpineHeight': 1.5, 'm_FeetHeight': 1.0, 'm_SpineFixedHeight': 0.05000000074505806,
    'm_StopSpeed': 0.05000000074505806, 'm_FlotageAcc': 10.5, 'm_FlotageMaxSpeed': 1.0, 'm_GroundDrag': 1.5,
    'm_WaterDrag': 1.2999999523162842, 'm_AirDrag': 0.006000000052154064,
    'm_WaterAcc': {'x': 0.0, 'y': 0.0, 'z': 0.0}, 'm_SubAcc': {'x': 0.0, 'y': 0.0, 'z': 0.0}, 'm_IsContrler': 1,
    'm_LeaveGroundTime': 0.5, 'm_MaxGroundBalanceAngle': 60.0, 'forwardAngleLerpSpeed': 3.0,
    'netMoveSpeedScale': 1.0, 'maxSpeed': 50.0,
}
UNITY_FIELDS = ('m_ObjectHideFlags','m_PrefabParentObject','m_PrefabInternal','m_GameObject','m_Enabled',
                'm_EditorHideFlags','m_Script','m_Name','m_EditorClassIdentifier')
RIGIDBODY = {'m_Mass': 200.0, 'm_Drag': 0.0, 'm_AngularDrag': 0.05000000074505806, 'm_UseGravity': False,
             'm_IsKinematic': False, 'm_Interpolate': 0, 'm_Constraints': 112, 'm_CollisionDetection': 0}
CAPSULE = {'m_IsTrigger': False, 'm_Enabled': True, 'm_Radius': 0.3700000047683716, 'm_Height': 1.75,
           'm_Direction': 1, 'm_Center': {'x': 0.0, 'y': 0.875, 'z': 0.0}}
MODEL_OBJECT, HUMAN_PHY, RIGIDBODY_ID, CAPSULE_ID, MODEL_LAYER = 100170, 11400002, 5400000, 13600000, 10
FIXED_TIMESTEP, MAXIMUM_TIMESTEP, GRAVITY_Y = 0.019999999552965164, 0.3333333432674408, -9.8100004196167


def key(method):
    return method['declaring_type']+'::'+method['name']


def methods_digest(evidence):
    rows=sorted((name,m['token'],m['body']['il_sha256']) for name,part in evidence['assemblies'].items()
                for m in part['methods'])
    return hashlib.sha256(json.dumps(rows,separators=(',',':')).encode()).hexdigest()


def excerpt(inventories):
    """Build the committed excerpt from saved REA inspect-managed-members results."""
    assemblies={}
    for name,path in inventories.items():
        envelope=json.loads(Path(path).read_text()); data=envelope.get('normalized_result') or envelope
        raw=(MANAGED/name).read_bytes(); methods=[]
        if data['coverage']['state']!='complete': raise ValueError('Incomplete inventory for '+name)
        for method in data['methods']:
            if not EXCERPT[name](method): continue
            body=method['body']; keep=dict(token=method['token'],declaring_type=method['declaring_type'],
                name=method['name'],signature=dict(raw_sha256=method['signature']['raw_sha256'],
                    return_type=method['signature']['return_type'],
                    parameter_types=method['signature']['parameter_types']))
            keep['body']={k:body[k] for k in ('header_format','file_offset','il_size','il_sha256','instruction_count')}
            keep['raw_il_hex']=cil_disasm.body_bytes(raw,body).hex(); methods.append(keep)
        tokens={m['token'] for m in methods}; part=dict(sha256=data['artifact']['sha256'],
            byte_length=data['artifact']['byte_length'],mvid=data['module']['mvid'],
            rea_evidence_id=envelope.get('evidence_id'),methods=methods)
        if name=='Assembly-CSharp.dll':
            part['fields']=[dict(token=x['token'],name=x['name'],type=x['signature'].get('field_type'),flags=x['flags'])
                            for x in data['fields'] if x.get('declaring_type')=='HumanPhyCtrl']
            names={x['token']:x['declaring_type']+'::'+x['name'] for x in data['methods']}
            human={t for t in tokens if names[t].startswith('HumanPhyCtrl::')}; callers={}
            for edge in data['call_edges']:
                if edge.get('target_token') in human and edge['caller_token'] not in human:
                    callers.setdefault(names[edge['target_token']].split('::')[1],set()).add(names[edge['caller_token']])
            part['external_callers']={k:sorted(v) for k,v in sorted(callers.items())}
        assemblies[name]=part
    return dict(schema=1,authority='shipped-artifact',confidence='observed',
        provider='rea-agents 6.0.0 inspect-managed-members (CLI); raw CIL sliced by cil_disasm.py',
        scope='Focused static method excerpt. Complete inventories stay ignored under rebuild/build. No assembly or game execution.',
        limitations=['Metadata tokens are build-local and only meaningful with the stated artifact SHA-256 and MVID.',
                     'CIL is read from file bytes; no JIT, floating-point or PhysX behavior is observed.',
                     'external_callers lists static call sites, not which run for the single-player female controller.'],
        assemblies=assemblies)


def load_evidence(root=ROOT):
    """Check the committed excerpt against the pinned identities and bodies."""
    evidence=json.loads((root/EVIDENCE).read_text()); found={}
    if set(evidence['assemblies'])!=set(ASSEMBLIES): raise ValueError('Unexpected HumanPhyCtrl evidence assemblies')
    for name,(digest,mvid) in ASSEMBLIES.items():
        part=evidence['assemblies'][name]
        if part['sha256']!=digest or part['mvid']!=mvid: raise ValueError('Unqualified evidence identity for '+name)
        for method in part['methods']:
            data=bytes.fromhex(method['raw_il_hex']); body=method['body']
            if len(data)!=body['il_size'] or hashlib.sha256(data).hexdigest()!=body['il_sha256']:
                raise ValueError('Method evidence differs from its hash: '+key(method))
            if len(cil_disasm.decode(data))!=body['instruction_count']:
                raise ValueError('Undecoded method evidence: '+key(method))
            found.setdefault(key(method),[]).append(method)
    if methods_digest(evidence)!=METHODS_DIGEST: raise ValueError('HumanPhyCtrl method evidence changed')
    selected={}
    for name,prefix in METHOD_IL.items():
        # Overloads share a name; the pinned body selects one.
        match=[m for m in found.get(name,()) if m['body']['il_sha256'].startswith(prefix)]
        if len(match)!=1: raise ValueError('Unqualified method body: '+name)
        selected[name]=match[0]
    return evidence,selected


def qualify_installed(evidence,managed=MANAGED):
    """Bind every excerpted body to the installed assembly bytes."""
    for name,(digest,_) in ASSEMBLIES.items():
        raw=(managed/name).read_bytes()
        if hashlib.sha256(raw).hexdigest()!=digest: raise ValueError('Unqualified installed '+name)
        for method in evidence['assemblies'][name]['methods']:
            if cil_disasm.body_bytes(raw,method['body'])!=bytes.fromhex(method['raw_il_hex']):
                raise ValueError('Method evidence differs from installed bytes: '+key(method))


def literals(method):
    return tuple(operand for _,mnemonic,operand in cil_disasm.decode(bytes.fromhex(method['raw_il_hex']))
                 if mnemonic=='ldc.r4')


def constructor_defaults(evidence,found):
    fields={int(x['token'],16):x['name'] for x in evidence['assemblies']['Assembly-CSharp.dll']['fields']}
    code=cil_disasm.decode(bytes.fromhex(found['HumanPhyCtrl::.ctor']['raw_il_hex'])); defaults={}
    for (_,a,_),(_,b,value),(_,c,token) in zip(code,code[1:],code[2:]):
        if c!='stfld': continue
        if a!='ldarg.0' or b not in ('ldc.r4','ldc.i4.1') or fields[token] in defaults:
            raise ValueError('Unsupported HumanPhyCtrl field initializer')
        defaults[fields[token]]=1 if b=='ldc.i4.1' else value
    if defaults!=CTOR_DEFAULTS: raise ValueError('HumanPhyCtrl constructor defaults changed')
    for name,expected in BODY_LITERALS.items():
        if literals(found[name])!=expected: raise ValueError('Literal changed in '+name)
    return defaults


def read_objects(path):
    import UnityPy
    result={}
    for obj in UnityPy.load(str(path)).objects:
        data=obj.read_typetree(); guid=None
        if obj.type.name=='MonoBehaviour':
            ref=data['m_Script']
            if not 1<=ref['m_FileID']<=len(obj.assets_file.externals): raise ValueError('Expected external script')
            raw=bytes(obj.assets_file.externals[ref['m_FileID']-1].guid).hex()
            guid=''.join(raw[i:i+2][::-1] for i in range(0,32,2))
        result[obj.path_id]=dict(kind=obj.type.name,data=data,script_guid=guid)
    return result


def owned(objects,path_id,kind):
    obj=objects.get(path_id)
    if not obj or obj['kind']!=kind or obj['data']['m_GameObject']!={'m_FileID':0,'m_PathID':MODEL_OBJECT}:
        raise ValueError('Female model %s changed'%kind)
    return obj


def qualify_model(model):
    root=model.get(MODEL_OBJECT)
    if not root or root['kind']!='GameObject' or root['data']['m_Layer']!=MODEL_LAYER or not root['data']['m_IsActive']:
        raise ValueError('Female physics object changed')
    matches=[k for k,o in model.items() if o.get('script_guid')==HUMAN_PHY_GUID]
    human=owned(model,HUMAN_PHY,'MonoBehaviour')
    if matches!=[HUMAN_PHY] or not human['data']['m_Enabled']: raise ValueError('Expected one enabled HumanPhyCtrl')
    serialized={k:v for k,v in human['data'].items() if k not in UNITY_FIELDS}
    # MaxSubSpeed, m_MoveSpeed and the request/ground state are private and
    # unserialized: their constructor values apply to the active model.
    if serialized!=SERIALIZED: raise ValueError('Female HumanPhyCtrl serialized values changed')
    body=owned(model,RIGIDBODY_ID,'Rigidbody')['data']; capsule=owned(model,CAPSULE_ID,'CapsuleCollider')['data']
    if any(o['kind']=='Rigidbody' and o['data']['m_GameObject']['m_PathID']==MODEL_OBJECT
           for k,o in model.items() if k!=RIGIDBODY_ID):
        raise ValueError('Unexpected second model Rigidbody')
    if {k:body[k] for k in RIGIDBODY}!=RIGIDBODY: raise ValueError('Female Rigidbody changed')
    if {k:capsule[k] for k in CAPSULE}!=CAPSULE or capsule['m_Material']['m_PathID']:
        raise ValueError('Female movement capsule changed')
    return dict(model_object=MODEL_OBJECT,layer=MODEL_LAYER,human_phy_component=HUMAN_PHY,script_guid=HUMAN_PHY_GUID,
                serialized=serialized,rigidbody=dict(component=RIGIDBODY_ID,**RIGIDBODY),
                capsule=dict(component=CAPSULE_ID,**CAPSULE))


def qualify_settings(time,dynamics):
    if len(time)!=1 or len(dynamics)!=1: raise ValueError('Unexpected project settings objects')
    (time,),(dynamics,)=time.values(),dynamics.values()
    if time['data']['Fixed Timestep']!=FIXED_TIMESTEP or time['data']['Maximum Allowed Timestep']!=MAXIMUM_TIMESTEP \
            or time['data']['m_TimeScale']!=1.0:
        raise ValueError('Project time settings changed')
    if dynamics['data']['m_Gravity']!={'x':0.0,'y':GRAVITY_Y,'z':0.0}: raise ValueError('Project gravity changed')
    return dict(fixed_timestep=FIXED_TIMESTEP,maximum_allowed_timestep=MAXIMUM_TIMESTEP,gravity_y=GRAVITY_Y,
                layer_collision_matrix=dynamics['data']['m_LayerCollisionMatrix'])


def build_outputs(root=ROOT,managed=MANAGED):
    evidence,found=load_evidence(root); qualify_installed(evidence,managed)
    defaults=constructor_defaults(evidence,found)
    binding=qualify_model(read_objects(root/MODEL))
    settings=qualify_settings(read_objects(root/TIME),read_objects(root/DYNAMICS))
    part=evidence['assemblies']['Assembly-CSharp.dll']; serialized=binding['serialized']
    metadata=dict(schema=1,source_files=[describe_source(root/p) for p in SOURCES],
        assemblies={n:dict(sha256=d,mvid=m,rea_evidence_id=evidence['assemblies'][n]['rea_evidence_id'])
                    for n,(d,m) in ASSEMBLIES.items()},
        methods_digest=METHODS_DIGEST,method_il_sha256={n:found[n]['body']['il_sha256'] for n in METHOD_IL},
        constructor_defaults=defaults,binding=binding,project=settings,
        external_callers=part['external_callers'],
        scope='Disconnected HumanPhyCtrl request latch, ground/drag state and per-FixedUpdate force requests for the female controller. No PhysX integration, collision response, contact generation, water queries, animation or live knockback.',
        request='ApplyMoveRequest sums into a pending velocity and clamps its magnitude above MaxSubSpeed 10. LateUpdate latches pending into applied and clears pending; CancelMoveRequest clears both.',
        order='FixedUpdate consumes the applied request latched by the previous LateUpdate. Update clears a counter; only the first two non-kinematic FixedUpdate calls after an Update run.',
        forces='Desired velocity replaces the Rigidbody velocity through one VelocityChange request (whole vector when physically grounded, in water or on a rope; otherwise only the part on the plane of transform.up). Gravity, water and sub accelerations follow, then quadratic drag only when the summed desired velocity is zero.',
        engine='Static CIL only. That Rigidbody.velocity read inside FixedUpdate excludes forces queued in the same call is an unqualified PhysX assumption; Mono float evaluation is not claimed bit-exact.')
    header='/* Generated by export_human_phy.py; do not edit. */\n#ifndef PE_HUMAN_PHY_SOURCE_H\n#define PE_HUMAN_PHY_SOURCE_H\n#include <stdint.h>\n'
    values=dict(max_sub_speed=defaults['MaxSubSpeed'],default_move_speed=defaults['m_MoveSpeed'],
        speed_times=serialized['mSpeedTimes'],minimum_move_speed=BODY_LITERALS['HumanPhyCtrl::ResetSpeed'][0],
        net_move_speed_scale=serialized['netMoveSpeedScale'],max_speed=serialized['maxSpeed'],
        stop_speed=serialized['m_StopSpeed'],ground_drag=serialized['m_GroundDrag'],
        water_drag=serialized['m_WaterDrag'],air_drag=serialized['m_AirDrag'],
        leave_ground_time=serialized['m_LeaveGroundTime'],
        max_ground_balance_angle=serialized['m_MaxGroundBalanceAngle'],
        forward_angle_lerp_speed=serialized['forwardAngleLerpSpeed'],human_height=serialized['m_HuamnHeight'],
        spine_height=serialized['m_SpineHeight'],feet_height=serialized['m_FeetHeight'],
        spine_fixed_height=serialized['m_SpineFixedHeight'],
        overspeed_square=BODY_LITERALS['HumanPhyCtrl::FixedUpdate'][4],
        rescue_below_y=BODY_LITERALS['HumanPhyCtrl::FixedUpdate'][5],
        rescue_y=BODY_LITERALS['HumanPhyCtrl::FixedUpdate'][6],
        gravity_y=settings['gravity_y'],fixed_timestep=settings['fixed_timestep'],
        maximum_timestep=settings['maximum_allowed_timestep'],rigidbody_mass=RIGIDBODY['m_Mass'],
        capsule_radius=CAPSULE['m_Radius'],capsule_height=CAPSULE['m_Height'],
        capsule_center_y=CAPSULE['m_Center']['y'])
    header+=''.join('static const float pe_human_phy_source_%s=%s;\n'%(k,f(v)) for k,v in values.items())
    header+='static const uint32_t pe_human_phy_source_ground_layers=UINT32_C(%d);\n'%serialized['m_GroundLayer']['m_Bits']
    header+='static const uint32_t pe_human_phy_source_water_layers=UINT32_C(%d);\n'%serialized['m_WaterLayer']['m_Bits']
    header+='static const int pe_human_phy_source_is_controller=%d;\n'%serialized['m_IsContrler']
    header+='static const int pe_human_phy_source_fixed_updates_per_update=2;\n#endif\n'
    return {'pe_human_phy_source.h':header.encode(),
            'pe_human_phy_source.json':(json.dumps(metadata,indent=2)+'\n').encode()}


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    parser.add_argument('--output',type=Path,default=ROOT/'rebuild/generated')
    parser.add_argument('--excerpt',nargs=3,type=Path,metavar=('CSHARP','FIRSTPASS','UNITYENGINE'),
                        help='rewrite the committed excerpt from saved REA inventories, then exit')
    args=parser.parse_args(argv)
    try:
        if args.excerpt:
            data=excerpt(dict(zip(ASSEMBLIES,args.excerpt)))
            (ROOT/EVIDENCE).write_text(json.dumps(data,indent=1)+'\n'); print(methods_digest(data)); return 0
        outputs=build_outputs()
        if args.check:
            for name,data in outputs.items():
                if not (args.output/name).is_file() or (args.output/name).read_bytes()!=data:
                    raise ValueError('Stale or missing '+name)
        else:
            args.output.mkdir(parents=True,exist_ok=True)
            for name,data in outputs.items(): (args.output/name).write_bytes(data)
    except (OSError,ValueError,KeyError,ImportError) as error:
        print(error,file=sys.stderr); return 1
    return 0


if __name__=='__main__': raise SystemExit(main())
