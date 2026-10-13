#!/usr/bin/env python3
"""FinalIK full-body solver evidence, rig regeneration and a second transcription
of the compiled update compared with the C owner; no Unity execution."""
import copy
import importlib.util
import json
import math
from pathlib import Path
import random
import shutil
import subprocess
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_fbbik as e
import cil_disasm

HAVE_UNITYPY=importlib.util.find_spec('UnityPy') is not None
GENERATED=e.ROOT/'rebuild/generated'

# --- Second transcription, in double precision and in the original's object
# shape (Transform/Node/Chain/BoneMap), written from the excerpted CIL. It
# shares the engine-native adapters' definitions with C by necessity.
def add(a,b):return tuple(x+y for x,y in zip(a,b))
def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def mul(a,s):return tuple(x*s for x in a)
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def mag(a):return math.sqrt(dot(a,a))
def unit(a):
    m=mag(a);return mul(a,1/m) if m>1e-5 else (0.,0.,0.)
def same(a,b):
    d=sub(a,b);return dot(d,d)<9.99999944e-11
def clamp(v,lo,hi):return lo if v<lo else hi if v>hi else v
def lerp(a,b,t):return add(a,mul(sub(b,a),clamp(t,0,1)))
def qmul(a,b):
    return (a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],a[3]*b[1]+a[1]*b[3]+a[2]*b[0]-a[0]*b[2],
            a[3]*b[2]+a[2]*b[3]+a[0]*b[1]-a[1]*b[0],a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2])
def qinv(a):return (-a[0],-a[1],-a[2],a[3])
def rot(q,p):
    u=q[:3];t=mul(cross(u,p),2);return add(add(p,mul(t,q[3])),cross(u,t))
def qunit(q):
    m=math.sqrt(sum(x*x for x in q));return tuple(x/m for x in q) if m>1e-5 else (0.,0.,0.,1.)
def qlerp(a,b,t):
    t=clamp(t,0,1);s=-1 if sum(x*y for x,y in zip(a,b))<0 else 1
    return qunit(tuple(x+t*(s*y-x) for x,y in zip(a,b)))
def perpendicular(a):
    c=cross((1,0,0),a)
    if dot(c,c)<1e-6:c=cross((0,1,0),a)
    return unit(c)
def from_to(a,b):
    a,b=unit(a),unit(b)
    if same(a,(0,0,0)) or same(b,(0,0,0)):return (0.,0.,0.,1.)
    d=dot(a,b)
    if d>=1-1e-6:return (0.,0.,0.,1.)
    if d<=-1+1e-6:return perpendicular(a)+(0.,)
    return qunit(cross(a,b)+(1+d,))
def look(forward,up):
    z=unit(forward)
    if same(z,(0,0,0)):return (0.,0.,0.,1.)
    x=cross(up,z)
    if mag(x)<1e-5:return from_to((0,0,1),z)
    x=unit(x);y=cross(z,x);m=[[x[0],y[0],z[0]],[x[1],y[1],z[1]],[x[2],y[2],z[2]]]
    # Rebuild from the basis as the product of two swings rather than the trace form.
    first=from_to((0,0,1),z);up_now=rot(first,(0,1,0));angle=math.atan2(dot(cross(up_now,y),z),dot(up_now,y))
    return qunit(qmul((z[0]*math.sin(angle/2),z[1]*math.sin(angle/2),z[2]*math.sin(angle/2),math.cos(angle/2)),first))
def angle(a,b):return math.degrees(math.acos(clamp(dot(unit(a),unit(b)),-1,1)))
def ortho(n,t):
    n=unit(n)
    if same(n,(0,0,0)):n=(1.,0.,0.)
    t=sub(t,mul(n,dot(n,t)));return n,(perpendicular(n) if mag(t)<1e-5 else unit(t))
def slerp(a,b,t):
    t=clamp(t,0,1);ma,mb=mag(a),mag(b)
    if ma<1e-5 or mb<1e-5:return lerp(a,b,t)
    d=dot(a,b)/(ma*mb);length=ma+(mb-ma)*t
    if d>1-1e-6:return lerp(a,b,t)
    axis=perpendicular(mul(a,1/ma)) if d<-1+1e-6 else unit(cross(a,b));half=(math.pi if d<-1+1e-6 else math.acos(d))*t/2
    return mul(rot(mul(axis,math.sin(half))+(math.cos(half),),mul(a,1/ma)),length)
def joint(a,b,length):return add(b,mul(unit(sub(a,b)),length))

class Transform:
    def __init__(self,parent,position,rotation):self.parent,self.lp,self.lr=parent,tuple(position),tuple(rotation)
    @property
    def rotation(self):return self.lr if self.parent is None else qmul(self.parent.rotation,self.lr)
    @rotation.setter
    def rotation(self,q):self.lr=q if self.parent is None else qmul(qinv(self.parent.rotation),q)
    @property
    def position(self):return self.lp if self.parent is None else add(self.parent.position,rot(self.parent.rotation,self.lp))
    @position.setter
    def position(self,p):self.lp=p if self.parent is None else rot(qinv(self.parent.rotation),sub(p,self.parent.position))
class Node:
    def __init__(self,transform):self.transform=transform;self.solver=(0.,0.,0.);self.offset=(0.,0.,0.);self.length=0.;self.epw=0.;self.erw=0.;self.srot=(0.,0.,0.,1.)
class BoneMap:
    def __init__(self,transform,node=None):self.transform,self.node=transform,node
    def set_plane(self,a,b,c):self.plane=(a,b,c)
    def last_animated(self):
        a,b,c=(n.transform.position for n in self.plane)
        return (0.,0.,0.,1.) if same(a,c) else look(sub(b,a),sub(c,a))
    def target(self):
        a,b,c=(n.solver for n in self.plane)
        return (0.,0.,0.,1.) if same(a,c) else look(sub(b,a),sub(c,a))
    def update_plane(self,position):
        last=self.last_animated();self.default_target=qinv(qmul(qinv(self.transform.rotation),last))
        if position:self.plane_position=rot(qinv(last),sub(self.transform.position,self.plane[0].transform.position))
    def rotate_to_plane(self,w):
        r=qmul(self.target(),self.default_target);self.transform.rotation=r if w>=1 else qlerp(self.transform.rotation,r,w)
    def swing(self,target,w):
        t=self.transform;r=qmul(from_to(rot(t.rotation,self.swing_axis),sub(target,t.position)),t.rotation)
        t.rotation=r if w>=1 else qlerp(t.rotation,r,w)

def replay(rig,init_pose,pose,weight,effectors,bend_weight):
    def build(local):
        bones=[]
        for bone,(p,q) in zip(rig['bones'],local):bones.append(Transform(None if bone['parent']<0 else bones[bone['parent']],p,q))
        return bones
    # Initiate on the initiation pose.
    start=build(init_pose);defaults=[]
    for ids in rig['chain_bones'][1:]:
        a,b,c=(start[i] for i in ids);upper=sub(b.position,a.position);limb=sub(c.position,a.position)
        direction=ortho(upper,ortho(limb,upper)[1])[1]
        local=rot(qinv(a.rotation),direction) if angle(upper,limb)>=1 else (0.,0.,0.)
        defaults.append((direction,local,rot(qinv(c.rotation),cross(unit(limb),direction))))
    parent_axis=[None if p==e.NONE else rot(qinv(start[p].rotation),sub(start[c[0]].position,start[p].position)) for p,c in zip(rig['limb_parents'],rig['chain_bones'][1:])]
    if not weight>0:return [(tuple(p),tuple(q)) for p,q in pose]
    weight=clamp(weight,0,1);bones=build(pose);full=rig['iterations']>0
    chains=[[Node(bones[i]) for i in ids] for ids in rig['chain_bones']];flat=lambda n:chains[n//3][n%3]
    # Biped ReadPose: PullBody, then LimitBend.
    body_offset=(0.,0.,0.)
    if rig['iterations']>=1 and (rig['pull_body_vertical']!=0 or rig['pull_body_horizontal']!=0):
        def pull(effector,chain,offset):
            d=sub(effector['position'],add(chain[0].transform.position,offset))
            reach=mag(sub(chain[0].transform.position,chain[1].transform.position))+mag(sub(chain[1].transform.position,chain[2].transform.position))
            m=mag(d);return (0.,0.,0.) if m<reach else mul(mul(d,1/m),m-reach)
        o=mul(pull(effectors[5],chains[1],(0.,0.,0.)),clamp(effectors[5]['position_weight'],0,1))
        o=add(o,mul(pull(effectors[6],chains[2],o),clamp(effectors[6]['position_weight'],0,1)))
        up=rot(bones[rig['root']].rotation,(0,1,0))
        project=lambda v,n:(0.,0.,0.) if dot(n,n)<1.401298e-45 else mul(n,dot(v,n)/dot(n,n))
        if rig['pull_body_vertical']!=0:body_offset=add(body_offset,mul(project(o,up),rig['pull_body_vertical']))
        if rig['pull_body_horizontal']!=0:body_offset=add(body_offset,mul(project(o,ortho(up,o)[1]),rig['pull_body_horizontal']))
    for limb,chain in enumerate(chains[1:]):
        b1,b2,b3=(n.transform for n in chain);normal=rot(b1.rotation,mul(defaults[limb][1],-1));lower=sub(b3.position,b2.position)
        cw=0.5049999952316284*weight;pw=effectors[5+limb]['position_weight'];changed=False;clamped=lower
        if cw>=1:changed,clamped=True,normal
        elif cw>0:
            d=1-angle(normal,lower)/180
            if d<=cw:changed=True;clamped=slerp(normal,lower,clamp(d/cw,0,1)*clamp(1-(cw-d)/(1-d),0,1))
        hand=b3.rotation
        if changed:b2.rotation=qmul(from_to(lower,clamped),b2.rotation)
        if pw>0:
            fore=ortho(sub(b2.position,b1.position),sub(b3.position,b2.position))[1]
            b2.rotation=qlerp(b2.rotation,qmul(from_to(fore,normal),b2.rotation),pw*weight)
        if changed or pw>0:b3.rotation=hand
    # Effector OnPreSolve.
    state=[]
    for index,(effector,node_id,plane) in enumerate(zip(effectors,rig['effector_nodes'],rig['effector_planes'])):
        node=flat(node_id);offset=add(effector['position_offset'],body_offset) if index==0 else effector['position_offset']
        item=dict(node=node,pos_w=clamp(effector['position_weight'],0,1)*weight,maintain=clamp(effector['maintain'],0,1),
                  plane=None if plane[0]==e.NONE else [flat(p) for p in plane],first=True,position=effector['position'],children=[],local=[],offset_q=(0.,0.,0.,1.))
        node.epw=item['pos_w'];node.erw=clamp(effector['rotation_weight'],0,1)*weight;node.srot=effector['rotation']
        node.offset=add(node.offset,mul(offset,weight))
        if index==0 and full:
            for child in rig['body_children']:
                item['children'].append(flat(child));item['local'].append(sub(flat(child).transform.position,node.transform.position))
                flat(child).offset=add(flat(child).offset,mul(offset,weight))
        if item['plane'] and item['maintain']>0:
            a,b,c=(n.transform.position for n in item['plane']);item['animated_plane']=look(sub(b,a),sub(c,a))
        state.append(item)
    def update(item):
        node=item['node']
        if item['first']:item['animated']=add(node.transform.position,node.offset);item['first']=False
        item['offset_q']=(0.,0.,0.,1.)
        if item['plane'] is None:target=node.solver
        elif not item['maintain']>0:target=item['animated']
        else:
            a,b,c=item['plane'];u=sub(b.solver,a.solver);v=sub(c.solver,a.solver)
            turned=qmul((0.,0.,0.,1.) if same(u,(0,0,0)) else look(u,v),qinv(item['animated_plane']))
            moved=add(a.solver,rot(turned,sub(node.transform.position,a.transform.position)))
            item['offset_q']=qlerp((0.,0.,0.,1.),turned,item['maintain']);target=lerp(item['animated'],add(moved,node.offset),item['maintain'])
        node.solver=lerp(target,item['position'],item['pos_w'])
        for child,local in zip(item['children'],item['local']):child.solver=lerp(child.solver,add(node.solver,local),item['pos_w'])
    def update_all(end):
        for item in state:
            if (item['plane'] is not None)==end:update(item)
    # Chain ReadPose.
    settings=rig['chains'];length=[0.]*5;root_length=[0.]*5;reach_force=[0.]*5;span0=[0.]*5
    for k,chain in enumerate(chains):
        for node in chain:node.solver=add(node.transform.position,node.offset)
        for a,b in zip(chain,chain[1:]):a.length=mag(sub(a.transform.position,b.transform.position));length[k]+=a.length
        if k:
            root_length[k]=mag(sub(chain[0].transform.position,chains[0][0].transform.position))
            reach_force[k]=settings[k]['reach']*clamp(chain[2].epw,0,1)
            span0[k]=mag(sub(chain[0].transform.position,chain[2].transform.position))
    push_elasticity=[clamp(1-rig['spine_stiffness'],0,1)]*2+[c['push_elasticity'] for c in rig['constraints'][2:]]
    constraints=[]
    for index,item in enumerate(rig['constraints']):
        a,b=chains[item['chain1']][0],chains[item['chain2']][0]
        rigid=not push_elasticity[index]>0 and not item['pull_elasticity']>0
        fade=1-(.5+(settings[item['chain1']]['pull']-settings[item['chain2']]['pull'])*.5) if rigid else .5
        constraints.append((a,b,mag(sub(a.transform.position,b.transform.position)),rigid,fade,push_elasticity[index],item['pull_elasticity']))
    total=max(1.,chains[0][0].epw+sum(chains[k][0].epw*settings[k]['pull'] for k in range(1,5)))
    cross_fade=[0.]+[chains[k][0].epw*settings[k]['pull']/total for k in range(1,5)]
    pull_sum=max(1.,sum(settings[k]['pull'] for k in range(1,5)))
    # Mapping ReadPose.
    spine=[BoneMap(bones[i]) for i in rig['spine']];root_node=chains[0][0];last=len(spine)-1
    if full:
        spine[0].set_plane(root_node,chains[3][0],chains[4][0]);spine[0].update_plane(True)
        arms=sub(chains[1][0].transform.position,chains[2][0].transform.position)
        for a,b in zip(spine,spine[1:]):
            step=sub(b.transform.position,a.transform.position);a.length=mag(step)
            a.swing_axis=rot(qinv(a.transform.rotation),step);a.twist_axis=rot(qinv(a.transform.rotation),ortho(step,arms)[1])
        spine[last].set_plane(root_node,chains[1][0],chains[2][0]);spine[last].update_plane(True)
        head_rotation=bones[rig['head']].rotation
    limbs=[]
    for chain in chains[1:]:
        one,two=BoneMap(chain[0].transform,chain[0]),BoneMap(chain[1].transform,chain[1])
        one.set_plane(chain[0],chain[1],chain[2]);two.set_plane(chain[1],chain[2],chain[0]);one.update_plane(True);two.update_plane(False)
        limbs.append((one,two,chain[2].transform.rotation))
    def child_constraints():
        for a,b,nominal,rigid,fade,push_e,pull_e in constraints:
            if push_e>=1 and pull_e>=1:continue
            d=sub(b.solver,a.solver);m=mag(d)
            if m==nominal or m==0:continue
            f=1. if rigid else 1-(pull_e if m>nominal else push_e)
            step=mul(d,f*(1-nominal/m));a.solver=add(a.solver,mul(step,fade));b.solver=sub(b.solver,mul(step,1-fade))
    offsets=[(0.,0.,0.,1.)]*4
    def trigonometric(bend):
        for k in range(1,5):
            chain=chains[k];span=sub(chain[2].solver,chain[0].solver);m=mag(span)
            if m==0:continue
            reach=clamp(m,0,length[k]*0.9990000128746033);direction=mul(mul(span,1/m),reach)
            if bend:
                w=bend_weight[k-1];fixed=unit(defaults[k-1][0])
                if w>=1:normal=fixed
                else:
                    normal=rot(from_to(sub(chain[2].transform.position,chain[0].transform.position),span),sub(chain[1].transform.position,chain[0].transform.position))
                    if chain[2].erw>0:normal=lerp(normal,mul(cross(span,rot(chain[2].srot,defaults[k-1][2])),-1),chain[2].erw)
                    q=offsets[k-1]
                    if not sum(x*y for x,y in zip(q,(0,0,0,1)))>0.999999:normal=rot(qmul(from_to(rot(q,span),span),q),normal)
                    if w>0:normal=lerp(normal,fixed,w)
            else:normal=sub(chain[1].solver,chain[0].solver)
            x=((reach*reach+chain[0].length**2-chain[1].length**2)/2)/reach;y=math.sqrt(max(0.,chain[0].length**2-x*x))
            chain[1].solver=add(chain[0].solver,(0.,0.,0.) if same(direction,(0,0,0)) else rot(look(direction,normal),(0,y,x)))
    for _ in range(rig['iterations']):
        update_all(True)
        pushed=(0.,0.,0.)
        for k in range(1,5):
            chain=chains[k]
            if not settings[k]['push']>0:continue
            span=sub(chain[2].solver,chain[0].solver);m=mag(span)
            if m==0:continue
            f=1-m/span0[k]
            if not f>0:continue
            f=f*f if settings[k]['pushSmoothing']==1 else f**3 if settings[k]['pushSmoothing']==2 else f
            step=mul(span,-f*settings[k]['push']);chain[0].solver=add(chain[0].solver,step);pushed=add(pushed,mul(step,settings[k]['pushParent']))
        root_node.solver=add(root_node.solver,pushed)
        for k in range(1,5):
            chain=chains[k];force=reach_force[k]
            if not force>0:continue
            span=sub(chain[2].solver,chain[0].solver)
            if same(span,(0,0,0)):continue
            m=mag(span);f=clamp(clamp(m/length[k],1-force,1+force)-1+force,-1,1)
            f=f*f if settings[k]['reachSmoothing']==1 else f**3 if settings[k]['reachSmoothing']==2 else f
            step=mul(mul(span,1/m),length[k]*clamp(f,0,m))
            chain[0].solver=add(chain[0].solver,mul(step,1-chain[0].epw));chain[2].solver=add(chain[2].solver,step)
        update_all(False);trigonometric(False)
        for chain in chains[1:]:
            chain[1].solver=joint(chain[1].solver,chain[2].solver,chain[1].length);chain[0].solver=joint(chain[0].solver,chain[1].solver,chain[0].length)
        wanted=root_node.solver;child_constraints()
        for k in range(1,5):
            target=joint(root_node.solver,chains[k][0].solver,root_length[k]) if root_length[k]>0 else chains[k][0].solver
            wanted=add(wanted,mul(sub(target,root_node.solver),settings[k]['pull']/pull_sum))
        root_node.solver=lerp(wanted,root_node.solver,settings[0]['pin'])
        update_all(False)
        for _pass in range(int(clamp(rig['iterations'],2,4))):
            child_constraints()
            for k in range(1,5):
                a,b=root_node,chains[k][0];d=sub(b.solver,a.solver);m=mag(d)
                if root_length[k]==m or m==0:continue
                step=mul(d,1-root_length[k]/m);a.solver=add(a.solver,mul(step,cross_fade[k]));b.solver=sub(b.solver,mul(step,1-cross_fade[k]))
        for k in range(1,5):
            chain=chains[k]
            chain[0].solver=joint(chain[0].solver,root_node.solver,root_length[k]) if root_length[k]>0 else root_node.solver
            chain[1].solver=joint(chain[1].solver,chain[0].solver,chain[0].length);chain[2].solver=joint(chain[2].solver,chain[1].solver,chain[1].length)
    update_all(True)
    if full:offsets=[state[5+i]['offset_q'] for i in range(4)]
    else:
        body=effectors[0];shift=lerp(body['position_offset'],sub(body['position'],add(state[0]['node'].transform.position,body['position_offset'])),body['position_weight'])
        for item in state[:5]:item['node'].solver=add(item['node'].solver,shift)
    trigonometric(True)
    # WritePose.
    if full:
        low=add(root_node.solver,rot(spine[0].target(),spine[0].plane_position));high=add(root_node.solver,rot(spine[last].target(),spine[last].plane_position))
        index=rig['spine_root_index']
        if len(spine)>3 or index!=1:
            shift=sub(root_node.solver,spine[index].transform.position);ik=[add(s.transform.position,shift) for s in spine]
            for _ in range(rig['spine_iterations']):
                ik[last]=high
                for i in range(last-1,-1,-1):ik[i]=joint(ik[i],ik[i+1],spine[i].length)
                ik[0]=low
                for i in range(1,last+1):ik[i]=joint(ik[i],ik[i-1],spine[i-1].length)
                ik[index]=root_node.solver
        else:
            ik=[None]*len(spine);ik[0]=low;ik[index]=root_node.solver
        ik[last]=high
        spine[0].transform.position=ik[0];spine[0].rotate_to_plane(1)
        for i in range(1,last):
            spine[i].swing(ik[i+1],1)
            if rig['spine_twist_weight']>0:
                t=spine[i].transform;w=i/(len(spine)-2)*rig['spine_twist_weight']
                twist=ortho(sub(ik[i+1],t.position),sub(chains[1][0].solver,chains[2][0].solver))[1]
                r=qmul(from_to(rot(t.rotation,spine[i].twist_axis),twist),t.rotation);t.rotation=r if w>=1 else qlerp(t.rotation,r,w)
        spine[last].transform.position=ik[last];spine[last].rotate_to_plane(1)
        if weight*rig['head_maintain_rotation']>0:bones[rig['head']].rotation=qlerp(bones[rig['head']].rotation,head_rotation,weight*rig['head_maintain_rotation'])
    else:bones[rig['spine'][0]].position=add(bones[rig['spine'][0]].position,shift)
    for limb,(one,two,maintain) in enumerate(limbs):
        w=clamp(rig['limb_weight'][limb],0,1);chain=chains[limb+1]
        if not w>0:continue
        if full:
            if rig['limb_parents'][limb]!=e.NONE:
                parent=BoneMap(bones[rig['limb_parents'][limb]]);parent.swing_axis=parent_axis[limb];parent.swing(chain[0].solver,w)
            one.transform.position=chain[0].solver if w>=1 else lerp(one.transform.position,chain[0].solver,w)
        one.rotate_to_plane(w);two.rotate_to_plane(w);end=chain[2].transform
        keep=rig['limb_maintain_rotation'][limb]*w*weight
        if keep>0:end.rotation=qlerp(end.rotation,maintain,keep)
        turn=w*chain[2].erw
        if turn>0:end.rotation=chain[2].srot if turn>=1 else qlerp(end.rotation,chain[2].srot,turn)
    return [(b.lp,b.lr) for b in bones]

HARNESS=r'''
#include "pe_fbbik.h"
#include <stdio.h>
int main(void){
    const PeFbbikRig *rig=pe_fbbik_female_rig();unsigned cases;PeFbbikRig copy=*rig;
    if(scanf("%u",&cases)!=1)return 2;
    for(unsigned c=0;c<cases;c++){
        PeFbbikTransform start[PE_FBBIK_MAX_BONES]={0},pose[PE_FBBIK_MAX_BONES]={0};PeFbbik state;PeFbbikInput in;PeFbbikOutput out;
        if(scanf("%u",&copy.iterations)!=1)return 2;
        for(unsigned pass=0;pass<2;pass++)for(unsigned b=0;b<rig->bone_count;b++){
            PeFbbikTransform *t=pass?&pose[b]:&start[b];
            if(scanf("%f%f%f%f%f%f%f",&t->position.x,&t->position.y,&t->position.z,&t->rotation.x,&t->rotation.y,&t->rotation.z,&t->rotation.w)!=7)return 2;
        }
        if(!pe_fbbik_init(&state,&copy,start)||!pe_fbbik_input(&state,pose,&in)||scanf("%f",&in.weight)!=1)return 3;
        for(unsigned i=0;i<PE_FBBIK_LIMBS;i++)if(scanf("%f",&in.bend_weight[i])!=1)return 2;
        for(unsigned e=0;e<PE_FBBIK_EFFECTORS;e++){
            PeFbbikEffector *x=&in.effector[e];
            if(scanf("%f%f%f%f%f%f%f%f%f%f%f%f%f",&x->position_offset.x,&x->position_offset.y,&x->position_offset.z,&x->position.x,&x->position.y,&x->position.z,
                &x->rotation.x,&x->rotation.y,&x->rotation.z,&x->rotation.w,&x->position_weight,&x->rotation_weight,&x->maintain_relative_position_weight)!=13)return 2;
        }
        if(!pe_fbbik_solve(&state,&in,&out))return 4;
        for(unsigned b=0;b<rig->bone_count;b++){
            PeFbbikTransform t;if(!pe_fbbik_world(&copy,out.local,b,&t))return 5;
            printf("%.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",t.position.x,t.position.y,t.position.z,t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w);
        }
    }
    return 0;
}
'''

def random_case(rig,rng,index):
    rest=[(tuple(b['local_position']),tuple(b['local_rotation'])) for b in rig['bones']]
    def jitter(amount):
        pose=[]
        for position,rotation in rest:
            axis=unit((rng.uniform(-1,1),rng.uniform(-1,1),rng.uniform(-1,1)+1e-3));half=rng.uniform(-amount,amount)/2
            pose.append((position,qunit(qmul(rotation,mul(axis,math.sin(half))+(math.cos(half),)))))
        return pose
    pose=jitter(.45);init=rest if index%3 else jitter(.2)
    bones=[];
    for bone,(p,q) in zip(rig['bones'],pose):bones.append(Transform(None if bone['parent']<0 else bones[bone['parent']],p,q))
    effectors=[]
    for i,node in enumerate(rig['effector_nodes']):
        t=bones[rig['chain_bones'][node//3][node%3]];mode=index%6
        offset=tuple(rng.uniform(-.08,.08) for _ in range(3)) if rng.random()<.6 else (0.,0.,0.)
        weighted=mode in (2,3,5) and i in (0,5,6,7,8) and rng.random()<.6
        effectors.append(dict(position_offset=offset,position=add(t.position,tuple(rng.uniform(-.12,.12) for _ in range(3))),
            rotation=qunit(tuple(rng.uniform(-1,1) for _ in range(4))),
            position_weight=rng.choice((1.,.5)) if weighted else 0.,rotation_weight=rng.choice((1.,.4)) if mode==3 and i>=5 else 0.,
            maintain=rng.choice((1.,.6)) if mode in (4,5) and i>=5 else 0.))
    return dict(iterations=0 if index%7==6 else 1+index%2,init=init,pose=pose,weight=rng.choice((1.,1.,.6)),
                bend=[rng.choice((0.,0.,.5,1.)) for _ in range(4)],effectors=effectors)

class Portable(unittest.TestCase):
    def test_evidence_binds_ported_methods(self):
        evidence=e.load_evidence();part=evidence['assemblies']['Assembly-CSharp.dll'];methods=part['methods']
        self.assertEqual(len(methods),229);self.assertEqual(part['rea_evidence_id'],'ev_13b79477fb6033537478536807957a9437d4a82d8da7d9d87b9ef1ef27ee7243')
        def body(type_token,name):
            match=[m for m in methods if m['declaring_type_token']==type_token and m['name']==name]
            self.assertEqual(len(match),1,name);return cil_disasm.decode(bytes.fromhex(match[0]['raw_il_hex']))
        literals=lambda ops:[v for _,m,v in ops if m=='ldc.r4']
        self.assertIn(0.9990000128746033,literals(body('0x020005ea','SolveTrigonometric')))
        clamp=[m for m in methods if m['declaring_type_token']=='0x020005ed' and m['name']=='.ctor']
        self.assertEqual(len(clamp),2)
        for m in clamp:self.assertIn(0.5049999952316284,literals(cil_disasm.decode(bytes.fromhex(m['raw_il_hex']))))
        self.assertIn(180.0,literals(body('0x02000688','ClampDirection')))
        solve=[v for _,m,v in body('0x020005fd','Solve') if m in ('call','callvirt')]
        # Push precedes Reach, and SolveTrigonometric precedes Stage1 and Stage2.
        order=[solve.index(t) for t in (0x060023d9,0x060023d8,0x060023da,0x060023db,0x060023dc)]
        self.assertEqual(order,sorted(order))
        self.assertEqual([v for _,m,v in body('0x020005fd','ApplyBendConstraints') if m=='callvirt'],[0x060023da])

    def test_changed_evidence_is_rejected(self):
        original=json.loads((e.ROOT/e.EVIDENCE).read_text())
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/'docs/reference').mkdir(parents=True)
            for change in ('il','field','type'):
                data=copy.deepcopy(original);part=data['assemblies']['Assembly-CSharp.dll']
                if change=='il':part['methods'][5]['raw_il_hex']='00'+part['methods'][5]['raw_il_hex'][2:]
                elif change=='field':part['fields'][3]['flags']^=1
                else:part['type_names']['0x020005ea']='Other'
                (root/e.EVIDENCE).write_text(json.dumps(data))
                with self.assertRaises(ValueError):e.load_evidence(root)

    def test_generated_metadata_is_strict_and_scoped(self):
        data=json.loads((GENERATED/'pe_fbbik_source.json').read_text());rig=data['rig']
        self.assertEqual((len(rig['bones']),rig['iterations'],rig['spine_root_index'],len(rig['constraints'])),(24,1,1,4))
        self.assertEqual([rig['bones'][i]['id'] for i in rig['spine']],[400288,400264,400148])
        self.assertEqual(data['grounder']['component'],11400014);self.assertEqual(data['grounder']['enabled'],1)
        self.assertTrue(any('GrounderFBBIK' in x for x in data['limitations']) and any('Initiate' in x for x in data['limitations']))
        self.assertNotIn(str(e.ROOT),json.dumps(data))

    @unittest.skipUnless(shutil.which('cc'),'C compiler unavailable')
    def test_c_solver_against_second_transcription(self):
        library=e.ROOT/'rebuild/build/obj/core/libpe_core.a'
        if not library.is_file():self.skipTest('Built core library unavailable')
        rig=json.loads((GENERATED/'pe_fbbik_source.json').read_text())['rig'];rng=random.Random(20261012);cases=[random_case(rig,rng,i) for i in range(84)]
        fmt=lambda values:' '.join('%.9g'%v for v in values)
        lines=[str(len(cases))]
        for case in cases:
            lines.append(str(case['iterations']))
            for pose in (case['init'],case['pose']):lines+= [fmt(p+q) for p,q in pose]
            lines.append(fmt([case['weight']]+case['bend']))
            for x in case['effectors']:lines.append(fmt(x['position_offset']+x['position']+x['rotation']+(x['position_weight'],x['rotation_weight'],x['maintain'])))
        with tempfile.TemporaryDirectory(dir=e.ROOT/'rebuild/build') as folder:
            source=Path(folder)/'harness.c';binary=Path(folder)/'harness';source.write_text(HARNESS)
            subprocess.run(['cc','-std=c11','-O2','-I',str(e.ROOT/'rebuild/src'),str(source),str(library),'-lm','-o',str(binary)],check=True)
            result=subprocess.run([str(binary)],input='\n'.join(lines)+'\n',capture_output=True,text=True,check=True)
        rows=[[float(v) for v in line.split()] for line in result.stdout.splitlines()];self.assertEqual(len(rows),len(cases)*24)
        worst_position=worst_rotation=0.;moved=0
        for index,case in enumerate(cases):
            local=replay(dict(rig,iterations=case['iterations']),case['init'],case['pose'],case['weight'],case['effectors'],case['bend'])
            bones=[];before=[]
            for bone,(p,q),(p0,q0) in zip(rig['bones'],local,case['pose']):
                bones.append(Transform(None if bone['parent']<0 else bones[bone['parent']],p,q))
                before.append(Transform(None if bone['parent']<0 else before[bone['parent']],p0,q0))
            for b,(bone,was) in enumerate(zip(bones,before)):
                row=rows[index*24+b];worst_position=max(worst_position,mag(sub(bone.position,tuple(row[:3]))))
                worst_rotation=max(worst_rotation,1-abs(sum(x*y for x,y in zip(bone.rotation,row[3:]))))
                moved+=mag(sub(bone.position,was.position))>.01
        # float32 C against a double replay; a transcription slip moves bones by centimetres.
        self.assertLess(worst_position,2e-4);self.assertLess(worst_rotation,2e-6);self.assertGreater(moved,400)

@unittest.skipUnless(HAVE_UNITYPY and e.MANAGED.is_dir(),'UnityPy or the installed game unavailable')
class Installed(unittest.TestCase):
    def test_regeneration_matches_installed_bytes_and_prefab(self):
        for name,data in e.build_outputs().items():self.assertEqual((GENERATED/name).read_bytes(),data,name)

    def test_changed_prefab_solver_is_rejected(self):
        model=e.read_objects(e.ROOT/e.MODEL)
        for change in ('iterations','guid','weight','grounder'):
            copy_model=copy.deepcopy(model)
            if change=='iterations':copy_model[e.COMPONENT]['data']['solver']['iterations']=2
            elif change=='guid':copy_model[e.COMPONENT]['script_guid']='0'*32
            elif change=='weight':copy_model[e.COMPONENT]['data']['solver']['effectors'][5]['positionWeight']=1.0
            else:copy_model[e.GROUNDER]['data']['ik']['m_PathID']=0
            with self.assertRaises(ValueError):e.recover_rig(copy_model)

if __name__=='__main__':unittest.main()
