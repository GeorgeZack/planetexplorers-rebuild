#!/usr/bin/env python3
"""Bind the installed FinalIK full-body biped solver CIL and the female prefab's
serialized solver settings into the disconnected pe_fbbik rig; never execute Unity."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
import cil_evidence
from export_content import ROOT, describe_source
from export_hit_reaction import MODEL, f
from export_human_phy import MANAGED, read_objects

EVIDENCE='docs/reference/finalik-fbbik-il.json'
ASSEMBLIES={'Assembly-CSharp.dll':('56d90949e732331be7644fc9185dc088c78ad4888a100f60d96b7aa093f4d7d3','7ddcc464-5353-4750-b969-fe628ee474da')}
# Declaring type tokens of the update path. Nested types have bare names in the
# inventory, so selection is by token, never by name.
TYPES={'0x020005e4':'FullBodyBipedIK','0x020005e5':'IK','0x020005ea':'FBIKChain','0x020005eb':'FBIKChain/ChildConstraint',
       '0x020005ed':'IKConstraintBend','0x020005ee':'IKEffector','0x020005ef':'IKMapping','0x020005f0':'IKMapping/BoneMap',
       '0x020005f1':'IKMappingBone','0x020005f2':'IKMappingLimb','0x020005f4':'IKMappingSpine','0x020005f5':'IKSolver',
       '0x020005f6':'IKSolver/Point','0x020005f8':'IKSolver/Node','0x020005fd':'IKSolverFullBody',
       '0x02000600':'IKSolverFullBodyBiped','0x02000609':'SolverManager','0x02000687':'QuaTools','0x02000688':'V3Tools'}
EXTRA_METHODS={'0x0600247c':'IKSolverFABRIK::SolveJoint'}
METHODS_DIGEST='9622bc0139f4cbe9ad755cb56d64874f2ba3d5fd4b1906fc64ba173a70b3ce04'
FIELDS_DIGEST='1d89327431c87f6fdf6db609b637ff13edd9117b0d21d795caa6e91b58c38e3a'
SOLVER_DIGEST='2703cdf380577dd4f131f75a45e6861b4fa92419ac04635e66bf5d4900c35e48'
COMPONENT,SCRIPT_GUID=11400016,'a70e525c82ce9413fa4d940ad7fcf1db'
GROUNDER,GROUNDER_GUID=11400014,'6c72e1df647af4c0098866e944a04b01'
# FullBodyBipedEffector ordinal -> chain*3+node, and the constraint chain pairs.
EFFECTOR_NODES=(0,3,6,9,12,5,8,11,14)
NONE=0xff
# Serialized public fields the solve reads, by declaring type, as a layout guard.
SERIALIZED={'RootMotion.FinalIK.IKSolverFullBody':('iterations','chain','effectors','spineMapping','boneMappings','limbMappings'),
            'RootMotion.FinalIK.IKSolverFullBodyBiped':('rootNode','spineStiffness','pullBodyVertical','pullBodyHorizontal')}

def require(value,message):
    if not value:raise ValueError(message)
def digest(value):return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode()).hexdigest()

def excerpt(inventory):
    """Build the committed excerpt from a saved REA inspect-managed-members result."""
    # Abstract members have no body and carry no evidence.
    select=lambda m:(m['declaring_type_token'] in TYPES or m['token'] in EXTRA_METHODS) and m['body'].get('status')=='present'
    data=cil_evidence.excerpt({'Assembly-CSharp.dll':inventory},MANAGED,{'Assembly-CSharp.dll':select},
        'Static FinalIK FullBodyBipedIK update path: solver, chains, effectors, bend constraints, mappings and scheduling. Complete inventories stay ignored under rebuild/build. No assembly or game execution.',
        ['Engine Quaternion/Vector3 internal calls are native and absent from this managed evidence.',
         'Nested types carry bare names in the inventory; declaring_type_token and type_names identify them.',
         'Static method bodies do not establish which solver delegates are subscribed at runtime.'])
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
    require(part['type_names']==TYPES and digest(part['fields'])==FIELDS_DIGEST,'FinalIK field layout evidence changed')
    require({m['declaring_type_token'] for m in part['methods']}==set(TYPES)|{'0x020005fb'},'FinalIK method selection changed')
    for cls,names in SERIALIZED.items():
        public=[x['name'] for x in part['fields'] if x['declaring_type']==cls and x['flags']&7==6 and not x['flags']&0x90]
        require(tuple(n for n in public if n in names)==names,'Serialized solver field order changed: '+cls)
    return evidence

def ref(value):
    require(value['m_FileID']==0,'Nonlocal solver reference')
    return value['m_PathID']
def vec(v,keys='xyz'):return [v[k] for k in keys]

def recover_rig(model):
    owner=model.get(COMPONENT)
    require(owner and owner['kind']=='MonoBehaviour' and owner['script_guid']==SCRIPT_GUID,'FullBodyBipedIK identity changed')
    require([k for k,v in model.items() if v.get('script_guid')==SCRIPT_GUID]==[COMPONENT],'Expected one FullBodyBipedIK')
    data=owner['data'];solver=data['solver']
    require(data['m_Enabled']==1 and data['fixTransforms']==1 and data['timeStep']==0,'FullBodyBipedIK scheduling state changed')
    require(digest(solver)==SOLVER_DIGEST,'Serialized FullBodyBipedIK solver changed')
    for cls,names in SERIALIZED.items():require(all(n in solver for n in names),'Serialized solver fields missing: '+cls)
    grounder=model.get(GROUNDER)
    require(grounder and grounder['script_guid']==GROUNDER_GUID and ref(grounder['data']['ik'])==COMPONENT,'GrounderFBBIK binding changed')
    chains=solver['chain'];require(len(chains)==5 and solver['IKPositionWeight']==1,'Expected five biped chains at full weight')
    nodes={}
    for c,chain in enumerate(chains):
        require(len(chain['nodes'])==(3 if c else 1),'Unexpected chain node count')
        require(chain['children']==([1,2,3,4] if not c else []) and len(chain['childConstraints'])==(0 if c else 4),'Unexpected chain topology')
        # Runtime Initiate assigns the bend bones from the nodes; no goal or serialized bones.
        require(all(ref(chain['bendConstraint'][k])==0 for k in ('bone1','bone2','bone3','bendGoal')) and chain['bendConstraint']['weight']==0,'Unexpected serialized bend constraint')
        for n,node in enumerate(chain['nodes']):
            require(node['weight']==1 and vec(node['offset'])==[0,0,0],'Unexpected serialized node state')
            nodes.setdefault(ref(node['transform']),c*3+n)   # GetPoint returns the first match
    require(len(nodes)==13,'Duplicate solver node transform')
    def point(value,optional=False):
        identity=ref(value)
        if optional and not identity:return NONE
        require(identity in nodes,'Solver reference is not a chain node');return nodes[identity]
    require(point(solver['rootNode'])==0 and ref(solver['root'])!=0,'Root node is not the body chain')
    # IK.InitiateSolver passes the component's own transform to IKSolver.Initiate, which stores it
    # as root over the serialized reference.
    holder=model.get(ref(data['m_GameObject']));require(holder and holder['kind']=='GameObject','FullBodyBipedIK owner is missing')
    owned=[ref(c[1]) for c in holder['data']['m_Component'] if model.get(ref(c[1]),{}).get('kind')=='Transform']
    require(len(owned)==1,'FullBodyBipedIK owner transform is ambiguous');solver_root=owned[0]
    constraints=[]
    for item in chains[0]['childConstraints']:
        a,b=point(item['bone1']),point(item['bone2'])
        require(a%3==0 and b%3==0 and a and b,'Child constraint is not between limb roots')
        constraints.append(dict(chain1=a//3,chain2=b//3,push_elasticity=item['pushElasticity'],pull_elasticity=item['pullElasticity']))
    effectors=solver['effectors'];require(len(effectors)==9,'Expected nine biped effectors')
    planes=[];children=None
    for e,item in enumerate(effectors):
        require(point(item['bone'])==EFFECTOR_NODES[e] and ref(item['target'])==0,'Effector binding changed')
        require(item['positionWeight']==item['rotationWeight']==item['maintainRelativePositionWeight']==0 and vec(item['positionOffset'])==[0,0,0],'Unexpected serialized effector weight')
        require(item['effectChildNodes']==1 and vec(item['planeRotationOffset'],'xyzw')==[0,0,0,1],'Unexpected effector child/plane state')
        kids=[point(x) for x in item['childBones']]
        if e==0:children=kids
        else:require(not kids,'Only the body effector may own child nodes')
        planes.append([point(item[k],True) for k in ('planeBone1','planeBone2','planeBone3')])
        require(len({p==NONE for p in planes[-1]})==1 and (planes[-1][0]!=NONE)==(e>=5),'Unexpected effector plane')
    require(children==[9,12],'Body effector children changed')
    spine=solver['spineMapping'];spine_ids=[ref(x) for x in spine['spineBones']]
    require(2<=len(spine_ids)<=4 and all(spine_ids),'Unsupported spine length')
    root_index=max(i for i,x in enumerate(spine_ids) if x in nodes)   # OnInitiate keeps the last node bone
    require(nodes[spine_ids[root_index]]==0,'Spine root node changed')
    require([point(spine[k]) for k in ('leftUpperArmBone','rightUpperArmBone','leftThighBone','rightThighBone')]==[3,6,9,12],'Spine limb bindings changed')
    require(len(solver['boneMappings'])==1,'Expected one bone mapping');head=ref(solver['boneMappings'][0]['bone'])
    limbs=solver['limbMappings'];require(len(limbs)==4,'Expected four limb mappings')
    parents=[]
    for i,item in enumerate(limbs):
        require([point(item[k]) for k in ('bone1','bone2','bone3')]==[(i+1)*3+n for n in range(3)],'Limb mapping bones changed')
        parents.append(ref(item['parentBone']))
    used=set(nodes)|set(spine_ids)|{head,solver_root}|{p for p in parents if p}
    def father(t):
        obj=model.get(t);require(obj and obj['kind']=='Transform','Solver reference is not a Transform')
        return ref(obj['data']['m_Father'])
    def depth(t):
        count=0
        while father(t):t=father(t);count+=1
        return count
    closure=set()
    for t in used:
        while t:closure.add(t);t=father(t)
    order=sorted(closure,key=lambda t:(depth(t),t));index={t:i for i,t in enumerate(order)}
    require(len(order)<=32 and depth(order[0])==0 and sum(1 for t in order if not father(t))==1,'Unsupported solver hierarchy')
    bones=[]
    for t in order:
        tr=model[t]['data'];go=model[ref(tr['m_GameObject'])]['data']
        # Serialized scales differ from one by float rounding only; the C rig treats them as one.
        require(all(abs(x-1)<=1e-6 for x in vec(tr['m_LocalScale'])),'Solver bone has nonunit scale')
        values=vec(tr['m_LocalPosition'])+vec(tr['m_LocalRotation'],'xyzw');require(all(math.isfinite(x) for x in values),'Nonfinite rest pose')
        bones.append(dict(index=index[t],id=t,name=go['m_Name'],parent=index[father(t)] if father(t) else -1,
                          local_position=values[:3],local_rotation=values[3:]))
    rig=dict(bones=bones,root=index[solver_root],iterations=solver['iterations'],
        chain_bones=[[index[ref(n['transform'])] for n in c['nodes']] for c in chains],
        chains=[{k:c[k] for k in ('pin','pull','push','pushParent','reach','reachSmoothing','pushSmoothing')} for c in chains],
        constraints=constraints,effector_nodes=list(EFFECTOR_NODES),effector_planes=planes,body_children=children,
        spine=[index[x] for x in spine_ids],spine_root_index=root_index,spine_iterations=spine['iterations'],spine_twist_weight=spine['twistWeight'],
        limb_parents=[index[p] if p else NONE for p in parents],
        limb_maintain_rotation=[x['maintainRotationWeight'] for x in limbs],limb_weight=[x['weight'] for x in limbs],
        head=index[head],head_maintain_rotation=solver['boneMappings'][0]['maintainRotationWeight'],
        spine_stiffness=solver['spineStiffness'],pull_body_vertical=solver['pullBodyVertical'],pull_body_horizontal=solver['pullBodyHorizontal'])
    require(0<=rig['iterations']<=16 and 1<=rig['spine_iterations']<=16,'Unsupported iteration count')
    return rig,dict(component=GROUNDER,enabled=grounder['data']['m_Enabled'],weight=grounder['data']['weight'],
                    settings={k:v for k,v in grounder['data'].items() if k in ('solver','spineBend','spineSpeed','spine')})

def header(rig):
    row=lambda values:'{'+','.join(values)+'}'
    floats=lambda values:row(f(v) for v in values)
    ints=lambda values:row(str(v) for v in values)
    chains=rig['chains'];bones=rig['bones']
    text='/* Generated by export_fbbik.py; do not edit. */\n#ifndef PE_FBBIK_SOURCE_H\n#define PE_FBBIK_SOURCE_H\n#include "../src/pe_fbbik.h"\n'
    text+='static const unsigned pe_fbbik_source_bone_ids[]='+ints(b['id'] for b in bones)+';\n'
    text+='/* '+', '.join('%d %s'%(b['index'],b['name']) for b in bones)+' */\n'
    text+='static const PeFbbikTransform pe_fbbik_source_rest[]={\n'+''.join('{%s,%s},\n'%(floats(b['local_position']),floats(b['local_rotation'])) for b in bones)+'};\n'
    fields=[('bone_count',str(len(bones))),('parent',ints(b['parent'] for b in bones)),('root',str(rig['root'])),('iterations',str(rig['iterations'])),
        ('chain_bone',row(ints(c) for c in rig['chain_bones'])),
        ('pin',floats(c['pin'] for c in chains)),('pull',floats(c['pull'] for c in chains)),('push',floats(c['push'] for c in chains)),
        ('push_parent',floats(c['pushParent'] for c in chains)),('reach',floats(c['reach'] for c in chains)),
        ('reach_smoothing',ints(c['reachSmoothing'] for c in chains)),('push_smoothing',ints(c['pushSmoothing'] for c in chains)),
        ('constraint',row('{%d,%d,%s,%s}'%(c['chain1'],c['chain2'],f(c['push_elasticity']),f(c['pull_elasticity'])) for c in rig['constraints'])),
        ('effector_node',ints(rig['effector_nodes'])),('effector_plane',row(ints(p) for p in rig['effector_planes'])),
        ('body_child',ints(rig['body_children'])),('effect_child_nodes',ints(['true']*9)),
        ('spine_count',str(len(rig['spine']))),('spine_root_index',str(rig['spine_root_index'])),('spine_iterations',str(rig['spine_iterations'])),
        ('spine',ints(rig['spine'])),('spine_twist_weight',f(rig['spine_twist_weight'])),
        ('limb_parent',ints(rig['limb_parents'])),('limb_maintain_rotation',floats(rig['limb_maintain_rotation'])),('limb_weight',floats(rig['limb_weight'])),
        ('head',str(rig['head'])),('head_maintain_rotation',f(rig['head_maintain_rotation'])),
        ('spine_stiffness',f(rig['spine_stiffness'])),('pull_body_vertical',f(rig['pull_body_vertical'])),('pull_body_horizontal',f(rig['pull_body_horizontal']))]
    text+='static const PeFbbikRig pe_fbbik_source_rig={\n'+''.join(' .%s=%s,\n'%item for item in fields)+'};\n#endif\n'
    return text

def build_outputs(root=ROOT,managed=MANAGED):
    evidence=load_evidence(root);cil_evidence.qualify_installed(evidence,ASSEMBLIES,managed)
    rig,grounder=recover_rig(read_objects(root/MODEL))
    part=evidence['assemblies']['Assembly-CSharp.dll']
    metadata=dict(schema=1,evidence=describe_source(root/EVIDENCE),rea_evidence_id=part['rea_evidence_id'],
        artifact_sha256=ASSEMBLIES['Assembly-CSharp.dll'][0],methods_digest=METHODS_DIGEST,fields_digest=FIELDS_DIGEST,
        sources=[describe_source(root/MODEL)],source_component=dict(id=COMPONENT,script_guid=SCRIPT_GUID,solver_sha256=SOLVER_DIGEST),
        rig=rig,grounder=grounder,
        order=dict(update='IKPositionWeight<=0 zeroes every positionOffset and returns; otherwise clamp weight, OnPreRead, ReadPose, OnPreSolve, Solve, OnPostSolve, WritePose, then every effector positionOffset=0.',
            read_pose='SetToTarget, PullBody, spine push elasticity=clamp01(1-spineStiffness) on body constraints 0 and 1; LimitBend per initiated chain; ResetOffset; effector OnPreSolve; chain ReadPose; spine and bone mapping ReadPose when iterations>0; limb mapping ReadPose.',
            solve='Per iteration: end effectors Update, Push, Reach, other effectors Update, SolveTrigonometric(false), Stage1, other effectors Update, Stage2 with clamp(iterations,2,4) constraint passes. Then OnPreBend, end effectors Update, bend rotationOffset copy, SolveTrigonometric(true).',
            write_pose='Spine and bone mappings when iterations>0, then limb mappings: parent swing and FixToNode when iterations>0, RotateToPlane on bones 1 and 2, RotateToMaintain, RotateToEffector.',
            scheduling='SolverManager.LateUpdate calls UpdateSolver every frame when timeStep is 0; Update or FixedUpdate first restores default local state when fixTransforms is set.'),
        scope='Disconnected solver arithmetic for the serialized female rig. Not a host integration and not a pose claim.',
        limitations=evidence['limitations']+['Quaternion LookRotation/FromToRotation/Lerp/Inverse and Vector3 Slerp/OrthoNormalize are conventional adapters in C; no engine or Mono arithmetic was executed.',
            'IKConstraintBend and parent swing defaults are taken from the pose at Initiate; the prefab rest pose is the only recovered candidate and the actual pose at Start is unobserved.',
            'Solver bone scales are within 1e-6 of one in the prefab and are treated as exactly one.',
            'GrounderFBBIK is enabled on the same solver and writes foot, pelvis and spine offsets from raycasts each update; it is recorded, not recovered.',
            'The prefab script GUID has no checked-in MonoScript; class identity rests on the GrounderFBBIK ik reference and the serialized field layout.'])
    return {'pe_fbbik_source.h':header(rig).encode(),'pe_fbbik_source.json':(json.dumps(metadata,indent=2,allow_nan=False)+'\n').encode()}

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
