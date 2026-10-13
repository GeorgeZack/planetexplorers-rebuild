#!/usr/bin/env python3
"""Replay an observed FullBodyBipedIK capture through pe_fbbik and report the
difference from the original's own output. Offline; never launches anything."""
import argparse
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]
RIG=ROOT/'rebuild/generated/pe_fbbik_source.json'
LIBRARY=ROOT/'rebuild/build/obj/core/libpe_core.a'
EFFECTOR_NODES=(0,3,6,9,12,5,8,11,14)
HARNESS=r'''
#include "pe_fbbik.h"
#include <stdio.h>
static int v3(PeVec3 *v){return scanf("%f%f%f",&v->x,&v->y,&v->z)==3;}
static int pose(const PeFbbikRig *r,PeFbbikTransform *l){
    for(unsigned b=0;b<r->bone_count;b++)
        if(!v3(&l[b].position)||scanf("%f%f%f%f",&l[b].rotation.x,&l[b].rotation.y,&l[b].rotation.z,&l[b].rotation.w)!=4)return 0;
    return 1;
}
int main(void){
    const PeFbbikRig *rig=pe_fbbik_female_rig();PeFbbikTransform start[PE_FBBIK_MAX_BONES]={0};PeFbbik state;unsigned frames,override;
    if(scanf("%u%u",&frames,&override)!=2||!pose(rig,start)||!pe_fbbik_init(&state,rig,start))return 2;
    for(unsigned i=0;i<PE_FBBIK_LIMBS;i++){
        PeVec3 d,l,c;if(!v3(&d)||!v3(&l)||!v3(&c))return 2;
        if(override){state.bend_direction[i]=d;state.bend_default_local[i]=l;state.bend_default_child[i]=c;}
    }
    for(unsigned f=0;f<frames;f++){
        PeFbbikTransform local[PE_FBBIK_MAX_BONES]={0};PeFbbikInput in;PeFbbikOutput out;
        if(!pose(rig,local)||!pe_fbbik_input(&state,local,&in)||scanf("%f",&in.weight)!=1)return 3;
        for(unsigned i=0;i<PE_FBBIK_LIMBS;i++)if(scanf("%f",&in.bend_weight[i])!=1)return 3;
        for(unsigned e=0;e<PE_FBBIK_EFFECTORS;e++){
            PeFbbikEffector *x=&in.effector[e];
            if(!v3(&x->position_offset)||!v3(&x->position)||scanf("%f%f%f%f%f%f%f",&x->rotation.x,&x->rotation.y,&x->rotation.z,&x->rotation.w,
                &x->position_weight,&x->rotation_weight,&x->maintain_relative_position_weight)!=7)return 3;
        }
        if(!pe_fbbik_solve(&state,&in,&out)){puts("failed");continue;}
        for(unsigned b=0;b<rig->bone_count;b++){
            PeFbbikTransform t;if(!pe_fbbik_world(rig,out.local,b,&t))return 4;
            printf("%.9g %.9g %.9g %.9g %.9g %.9g %.9g ",t.position.x,t.position.y,t.position.z,t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w);
        }
        for(unsigned n=0;n<PE_FBBIK_NODES;n++)printf("%.9g %.9g %.9g ",out.node[n].x,out.node[n].y,out.node[n].z);
        puts("");
    }
    return 0;
}
'''

def require(value,message):
    if not value:raise ValueError(message)
def qmul(a,b):
    return (a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],a[3]*b[1]+a[1]*b[3]+a[2]*b[0]-a[0]*b[2],
            a[3]*b[2]+a[2]*b[3]+a[0]*b[1]-a[1]*b[0],a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2])
def rotate(q,p):
    u=q[:3];c=lambda a,b:(a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
    t=tuple(2*x for x in c(u,p));k=c(u,t);return tuple(p[i]+q[3]*t[i]+k[i] for i in range(3))
def world(parents,local):
    out=[]
    for parent,(p,q) in zip(parents,local):
        if parent<0:out.append((tuple(p),tuple(q)))
        else:
            pp,pq=out[parent];out.append((tuple(a+b for a,b in zip(pp,rotate(pq,p))),qmul(pq,q)))
    return out
def gap(a,b):return math.sqrt(sum((x-y)**2 for x,y in zip(a,b)))
def turn(a,b):return math.degrees(2*math.acos(min(1.,abs(sum(x*y for x,y in zip(a,b)))/math.sqrt(sum(x*x for x in a)*sum(x*x for x in b)))))

def bind(rig,record):
    """Map captured bone order onto the generated rig and verify the live topology."""
    names=[b['name'] for b in rig['bones']];order=[]
    captured=record['bones'];require(len(captured)==len(names),'Live solver hierarchy differs in size')
    lookup={}
    for index,bone in enumerate(captured):
        # The instantiated root may carry a clone suffix; it is identified by position.
        name=names[0] if bone['parent']<0 else bone['name']
        require(name in names and name not in lookup,'Unexpected or duplicate live solver bone: '+bone['name']);lookup[name]=index
    order=[lookup[n] for n in names];inverse={c:r for r,c in enumerate(order)}
    for r,bone in enumerate(rig['bones']):
        live=captured[order[r]];require((inverse[live['parent']] if live['parent']>=0 else -1)==bone['parent'],'Live parent differs: '+bone['name'])
        require(all(abs(s-1)<1e-4 for s in live['lossyScale']),'Live solver bone is scaled: '+bone['name'])
    to=lambda i:inverse[i]
    require([[to(n) for n in c['nodes']] for c in record['chains']]==rig['chain_bones'],'Live chain bones differ')
    nodes={b:c*3+n for c,chain in enumerate(rig['chain_bones']) for n,b in enumerate(chain)}
    require([nodes[to(b)] for b in record['effectorBones']]==list(EFFECTOR_NODES),'Live effector bones differ')
    require([to(b) for b in record['spine']]==rig['spine'] and to(record['head'])==rig['head'] and to(record['root'])==rig['root'],'Live spine/head/root differ')
    require([to(l['parent']) if l['parent']>=0 else 0xff for l in record['limbs']]==rig['limb_parents'],'Live limb parents differ')
    differences=[]
    def same(label,live,source):
        if live!=source:differences.append('%s: live %r, prefab %r'%(label,live,source))
    same('iterations',record['iterations'],rig['iterations'])
    for key,source in (('spineStiffness','spine_stiffness'),('pullBodyVertical','pull_body_vertical'),('pullBodyHorizontal','pull_body_horizontal')):same(key,record[key],rig[source])
    for c,(live,source) in enumerate(zip(record['chains'],rig['chains'])):
        for key in ('pin','pull','push','pushParent','reach','reachSmoothing','pushSmoothing'):same('chain %d %s'%(c,key),live[key],source[key])
        if c:
            require(live['bendInitiated'],'Live bend constraint is not initiated')
            if live['bendGoal']:differences.append('chain %d has a bend goal, which pe_fbbik does not model'%c)
    for i,(live,source) in enumerate(zip(record['chains'][0]['constraints'],rig['constraints'])):
        # ReadPose rewrites the first two push elasticities from spineStiffness every update.
        same('constraint %d pull'%i,live['pullElasticity'],source['pull_elasticity'])
        if i>=2:same('constraint %d push'%i,live['pushElasticity'],source['push_elasticity'])
    for i,live in enumerate(record['limbs']):
        same('limb %d maintain'%i,live['maintainRotationWeight'],rig['limb_maintain_rotation'][i]);same('limb %d weight'%i,live['weight'],rig['limb_weight'][i])
    same('head maintain',record['headMaintainRotationWeight'],rig['head_maintain_rotation'])
    same('spine iterations',record['spineIterations'],rig['spine_iterations']);same('spine twist',record['spineTwistWeight'],rig['spine_twist_weight'])
    return order,differences

def solve(rig,order,record,frames,override,build=None):
    require(LIBRARY.is_file(),'Build the core library first: make -C rebuild')
    fmt=lambda values:' '.join('%.9g'%v for v in values)
    pose=lambda local:[fmt(tuple(local[c][0])+tuple(local[c][1])) for c in order]
    lines=['%d %d'%(len(frames),1 if override else 0)]+[fmt(b['local_position']+b['local_rotation']) for b in rig['bones']]
    for chain in record['chains'][1:]:lines.append(fmt(chain['bendDirection']+chain['bendDefaultLocal']+chain['bendDefaultChild']))
    for frame in frames:
        lines+=pose(frame['pre']);lines.append(fmt([frame['weight']]+frame['bendWeights']))
        for e in frame['effectors']:lines.append(fmt(e['positionOffset']+e['position']+e['rotation']+[e['positionWeight'],e['rotationWeight'],e['maintain']]))
    with tempfile.TemporaryDirectory(dir=build or ROOT/'rebuild/build') as folder:
        source=Path(folder)/'harness.c';binary=Path(folder)/'harness';source.write_text(HARNESS)
        subprocess.run(['cc','-std=c11','-O2','-I',str(ROOT/'rebuild/src'),str(source),str(LIBRARY),'-lm','-o',str(binary)],check=True)
        result=subprocess.run([str(binary)],input='\n'.join(lines)+'\n',capture_output=True,text=True,check=True)
    rows=result.stdout.splitlines();require(len(rows)==len(frames),'Solver harness returned a different frame count')
    return [None if row=='failed' else [float(v) for v in row.split()] for row in rows]

def analyze(path,rig=None):
    rig=rig or json.loads(RIG.read_text())['rig'];header=record=end=None;frames=[]
    with Path(path).open() as stream:
        for line in stream:
            item=json.loads(line);kind=item.get('type')
            if kind=='header':header=item
            elif kind=='rig':
                require(record is None,'Capture reattached to a second solver');record=item
            elif kind=='frame':frames.append(item)
            elif kind=='end':end=item
    require(header and header.get('kind')=='observed-original-fbbik' and record and frames,'Not a complete FBBIK capture')
    order,differences=bind(rig,record);parents=[b['parent'] for b in rig['bones']]
    for frame in frames:
        if any(e['target'] for e in frame['effectors']):differences.append('an effector target is set, which pe_fbbik does not follow');break
    report=dict(schema='pe-fbbik-comparison-1',capture=str(path),frames=len(frames),end=end and end.get('reason'),
        pre_update_subscribers=record['preUpdateSubscribers'],model_behaviours=record['modelBehaviours'],setting_differences=differences,
        scope='Observed original solver input and output on the live player against pe_fbbik. Engine arithmetic is observed, not reproduced bit for bit.')
    reorder=lambda local:[local[c] for c in order]
    for label,override in (('observed_bend_defaults',True),('prefab_rest_initiation',False)):
        rows=solve(rig,order,record,frames,override);position=[];rotation=[];moved=[];node=[];offsets=[];failed=0;worst=None
        for frame,row in zip(frames,rows):
            if row is None:failed+=1;continue
            before=world(parents,reorder(frame['pre']));after=world(parents,reorder(frame['post']))
            mine=[(tuple(row[b*7:b*7+3]),tuple(row[b*7+3:b*7+7])) for b in range(len(parents))]
            p=max(gap(a[0],m[0]) for a,m in zip(after,mine));r=max(turn(a[1],m[1]) for a,m in zip(after,mine))
            position.append(p);rotation.append(r);moved.append(max(gap(a[0],b[0]) for a,b in zip(after,before)))
            offsets.append(sum(math.sqrt(sum(x*x for x in e['positionOffset'])) for e in frame['effectors']))
            base=len(parents)*7;live=iter(frame['nodes']);err=0.
            for c,chain in enumerate(rig['chain_bones']):
                for n in range(len(chain)):err=max(err,gap(next(live),row[base+(c*3+n)*3:base+(c*3+n)*3+3]))
            node.append(err)
            if worst is None or p>worst[0]:worst=(p,frame['sample'])
        stats=lambda values:dict(max=max(values),mean=sum(values)/len(values),p95=sorted(values)[int(.95*(len(values)-1))]) if values else None
        report[label]=dict(failed_frames=failed,bone_position_error_m=stats(position),bone_rotation_error_degrees=stats(rotation),
            node_position_error_m=stats(node),original_ik_displacement_m=stats(moved),summed_effector_offset_m=stats(offsets),
            worst_sample=worst and worst[1])
    live=[c['bendDefaultLocal'] for c in record['chains'][1:]]
    report['observed_bend_default_local']=live
    return report

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture',type=Path);p.add_argument('--output',type=Path)
    a=p.parse_args(argv)
    try:
        report=analyze(a.capture);text=json.dumps(report,indent=2)+'\n'
        if a.output:
            with a.output.open('x') as stream:stream.write(text)
        print(text,end='')
    except (OSError,ValueError,KeyError,TypeError,subprocess.CalledProcessError,json.JSONDecodeError) as error:
        print('FBBIK comparison: %s'%error,file=sys.stderr);return 1
    return 0
if __name__=='__main__':raise SystemExit(main())
