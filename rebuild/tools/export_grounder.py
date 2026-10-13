#!/usr/bin/env python3
"""Bind the installed FinalIK GrounderFBBIK CIL and the female prefab's
serialized grounding settings into the disconnected pe_grounder; never execute Unity."""
import argparse
import json
import math
from pathlib import Path
import sys
import cil_evidence
from export_content import ROOT, describe_source
from export_fbbik import ASSEMBLIES, COMPONENT, GROUNDER, GROUNDER_GUID, digest, ref, require
from export_hit_reaction import MODEL, f
from export_human_phy import MANAGED, read_objects

EVIDENCE='docs/reference/finalik-grounder-il.json'
# Declaring type tokens; nested Leg, Pelvis and SpineEffector have bare names in the inventory.
TYPES={'0x020005d5':'Grounder','0x020005d7':'GrounderFBBIK','0x020005d8':'GrounderFBBIK/SpineEffector',
       '0x020005dc':'Grounding','0x020005de':'Grounding/Leg','0x020005df':'Grounding/Pelvis'}
EXTRA_METHODS={'0x06002776':'Interp::LerpValue','0x06002799':'V3Tools::LineToPlane'}
METHODS_DIGEST='194c869a335dcb7b9900faff71de5d2175272041689e0bc319b5195589acfe94'
FIELDS_DIGEST='44c603dfdefb4ef9c15d76a71e0b65cf7a18bba54b224d26a8d7c4087329c51f'
SETTINGS_DIGEST='51a4a4b6612078d6c16a1db12f4264fc5c568fb80c2f2e09812963f2615b178a'
EFFECTORS=9
SOLVER_FIELDS=('layers','maxStep','heightOffset','footSpeed','footRadius','prediction','footRotationWeight','footRotationSpeed',
               'maxFootRotationAngle','rotateSolver','pelvisSpeed','pelvisDamper','lowerPelvisWeight','liftPelvisWeight','rootSphereCastRadius','quality')

def excerpt(inventory):
    """Build the committed excerpt from a saved REA inspect-managed-members result."""
    select=lambda m:(m['declaring_type_token'] in TYPES or m['token'] in EXTRA_METHODS) and m['body'].get('status')=='present'
    data=cil_evidence.excerpt({'Assembly-CSharp.dll':inventory},MANAGED,{'Assembly-CSharp.dll':select},
        'Static FinalIK GrounderFBBIK path: grounder, grounding, legs, pelvis and the two helpers they call. Complete inventories stay ignored under rebuild/build. No assembly or game execution.',
        ['Physics.Raycast and the engine Quaternion/Vector3 internal calls are native and absent from this managed evidence.',
         'Nested types carry bare names in the inventory; declaring_type_token and type_names identify them.',
         'Static method bodies do not establish collision layers, colliders or what a ray hits at runtime.'])
    envelope=json.loads(Path(inventory).read_text());source=envelope.get('normalized_result') or envelope
    part=data['assemblies']['Assembly-CSharp.dll'];tokens={m['token']:m['declaring_type_token'] for m in source['methods']}
    for method in part['methods']:method['declaring_type_token']=tokens[method['token']]
    part['type_names']=dict(TYPES)
    part['fields']=[dict(token=x['token'],declaring_type_token=x['declaring_type_token'],declaring_type=x['declaring_type'],
                         name=x['name'],type=x['signature'].get('field_type'),flags=x['flags'])
                    for x in source['fields'] if x['declaring_type_token'] in TYPES]
    return data

def load_evidence(root=ROOT):
    evidence,_=cil_evidence.load(root/EVIDENCE,ASSEMBLIES,METHODS_DIGEST,{})
    part=evidence['assemblies']['Assembly-CSharp.dll']
    require(part['type_names']==TYPES and digest(part['fields'])==FIELDS_DIGEST,'Grounder field layout evidence changed')
    require({m['token'] for m in part['methods']}>=set(EXTRA_METHODS) and
            {m['declaring_type_token'] for m in part['methods'] if m['token'] not in EXTRA_METHODS}==set(TYPES),'Grounder method selection changed')
    public=[x['name'] for x in part['fields'] if x['declaring_type_token']=='0x020005dc' and x['flags']&7==6 and not x['flags']&0x90]
    require(tuple(public)==SOLVER_FIELDS,'Serialized Grounding field order changed')
    return evidence

def recover_settings(model):
    owner=model.get(GROUNDER)
    require(owner and owner['kind']=='MonoBehaviour' and owner['script_guid']==GROUNDER_GUID,'GrounderFBBIK identity changed')
    require([k for k,v in model.items() if v.get('script_guid')==GROUNDER_GUID]==[GROUNDER],'Expected one GrounderFBBIK')
    data=owner['data'];require(ref(data['ik'])==COMPONENT and data['m_Enabled']==1,'GrounderFBBIK binding changed')
    picked={k:data[k] for k in ('weight','solver','spineBend','spineSpeed','spine')}
    require(digest(picked)==SETTINGS_DIGEST,'Serialized GrounderFBBIK settings changed')
    solver=data['solver'];require(tuple(solver)==SOLVER_FIELDS,'Serialized Grounding fields changed')
    # The C owner covers what this prefab uses: world-up grounding with the three-ray foot plane.
    require(solver['rotateSolver']==0 and solver['quality']==1 and solver['heightOffset']==0,'Unsupported grounding mode')
    require(data['weight']==1 and solver['maxStep']>0 and solver['footRadius']>0,'Unexpected grounding weights')
    horizontal=[0.]*EFFECTORS;vertical=[0.]*EFFECTORS;seen=set()
    for item in data['spine']:
        e=item['effectorType'];require(0<=e<EFFECTORS and e not in seen,'Duplicate or unknown spine effector');seen.add(e)
        horizontal[e]=item['horizontalWeight'];vertical[e]=item['verticalWeight']
    values=[solver[k] for k in SOLVER_FIELDS if k not in ('layers','rotateSolver','quality')]+[data['spineBend'],data['spineSpeed']]+horizontal+vertical
    require(all(math.isfinite(x) for x in values),'Nonfinite grounding setting')
    # GrounderFBBIK.Initiate hands Grounding the biped references root and the two foot effector bones.
    references=model[COMPONENT]['data']['references']
    def name(value):
        transform=model.get(ref(value));require(transform and transform['kind']=='Transform','Grounder reference is not a Transform')
        return model[ref(transform['data']['m_GameObject'])]['data']['m_Name']
    return dict(component=GROUNDER,weight=data['weight'],layers=solver['layers']['m_Bits'],
        root=name(references['root']),pelvis=name(references['pelvis']),feet=[name(references['leftFoot']),name(references['rightFoot'])],
        solver={k:solver[k] for k in SOLVER_FIELDS if k!='layers'},spine_bend=data['spineBend'],spine_speed=data['spineSpeed'],
        spine_horizontal=horizontal,spine_vertical=vertical)

def header(s):
    floats=lambda values:'{'+','.join(f(v) for v in values)+'}';g=s['solver']
    fields=[('max_step',f(g['maxStep'])),('foot_speed',f(g['footSpeed'])),('foot_radius',f(g['footRadius'])),('prediction',f(g['prediction'])),
        ('foot_rotation_weight',f(g['footRotationWeight'])),('foot_rotation_speed',f(g['footRotationSpeed'])),
        ('max_foot_rotation_angle',f(g['maxFootRotationAngle'])),('pelvis_speed',f(g['pelvisSpeed'])),('pelvis_damper',f(g['pelvisDamper'])),
        ('lower_pelvis_weight',f(g['lowerPelvisWeight'])),('lift_pelvis_weight',f(g['liftPelvisWeight'])),
        ('spine_bend',f(s['spine_bend'])),('spine_speed',f(s['spine_speed'])),
        ('spine_horizontal',floats(s['spine_horizontal'])),('spine_vertical',floats(s['spine_vertical']))]
    text='/* Generated by export_grounder.py; do not edit. */\n#ifndef PE_GROUNDER_SOURCE_H\n#define PE_GROUNDER_SOURCE_H\n#include "../src/pe_grounder.h"\n'
    text+='/* Grounding.layers; the caller owns what a ray can hit. */\n#define PE_GROUNDER_SOURCE_LAYERS %du\n'%s['layers']
    return text+'static const PeGrounderSettings pe_grounder_source_settings={\n'+''.join(' .%s=%s,\n'%item for item in fields)+'};\n#endif\n'

def build_outputs(root=ROOT,managed=MANAGED):
    evidence=load_evidence(root);cil_evidence.qualify_installed(evidence,ASSEMBLIES,managed)
    settings=recover_settings(read_objects(root/MODEL));part=evidence['assemblies']['Assembly-CSharp.dll']
    metadata=dict(schema=1,evidence=describe_source(root/EVIDENCE),rea_evidence_id=part['rea_evidence_id'],
        artifact_sha256=ASSEMBLIES['Assembly-CSharp.dll'][0],methods_digest=METHODS_DIGEST,fields_digest=FIELDS_DIGEST,
        sources=[describe_source(root/MODEL)],source_component=dict(id=GROUNDER,script_guid=GROUNDER_GUID,settings_sha256=SETTINGS_DIGEST),
        settings=settings,
        order=dict(update='GrounderFBBIK.Update sets firstSolve and clamps weight. OnSolverUpdate runs once per frame as the last OnPreUpdate subscriber: Grounding.Update, pelvis position += Pelvis.IKOffset*weight, SetLegIK left then right, then the spine offset.',
            grounding='Clamp settings; root ray; each Leg.Process; Pelvis.Process(-max(IKOffset)*lowerPelvisWeight, -min(IKOffset)*liftPelvisWeight, any leg grounded).',
            leg='Elapsed Time.time; flattened foot velocity; heel, forward and side rays; plane normal turned toward the hit plane at most maxFootRotationAngle radians; height is the root height above the plane, capped by the heel hit; IKOffset moves by LerpValue then Lerp toward the clamped height; clamp by leg height; foot rotation Slerp; IKPosition = foot - up*IKOffset.',
            pelvis='heightOffset Lerp toward the summed target, then minus root rise * pelvisDamper * damperF; damperF LerpValue toward grounded at 1 up, 10 down.',
            spine='spineOffset Lerp toward the summed leg bend vectors * weight * spineBend at deltaTime*spineSpeed; each listed effector gets spineOffset*horizontalWeight + root.up*|spineOffset|*verticalWeight.'),
        scope='Disconnected grounding arithmetic for the serialized female settings. Not a host integration, not a collision model and not a pose claim.',
        limitations=evidence['limitations']+['Rays are a caller callback; the original casts against Unity colliders on the serialized layers.',
            'Quaternion FromToRotation/Slerp and Vector3 RotateTowards are conventional adapters in C; no engine or Mono arithmetic was executed.',
            'Only rotateSolver off with quality Simple is implemented, as serialized; Fastest and Best are refused.',
            'The observed tutorial session was on flat ground, where the grounder contributes under a millimetre; no uneven-ground comparison exists.'])
    return {'pe_grounder_source.h':header(settings).encode(),'pe_grounder_source.json':(json.dumps(metadata,indent=2,allow_nan=False)+'\n').encode()}

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true')
    p.add_argument('--output',type=Path,default=ROOT/'rebuild/generated')
    p.add_argument('--excerpt',type=Path,metavar='INVENTORY',help='rebuild the committed CIL excerpt from a saved complete inventory')
    a=p.parse_args(argv)
    try:
        if a.excerpt:
            (ROOT/EVIDENCE).write_text(json.dumps(excerpt(a.excerpt),indent=1)+'\n');return 0
        for name,data in build_outputs().items():
            if a.check:require((a.output/name).is_file() and (a.output/name).read_bytes()==data,'Stale or missing '+name)
            else:a.output.mkdir(parents=True,exist_ok=True);(a.output/name).write_bytes(data)
    except (OSError,ValueError,KeyError,ImportError,IndexError) as error:print(error,file=sys.stderr);return 1
    return 0
if __name__=='__main__':raise SystemExit(main())
