#!/usr/bin/env python3
"""FinalIK grounder evidence, settings regeneration and a second transcription
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
import export_grounder as e
import cil_disasm

HAVE_UNITYPY=importlib.util.find_spec('UnityPy') is not None
GENERATED=e.ROOT/'rebuild/generated'
INF=float('inf')

# --- Second transcription, in double precision and in the original's object
# shape (Grounding/Leg/Pelvis/Grounder), written from the excerpted CIL. It
# shares the engine-native adapters' definitions with C by necessity.
def add(a,b):return tuple(x+y for x,y in zip(a,b))
def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def mul(a,s):return tuple(x*s for x in a)
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def mag(a):return math.sqrt(dot(a,a))
def normalized(a):
    m=mag(a);return mul(a,1/m) if m>1e-5 else (0.,0.,0.)
def clamp(v,lo,hi):return lo if v<lo else hi if v>hi else v
def lerp(a,b,t):return a+(b-a)*clamp(t,0,1)
UP=(0.,1.,0.);IDENTITY=(0.,0.,0.,1.)
def qunit(q):
    m=math.sqrt(dot(q,q));return tuple(x/m for x in q) if m>1e-5 else IDENTITY
def from_to(a,b):
    a=normalized(a);b=normalized(b);d=dot(a,b)
    if d>=1-1e-6:return IDENTITY
    if d<=-1+1e-6:
        axis=cross((1,0,0),a)
        if dot(axis,axis)<1e-6:axis=cross((0,1,0),a)
        return normalized(axis)+(0.,)
    return qunit(cross(a,b)+(1+d,))
def slerp_unclamped(a,b,t):
    d=dot(a,b)
    if d<0:b=mul(b,-1);d=-d
    if d>.9995:return qunit(tuple(x+(y-x)*t for x,y in zip(a,b)))
    angle=math.acos(clamp(d,-1,1));s=math.sin(angle);p=math.sin((1-t)*angle)/s;q=math.sin(t*angle)/s
    return qunit(tuple(x*p+y*q for x,y in zip(a,b)))
def slerp(a,b,t):return slerp_unclamped(a,b,clamp(t,0,1))
def rotate_towards(current,target,max_radians,max_magnitude):
    a=mag(current);b=mag(target)
    def moved(x,y,limit):
        delta=y-x;return x+min(delta,limit) if delta>0 else x-min(-delta,limit)
    if a>1e-5 and b>1e-5:
        fa=mul(current,1/a);tb=mul(target,1/b);d=dot(fa,tb)
        if d<=1-1e-6:
            if d<-1+1e-6:
                axis=cross((1,0,0),fa)
                if dot(axis,axis)<1e-6:axis=cross((0,0,1),fa)
                axis=normalized(axis);angle=max_radians
            else:axis=normalized(cross(fa,tb));angle=min(max_radians,math.acos(d))
            c=math.cos(angle);s=math.sin(angle)
            turned=add(add(mul(fa,c),mul(cross(axis,fa),s)),mul(axis,dot(axis,fa)*(1-c)))
            return mul(turned,moved(a,b,max_magnitude))
    delta=sub(target,current);m=mag(delta)
    return target if m<=max_magnitude or m==0 else add(current,mul(delta,max_magnitude/m))
def quat_rotate_towards(a,b,max_degrees):
    angle=math.acos(min(abs(dot(a,b)),1))*2*57.29578
    return b if angle==0 else slerp_unclamped(a,b,min(1,max_degrees/angle))

class Time:time=0.;delta=0.
def lerp_value(value,target,up,down):
    if value==target:return target
    if value<target:return clamp(value+Time.delta*up,-INF,target)
    return clamp(value-Time.delta*down,target,INF)
def line_to_plane(origin,direction,normal,point):
    a=dot(sub(point,origin),normal);b=dot(direction,normal)
    return (0.,0.,0.) if b==0 else add(origin,mul(normalized(direction),a/b))

class Root:position=(0.,0.,0.);forward=(0.,0.,1.);right=(1.,0.,0.);up=UP
class Foot:position=(0.,0.,0.)
class Hit:
    def __init__(self,point,normal=UP,distance=0.):self.point=point;self.normal=normal;self.distance=distance
class Leg:
    def __init__(self,grounding,transform):
        self.grounding=grounding;self.transform=transform;self.up=UP;self.last_position=transform.position;self.last_time=Time.delta
        self.ik_offset=0.;self.r=IDENTITY;self.rotation_offset=IDENTITY;self.ik_position=transform.position;self.height=0.;self.grounded=False
    def raycast(self,offset):
        g=self.grounding;origin=add(self.transform.position,offset);hit=Hit(sub(origin,mul(mul(self.up,g.max_step),2)))
        return g.cast(add(origin,mul(self.up,g.max_step)),g.max_step*3) or hit
    def height_from_ground(self,point):
        g=self.grounding;position=self.transform.position
        return (position[1]-point[1])-(position[1]-g.root.position[1])
    def process(self):
        g=self.grounding
        self.delta=Time.time-self.last_time;self.last_time=Time.time
        if self.delta==0:return
        self.up=UP;position=self.transform.position
        velocity=mul(sub(position,self.last_position),1/self.delta);velocity=(velocity[0],0.,velocity[2]);self.last_position=position
        prediction=mul(velocity,g.prediction)
        heel=self.raycast((0.,0.,0.));forward=self.raycast(add(mul(g.root.forward,g.foot_radius),prediction))
        side=self.raycast(mul(mul(g.root.right,g.foot_radius),.5))
        normal=normalized(cross(sub(forward.point,heel.point),sub(side.point,heel.point)))
        if dot(normal,self.up)<0:normal=mul(normal,-1)
        normal=rotate_towards(self.up,normal,g.max_foot_rotation_angle*0.01745329238474369,self.delta)
        to_hit=from_to(self.up,normal)
        on_plane=line_to_plane(add(position,mul(self.up,g.max_step)),mul(self.up,-1),normal,heel.point)
        self.height=clamp(self.height_from_ground(on_plane),-INF,self.height_from_ground(heel.point))
        self.grounded=self.height<g.max_step
        target=clamp(self.height,-g.max_step,g.max_step)
        if not g.root_grounded:target=0.
        self.ik_offset=lerp_value(self.ik_offset,target,g.foot_speed,g.foot_speed)
        self.ik_offset=lerp(self.ik_offset,target,self.delta*g.foot_speed)
        limit=clamp(g.max_step-(position[1]-g.root.position[1]),0,g.max_step)
        self.ik_offset=clamp(self.ik_offset,-limit,self.ik_offset)
        wanted=IDENTITY if g.max_foot_rotation_angle<=0 else to_hit if g.max_foot_rotation_angle>=180 else quat_rotate_towards(IDENTITY,to_hit,g.max_foot_rotation_angle)
        self.r=slerp(self.r,wanted,self.delta*g.foot_rotation_speed)
        self.ik_position=sub(position,mul(self.up,self.ik_offset))
        self.rotation_offset=self.r if g.foot_rotation_weight>=1 else slerp(IDENTITY,self.r,g.foot_rotation_weight)
class Pelvis:
    def __init__(self,grounding):
        self.grounding=grounding;self.last_root=grounding.root.position;self.last_time=Time.delta;self.damper=0.;self.height=0.
    def process(self,lowest,highest,grounded):
        g=self.grounding;delta=Time.time-self.last_time;self.last_time=Time.time
        if delta<=0:return
        target=lowest+highest
        if not g.root_grounded:target=0.
        self.height=lerp(self.height,target,delta*g.pelvis_speed)
        rise=g.root.position[1]-self.last_root[1];self.last_root=g.root.position
        self.damper=lerp_value(self.damper,1. if grounded else 0.,1.,10.)
        self.height-=rise*g.pelvis_damper*self.damper
class Grounding:
    def __init__(self,settings,root,feet,cast):
        s=settings['solver'];self.root=root;self.cast=cast
        self.max_step=s['maxStep'];self.foot_speed=s['footSpeed'];self.foot_radius=s['footRadius'];self.prediction=s['prediction']
        self.foot_rotation_weight=s['footRotationWeight'];self.foot_rotation_speed=s['footRotationSpeed'];self.max_foot_rotation_angle=s['maxFootRotationAngle']
        self.pelvis_speed=s['pelvisSpeed'];self.pelvis_damper=s['pelvisDamper'];self.lower=s['lowerPelvisWeight'];self.lift=s['liftPelvisWeight']
        self.legs=[Leg(self,foot) for foot in feet];self.pelvis=Pelvis(self);self.root_grounded=False;self.grounded=False
    def update(self):
        self.foot_radius=clamp(self.foot_radius,9.999999747378752e-05,self.max_step);self.pelvis_damper=clamp(self.pelvis_damper,0,1)
        self.max_foot_rotation_angle=clamp(self.max_foot_rotation_angle,0,90)
        hit=self.cast(add(self.root.position,mul(UP,self.max_step)),self.max_step*11)
        self.root_grounded=(hit.distance if hit else self.max_step*11)<self.max_step*2
        highest=-INF;lowest=INF;self.grounded=False
        for leg in self.legs:
            leg.process()
            if leg.ik_offset>highest:highest=leg.ik_offset
            if leg.ik_offset<lowest:lowest=leg.ik_offset
            if leg.grounded:self.grounded=True
        self.pelvis.process(-highest*self.lower,-lowest*self.lift,self.grounded)
class Grounder:
    def __init__(self,settings,root,feet,cast):
        self.solver=Grounding(settings,root,feet,cast);self.spine_bend=settings['spine_bend'];self.spine_speed=settings['spine_speed']
        self.horizontal=settings['spine_horizontal'];self.vertical=settings['spine_vertical'];self.spine_offset=(0.,0.,0.)
    def on_solver_update(self,enabled,weight):
        weight=clamp(weight,0,1)
        if not enabled or weight<=0:return None
        g=self.solver;g.update();pelvis=mul(mul(UP,g.pelvis.height),weight);offsets=[];rotations=[];target=(0.,0.,0.)
        for leg in g.legs:
            moved=add(leg.transform.position,pelvis)                 # the pelvis write has moved the foot bone
            offsets.append(mul(sub(leg.ik_position,moved),weight));rotations.append(slerp(IDENTITY,leg.rotation_offset,weight))
            tangent=sub(moved,g.root.position);tangent=(tangent[0],0.,tangent[2])
            facing=(dot(g.root.forward,normalized(tangent))+1)*.5
            target=add(target,mul(mul(tangent,mag(sub(leg.ik_position,moved))),facing))
        spine=[(0.,0.,0.)]*9
        if self.spine_bend!=0:
            wanted=mul(mul(target,weight),self.spine_bend);t=clamp(Time.delta*max(0.,self.spine_speed),0,1)
            self.spine_offset=add(self.spine_offset,mul(sub(wanted,self.spine_offset),t));vertical=mul(g.root.up,mag(self.spine_offset))
            spine=[add(mul(self.spine_offset,h),mul(vertical,v)) for h,v in zip(self.horizontal,self.vertical)]
        return pelvis,offsets,rotations,spine

# Terrain shared with the C harness: two sinusoids, and no ground at all where x > pit.
def terrain(p,x,z):return p[0]+p[1]*math.sin(p[2]*x+p[3])+p[4]*math.sin(p[5]*z+p[6])
def caster(p):
    def cast(origin,limit):
        if origin[0]>p[7]:return None
        y=terrain(p,origin[0],origin[2]);distance=origin[1]-y
        return Hit((origin[0],y,origin[2]),UP,distance) if 0<=distance<=limit else None
    return cast
def scenario(rng,index):
    p=[rng.uniform(-.2,.2),rng.uniform(0,.25),rng.uniform(.5,2.5),rng.uniform(0,6),rng.uniform(0,.25),rng.uniform(.5,2.5),rng.uniform(0,6),
       rng.uniform(1.5,4) if index%5==4 else 1e9]
    yaw=rng.uniform(0,6.28);x=rng.uniform(-1,1);z=rng.uniform(-1,1);speed=rng.uniform(0,5);hover=rng.choice((0.,0.,.15,-.1,.9));time=0.;frames=[]
    for k in range(70):
        dt=0. if k and rng.random()<.04 else rng.uniform(.012,.1);time+=dt
        yaw+=rng.uniform(-.2,.2)*dt*5;forward=(math.sin(yaw),0.,math.cos(yaw));right=(math.cos(yaw),0.,-math.sin(yaw))
        x+=forward[0]*speed*dt;z+=forward[2]*speed*dt;y=terrain(p,x,z)+hover+rng.uniform(-.01,.01)
        feet=[]
        for side in (-1,1):
            stride=.25*math.sin(time*7+side);lift=.08+.1*max(0.,math.cos(time*7+side))
            feet.append((x+right[0]*.1*side+forward[0]*stride,y+lift,z+right[2]*.1*side+forward[2]*stride))
        frames.append(dict(time=time,delta=dt if k else rng.uniform(.012,.1),root=(x,y,z),forward=forward,right=right,feet=feet,
                           enabled=rng.random()>.03,weight=rng.choice((1.,1.,1.,.5,0.,1.3))))
    return p,frames

HARNESS=r'''
#include "pe_grounder.h"
#include <math.h>
#include <stdio.h>
static float p[8];
static bool ray(void *context,PeVec3 o,PeVec3 d,float limit,PeGrounderHit *hit){
    (void)context;(void)d;if(o.x>p[7])return false;
    float y=p[0]+p[1]*sinf(p[2]*o.x+p[3])+p[4]*sinf(p[5]*o.z+p[6]),distance=o.y-y;
    if(distance<0||distance>limit)return false;
    *hit=(PeGrounderHit){.point={o.x,y,o.z},.normal={0,1,0},.distance=distance};return true;
}
static int v3(PeVec3 *v){return scanf("%f%f%f",&v->x,&v->y,&v->z)==3;}
int main(void){
    unsigned cases;if(scanf("%u",&cases)!=1)return 2;
    for(unsigned c=0;c<cases;c++){
        unsigned frames;for(int i=0;i<8;i++)if(scanf("%f",&p[i])!=1)return 2;
        if(scanf("%u",&frames)!=1)return 2;
        PeGrounder g={0};
        for(unsigned k=0;k<frames;k++){
            PeGrounderInput in={.ray=ray,.root_up={0,1,0}};int enabled;PeGrounderOutput out;
            if(scanf("%f%f%d%f",&in.time,&in.delta_time,&enabled,&in.weight)!=4||!v3(&in.root_position)||!v3(&in.root_forward)||!v3(&in.root_right)||!v3(&in.foot[0])||!v3(&in.foot[1]))return 2;
            in.enabled=enabled;
            if(!k){
                PeGrounderStart start={.delta_time=in.delta_time,.root_position=in.root_position,.foot={in.foot[0],in.foot[1]}};
                if(!pe_grounder_init(&g,pe_grounder_female_settings(),start))return 3;
                continue;
            }
            if(!pe_grounder_update(&g,&in,&out))return 4;
            printf("%d %.9g",out.ran,out.pelvis_offset.y);
            for(int i=0;i<2;i++)printf(" %.9g %.9g %.9g %.9g %.9g %.9g %.9g",out.foot_offset[i].x,out.foot_offset[i].y,out.foot_offset[i].z,
                out.foot_rotation[i].x,out.foot_rotation[i].y,out.foot_rotation[i].z,out.foot_rotation[i].w);
            for(int e=1;e<3;e++)printf(" %.9g %.9g %.9g",out.effector_offset[e].x,out.effector_offset[e].y,out.effector_offset[e].z);
            printf(" %.9g %.9g %.9g\n",out.effector_offset[0].x,out.effector_offset[5].y,out.effector_offset[7].z);
        }
    }
    return 0;
}
'''

class Portable(unittest.TestCase):
    def test_evidence_binds_ported_methods(self):
        evidence=e.load_evidence();part=evidence['assemblies']['Assembly-CSharp.dll'];methods=part['methods']
        self.assertEqual(len(methods),77);self.assertEqual(part['rea_evidence_id'],'ev_13b79477fb6033537478536807957a9437d4a82d8da7d9d87b9ef1ef27ee7243')
        def body(type_token,name):
            match=[m for m in methods if m['declaring_type_token']==type_token and m['name']==name]
            self.assertEqual(len(match),1,name);return cil_disasm.decode(bytes.fromhex(match[0]['raw_il_hex']))
        literals=lambda ops:[v for _,m,v in ops if m=='ldc.r4']
        calls=lambda ops:[v for _,m,v in ops if m in ('call','callvirt')]
        process=body('0x020005de','Process')
        # The foot offset takes Interp.LerpValue; rays: one for Fastest, three for Simple, one beside Best's capsule.
        self.assertIn(0x06002776,calls(process));self.assertEqual(calls(process).count(0x06002391),5)
        self.assertEqual(literals(body('0x020005de','GetRaycastHit')),[2.0,0.0,3.0])
        self.assertIn(0.01745329238474369,literals(body('0x020005de','RotateNormal')))
        self.assertEqual(literals(body('0x020005df','Process'))[-3:],[0.0,1.0,10.0])
        self.assertEqual(literals(body('0x020005dc','get_rootGrounded')),[2.0])
        # OnSolverUpdate: Grounding.Update, then both SetLegIK calls.
        update=calls(body('0x020005d7','OnSolverUpdate'));order=[update.index(0x06002374)]+[i for i,t in enumerate(update) if t==0x06002343]
        self.assertEqual(len(order),3);self.assertEqual(order,sorted(order))
        self.assertEqual(literals(body('0x020005d5','GetLegSpineBendVector')),[1.0,0.5])

    def test_changed_evidence_is_rejected(self):
        original=json.loads((e.ROOT/e.EVIDENCE).read_text())
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/'docs/reference').mkdir(parents=True)
            for change in ('il','field','type'):
                data=copy.deepcopy(original);part=data['assemblies']['Assembly-CSharp.dll']
                if change=='il':part['methods'][5]['raw_il_hex']='00'+part['methods'][5]['raw_il_hex'][2:]
                elif change=='field':part['fields'][3]['flags']^=1
                else:part['type_names']['0x020005dc']='Other'
                (root/e.EVIDENCE).write_text(json.dumps(data))
                with self.assertRaises(ValueError):e.load_evidence(root)

    def test_generated_metadata_is_strict_and_scoped(self):
        data=json.loads((GENERATED/'pe_grounder_source.json').read_text());s=data['settings']
        self.assertEqual((s['component'],s['root'],s['pelvis'],s['feet'],s['layers']),(11400014,'Model','Bip01',['Bip01 L Foot','Bip01 R Foot'],620801))
        self.assertEqual((s['solver']['quality'],s['solver']['rotateSolver'],s['spine_bend'],s['spine_speed']),(1,0,4.5,2.0))
        self.assertEqual([i for i,v in enumerate(s['spine_horizontal']) if v],[1,2]);self.assertFalse(any(s['spine_vertical']))
        self.assertTrue(any('flat ground' in x for x in data['limitations']) and any('callback' in x for x in data['limitations']))
        self.assertNotIn(str(e.ROOT),json.dumps(data))

    @unittest.skipUnless(shutil.which('cc'),'C compiler unavailable')
    def test_c_grounder_against_second_transcription(self):
        library=e.ROOT/'rebuild/build/obj/core/libpe_core.a'
        if not library.is_file():self.skipTest('Built core library unavailable')
        settings=json.loads((GENERATED/'pe_grounder_source.json').read_text())['settings'];rng=random.Random(20261013)
        cases=[scenario(rng,i) for i in range(40)];fmt=lambda values:' '.join('%.9g'%v for v in values);lines=[str(len(cases))]
        for p,frames in cases:
            lines+=[fmt(p),str(len(frames))]
            for f in frames:lines.append(fmt([f['time'],f['delta'],int(f['enabled']),f['weight']]+list(f['root'])+list(f['forward'])+list(f['right'])+list(f['feet'][0])+list(f['feet'][1])))
        with tempfile.TemporaryDirectory(dir=e.ROOT/'rebuild/build') as folder:
            source=Path(folder)/'harness.c';binary=Path(folder)/'harness';source.write_text(HARNESS)
            subprocess.run(['cc','-std=c11','-O2','-I',str(e.ROOT/'rebuild/src'),str(source),str(library),'-lm','-o',str(binary)],check=True)
            result=subprocess.run([str(binary)],input='\n'.join(lines)+'\n',capture_output=True,text=True,check=True)
        rows=iter([[float(v) for v in line.split()] for line in result.stdout.splitlines()]);worst=0.;worst_turn=0.;ran=moved=tilted=airborne=0
        for p,frames in cases:
            root=Root();feet=[Foot(),Foot()];grounder=None
            for k,f in enumerate(frames):
                Time.time=f['time'];Time.delta=f['delta'];root.position=f['root'];root.forward=f['forward'];root.right=f['right']
                for foot,position in zip(feet,f['feet']):foot.position=position
                if not k:grounder=Grounder(settings,root,feet,caster(p));continue
                expected=grounder.on_solver_update(f['enabled'],f['weight']);row=next(rows)
                self.assertEqual(bool(row[0]),expected is not None)
                if expected is None:continue
                pelvis,offsets,rotations,spine=expected;ran+=1;got=row[1:]
                values=[pelvis[1]]+[x for i in range(2) for x in offsets[i]]
                actual=[got[0]]+got[1:4]+got[8:11]
                worst=max(worst,max(abs(a-b) for a,b in zip(values,actual)))
                for i in range(2):worst_turn=max(worst_turn,1-abs(dot(rotations[i],got[4+7*i:8+7*i])))
                worst=max(worst,max(abs(a-b) for a,b in zip(list(spine[1])+list(spine[2])+[spine[0][0],spine[5][1],spine[7][2]],got[15:24])))
                moved+=abs(offsets[0][1])>.01;tilted+=abs(rotations[0][3])<.9995;airborne+=not grounder.solver.root_grounded
        self.assertRaises(StopIteration,next,rows)
        # float32 C against a double replay over drifting state; a transcription slip is centimetres.
        self.assertLess(worst,2e-5);self.assertLess(worst_turn,1e-6)
        self.assertGreater(ran,2000);self.assertGreater(moved,400);self.assertGreater(tilted,400);self.assertGreater(airborne,300)

@unittest.skipUnless(HAVE_UNITYPY and e.MANAGED.is_dir(),'UnityPy or the installed game unavailable')
class Installed(unittest.TestCase):
    def test_regeneration_matches_installed_bytes_and_prefab(self):
        for name,data in e.build_outputs().items():self.assertEqual((GENERATED/name).read_bytes(),data,name)

    def test_changed_prefab_grounder_is_rejected(self):
        model=e.read_objects(e.ROOT/e.MODEL)
        for change in ('quality','rotate','guid','bend','binding','spine'):
            copy_model=copy.deepcopy(model);data=copy_model[e.GROUNDER]['data']
            if change=='quality':data['solver']['quality']=2
            elif change=='rotate':data['solver']['rotateSolver']=1
            elif change=='guid':copy_model[e.GROUNDER]['script_guid']='0'*32
            elif change=='bend':data['spineBend']=2.0
            elif change=='binding':data['ik']['m_PathID']=0
            else:data['spine'][0]['horizontalWeight']=.5
            with self.assertRaises(ValueError):e.recover_settings(copy_model)

if __name__=='__main__':unittest.main()
