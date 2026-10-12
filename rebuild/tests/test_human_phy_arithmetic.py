#!/usr/bin/env python3
"""Compare compiled HumanPhyCtrl requests with an independent double oracle."""
import math
from pathlib import Path
import random
import struct
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
GRAVITY,MAX_SUB,MAX_SPEED,STOP=-9.8100004196167,10.0,50.0,0.05000000074505806


def f32(value):
    return struct.unpack('f',struct.pack('f',value))[0]


def length(v): return math.sqrt(sum(x*x for x in v))
def unit(v):
    m=length(v)
    return [x/m for x in v] if m>1e-5 else [0.0]*3
def planar(v,n):
    d=sum(a*b for a,b in zip(v,n))/sum(x*x for x in n)
    return [a-b*d for a,b in zip(v,n)]


def request_oracle(steps):
    total=[0.0]*3
    for step in steps:
        total=[a+b for a,b in zip(total,step)]
        if length(total)>MAX_SUB: total=[MAX_SUB*x for x in unit(total)]
    return total


def force_oracle(c):
    """Written from the audit's prose order, in doubles; a tolerance check only."""
    own=[x*c['speed'] for x in c['direction']] if length(c['direction'])>=1e-5 else [0.0]*3
    desired=[(a+b)*c['scale'] for a,b in zip(own,c['applied'])]; v=c['velocity']; change=[0.0]*3
    moving=length(desired)>=1e-5
    if moving:
        change=[a-b for a,b in zip(desired,v)] if c['whole'] else \
            [a-b for a,b in zip(planar(desired,c['up']),planar(v,c['up']))]
    if length(v)>100: change=[-x for x in v]
    acceleration=[GRAVITY*g+s for g,s in zip(c['gravity_up'],c['sub'])]
    if not moving: acceleration=[a-c['drag']*length(v)**2*u for a,u in zip(acceleration,unit(v))]
    if length(own)<1e-5 and length(c['sub'])>=1e-5 and length(v)<STOP: change=[a-b for a,b in zip(change,v)]
    if length(v)>MAX_SPEED: change=[a-u*(length(v)-MAX_SPEED) for a,u in zip(change,unit(v))]
    return change+acceleration


class HumanPhyArithmetic(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory=tempfile.TemporaryDirectory(prefix='human-phy-arithmetic-')
        cls.addClassCleanup(cls.directory.cleanup)
        stage=Path(cls.directory.name); source=stage/'probe.c'; cls.binary=stage/'probe'
        source.write_text('''#include "pe_human_phy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int vec(PeVec3 *v){return scanf("%f %f %f",&v->x,&v->y,&v->z)==3;}
int main(void){char kind[8];
    while(scanf("%7s",kind)==1){
        PeHumanPhy p={0};pe_human_phy_init(&p);
        if(!strcmp(kind,"req")){int n;PeVec3 v;assert(scanf("%d",&n)==1);
            while(n--){assert(vec(&v)&&pe_human_phy_apply_move_request(&p,v));}
            printf("%.9g %.9g %.9g\\n",p.request_velocity.x,p.request_velocity.y,p.request_velocity.z);continue;}
        int whole;PeHumanPhyStep s={.fixed_delta_time=.02f};PeHumanPhyForces out;
        assert(scanf("%d %f %f %f",&whole,&p.move_speed,&p.net_move_speed_scale,&p.drag)==4);
        assert(vec(&p.desired_direction)&&vec(&p.applied_velocity)&&vec(&s.velocity)&&vec(&s.up));
        assert(vec(&p.gravity_up)&&vec(&p.sub_acc));p.phy_ground=whole;
        assert(pe_human_phy_fixed_update(&p,s,&out)&&out.ran&&!out.set_position);
        printf("%.9g %.9g %.9g %.9g %.9g %.9g\\n",out.velocity_change.x,out.velocity_change.y,
            out.velocity_change.z,out.acceleration.x,out.acceleration.y,out.acceleration.z);
    }return 0;}
''')
        library=ROOT/'build/obj/core/libpe_core.a'
        if not library.is_file(): raise AssertionError('Build libpe_core.a before this test')
        subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Wpedantic',
                        '-I'+str(ROOT/'src'),str(source),str(library),'-lm','-o',str(cls.binary)],check=True,capture_output=True,text=True)

    def run_probe(self,lines):
        result=subprocess.run([str(self.binary)],input='\n'.join(lines)+'\n',text=True,capture_output=True,check=True)
        rows=[list(map(float,row.split())) for row in result.stdout.splitlines()]
        self.assertEqual(len(rows),len(lines)); return rows

    def near(self,actual,expected,scale):
        for a,b in zip(actual,expected): self.assertAlmostEqual(a,b,delta=2e-4+scale*2e-5)

    def test_request_sums_and_clamp(self):
        rng=random.Random(20261012); cases=[]
        for _ in range(400):
            cases.append([[f32(rng.uniform(-9,9)) for _ in range(3)] for _ in range(rng.randint(1,5))])
        cases+=[[[0,0,10]],[[0,0,f32(10.001)]],[[6,0,8],[6,0,8]],[[0,0,16],[0,0,-10]]]
        lines=['req %d '%len(c)+' '.join('%.9g'%x for step in c for x in step) for c in cases]
        clamped=0
        for case,row in zip(cases,self.run_probe(lines)):
            expected=request_oracle(case); clamped+=abs(length(expected)-MAX_SUB)<1e-9
            self.near(row,expected,10); self.assertLessEqual(length(row),MAX_SUB+1e-4)
        self.assertGreater(clamped,100); self.assertLess(clamped,len(cases)-50)

    def test_fixed_update_forces(self):
        rng=random.Random(750); cases=[]
        def vector(limit,zero=.25):
            return [0.0]*3 if rng.random()<zero else [f32(rng.uniform(-limit,limit)) for _ in range(3)]
        for index in range(900):
            tilt=unit([rng.uniform(-.4,.4),1,rng.uniform(-.4,.4)])
            velocity=vector(rng.choice((.03,8,8,8,70,130)),.1)
            cases.append(dict(whole=index%2,speed=f32(rng.uniform(.1,9)),scale=f32(rng.choice((1,1,.5))),
                drag=f32(rng.choice((1.5,1.3,.006))),direction=unit(vector(1,.5)),applied=vector(10,.4),
                velocity=velocity,up=[f32(x) for x in rng.choice(([0,1,0],tilt))],
                gravity_up=[f32(x) for x in rng.choice(([0,1,0],tilt))],sub=vector(.5,.7)))
        for c in cases: c['direction']=[f32(x) for x in c['direction']]
        lines=['fix %d %.9g %.9g %.9g '%(c['whole'],c['speed'],c['scale'],c['drag'])+' '.join(
            '%.9g'%x for k in ('direction','applied','velocity','up','gravity_up','sub') for x in c[k]) for c in cases]
        seen=set()
        for c,row in zip(cases,self.run_probe(lines)):
            expected=force_oracle(c); speed=length(c['velocity'])
            seen.add((c['whole'],speed>100,speed>50,length(expected[:3])<1e-9))
            with self.subTest(case=c): self.near(row,expected,max(10,speed*speed*c['drag']))
        self.assertGreaterEqual(len(seen),8) # grounded/air x overspeed/limiter/idle branches all sampled


if __name__=='__main__': unittest.main()
