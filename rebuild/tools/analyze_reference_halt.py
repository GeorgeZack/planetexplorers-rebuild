#!/usr/bin/env python3
"""Fit pe_halt to the effector offsets in an observed FullBodyBipedIK capture.
Offline; never launches anything.

The capture holds what every offset writer added, not the velocity Halt began
with. Each stop is therefore fitted: foot, start frame, speed and heading are
chosen to minimize the error on the channels only the Halt effect writes
(body, hands and vertical shoulder offsets). Agreement shows the curves and
timing match the original; it is not an independent prediction."""
import argparse
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]
LIBRARY=ROOT/'rebuild/build/obj/core/libpe_core.a'
# (effector, axis): GrounderFBBIK writes feet and horizontal shoulder offsets, so those are excluded.
PURE=((0,0),(0,1),(0,2),(5,0),(5,1),(5,2),(6,0),(6,1),(6,2),(1,1),(2,1))
SHARED=((7,0),(7,1),(7,2),(8,0),(8,1),(8,2),(1,0),(1,2),(2,0),(2,2))
QUIET=1e-4
HARNESS=r'''
#include "pe_halt.h"
#include <math.h>
#include <stdio.h>
int main(void){
    float vx,vz;int right,n;
    while(scanf("%f%f%d%d",&vx,&vz,&right,&n)==4){
        PeHalt a={0};PeHaltUpdate u;float m=sqrtf(vx*vx+vz*vz);PeVec3 d={vx/m,0,vz/m};
        PeHaltStart s={.effect_present=true,.motor_present=true,.velocity={vx,0,vz}};
        /* The right foot steps when the vector from it to the left foot opposes the motion. */
        s.left_foot=right?(PeVec3){-d.x,0,-d.z}:(PeVec3){d.x,0,d.z};
        bool live=pe_halt_begin(&a,s,&u);
        printf("%.9g %.9g\n",a.weight,a.step_time);
        for(int i=0;i<n;i++){
            float dt,o[27]={0};if(scanf("%f",&dt)!=1)return 2;
            if(live){
                PeHaltTick t={.delta_time=dt,.effect_present=true,.motor_present=true,.effect_enabled=true,.master_weight=1};
                if(!pe_halt_step(&a,t,&u))live=false;
                else{
                    for(unsigned k=0;k<u.offset_count;k++){unsigned e=u.offsets[k].effector;o[e*3]=u.offsets[k].offset.x;o[e*3+1]=u.offsets[k].offset.y;o[e*3+2]=u.offsets[k].offset.z;}
                    if(u.ended)live=false;
                }
            }
            for(int k=0;k<27;k++)printf("%.9g ",o[k]);
            printf("\n");
        }
    }
    return 0;
}
'''

def require(condition,message):
    if not condition:raise ValueError(message)

class Replay:
    def __init__(self,build=None):
        require(LIBRARY.is_file(),'Build the core library first: make -C rebuild')
        self.folder=tempfile.TemporaryDirectory(dir=build or ROOT/'rebuild/build');folder=Path(self.folder.name)
        source=folder/'harness.c';self.binary=folder/'harness';source.write_text(HARNESS)
        subprocess.run(['cc','-std=c11','-O2','-I',str(ROOT/'rebuild/src'),str(source),str(LIBRARY),'-lm','-o',str(self.binary)],check=True)
    def close(self):self.folder.cleanup()
    def run(self,queries):
        """queries: (speed, heading degrees, right foot, frame times) -> (weight, step time, offset rows)"""
        text=''.join('%.9g %.9g %d %d %s\n'%(s*math.cos(math.radians(h)),s*math.sin(math.radians(h)),r,len(dts),' '.join('%.9g'%x for x in dts)) for s,h,r,dts in queries)
        lines=subprocess.run([str(self.binary)],input=text,capture_output=True,text=True,check=True).stdout.splitlines();results=[];at=0
        for query in queries:
            count=len(query[3]);weight,step=(float(x) for x in lines[at].split())
            results.append((weight,step,[[float(x) for x in line.split()] for line in lines[at+1:at+1+count]]));at+=1+count
        require(at==len(lines),'Halt harness returned a different row count');return results

def offset(frame,effector):return frame['effectors'][effector]['positionOffset']
def errors(rows,frames,channels):
    return [row[e*3+c]-offset(frame,e)[c] for row,frame in zip(rows,frames) for e,c in channels]
def active(frame):return any(abs(offset(frame,e)[c])>QUIET for e,c in PURE)

def stops(frames):
    found=[];i=0
    while i<len(frames):
        if active(frames[i]):
            j=i
            while j<len(frames) and active(frames[j]):j+=1
            found.append((i,j));i=j
        else:i+=1
    return found

def fit(replay,frames,first,last):
    best=None
    # Every Halt-only horizontal offset lies along the motion, in either sense depending on the curve.
    peak=max(frames[first:last],key=lambda f:math.hypot(offset(f,0)[0],offset(f,0)[2]))
    along=math.degrees(math.atan2(offset(peak,0)[2],offset(peak,0)[0]))
    headings=[along+turn+step for turn in (0,180) for step in range(-9,10,3)]
    # The frame Halt begins in contributes nothing, so the start is the last quiet frame or just before it.
    for start in range(max(0,first-2),first+1):
        window=frames[start:min(len(frames),last+2)];times=[f['dt'] for f in window];queries=[]
        for right in (0,1):
            for speed in (4.6+0.1*k for k in range(40)):
                for heading in headings:queries.append((speed,heading,right,times))
        for query,(weight,step,rows) in zip(queries,replay.run(queries)):
            score=sum(x*x for x in errors(rows,window,PURE))
            if best is None or score<best[0]:best=(score,start,query,weight,step,rows)
    score,start,(speed,heading,right,times),weight,step,rows=best;window=frames[start:start+len(times)]
    for span_speed,span_heading in ((0.1,3.),(0.01,0.3)):
        queries=[(speed+span_speed*a/10,heading+span_heading*b/10,right,times) for a in range(-10,11) for b in range(-10,11)]
        for query,(w,s,r) in zip(queries,replay.run(queries)):
            candidate=sum(x*x for x in errors(r,window,PURE))
            if candidate<score:score,speed,heading,weight,step,rows=candidate,query[0],query[1],w,s,r
    pure=errors(rows,window,PURE);shared=errors(rows,window,SHARED)
    return dict(independent=independent(frames,start,heading),first_frame=frames[first]['sample'],frames=last-first,start_frame=frames[start]['sample'],foot='right' if right else 'left',
        fitted_speed=round(speed,3),fitted_heading_degrees=round(heading%360,2),weight=weight,step_time=step,
        halt_only_error_m=dict(rms=math.sqrt(sum(x*x for x in pure)/len(pure)),max=max(abs(x) for x in pure)),
        grounder_shared_error_m=dict(rms=math.sqrt(sum(x*x for x in shared)/len(shared)),max=max(abs(x) for x in shared)),
        observed_peak_shoulder_up_m=max(abs(offset(f,e)[1]) for f in frames[first:last] for e in (1,2)),
        observed_peak_body_m=max(math.sqrt(sum(x*x for x in offset(f,0))) for f in frames[first:last]))

def independent(frames,start,heading):
    """Foot rule and velocity from observed node positions, which the fit never sees."""
    if start<4 or any('nodes' not in f for f in frames[start-4:start+1]):return None
    left,right=frames[start-1]['nodes'][9],frames[start-1]['nodes'][12];h=math.radians(heading)
    side=(left[0]-right[0])*math.cos(h)+(left[2]-right[2])*math.sin(h)
    before,after=frames[start-4]['nodes'][0],frames[start-1]['nodes'][0];elapsed=sum(f['dt'] for f in frames[start-3:start])
    velocity=[(after[i]-before[i])/elapsed for i in (0,2)]
    return dict(foot_from_observed_feet='right' if side<0 else 'left',body_speed=round(math.hypot(*velocity),2),
                body_heading_degrees=round(math.degrees(math.atan2(velocity[1],velocity[0]))%360,1))

def analyze(path,build=None):
    header=None;frames=[]
    with Path(path).open() as stream:
        for line in stream:
            item=json.loads(line);kind=item.get('type')
            if kind=='header':header=item
            elif kind=='frame':frames.append(item)
    require(header and header.get('kind')=='observed-original-fbbik' and frames,'Not an FBBIK capture')
    require(all(len(f['effectors'])==9 and f['dt']>0 for f in frames),'Malformed capture frame')
    replay=Replay(build)
    try:fitted=[fit(replay,frames,a,b) for a,b in stops(frames) if a>0 and b<len(frames)]
    finally:replay.close()
    checked=[s for s in fitted if s['independent']]
    return dict(schema='pe-halt-comparison-1',capture=str(path),frames=len(frames),stops=fitted,
        foot_rule_agreement='%d of %d'%(sum(s['independent']['foot_from_observed_feet']==s['foot'] for s in checked),len(checked)),
        worst_halt_only_error_m=max((s['halt_only_error_m']['max'] for s in fitted),default=None),
        scope='Speed, heading, foot and start frame are fitted per stop; the capture does not hold the starting velocity. '
              'Halt-only channels are body, hands and vertical shoulder offsets.')

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('capture',type=Path);parser.add_argument('--output',type=Path)
    a=parser.parse_args(argv)
    try:
        text=json.dumps(analyze(a.capture),indent=2)+'\n'
        if a.output:
            with a.output.open('x') as stream:stream.write(text)
        print(text,end='')
    except (OSError,ValueError,KeyError,TypeError,subprocess.CalledProcessError,json.JSONDecodeError) as error:
        print('Halt comparison: %s'%error,file=sys.stderr);return 1
    return 0
if __name__=='__main__':raise SystemExit(main())
