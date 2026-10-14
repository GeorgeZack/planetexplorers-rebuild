#!/usr/bin/env python3
"""Replay an observed GrounderFBBIK capture through pe_grounder and report the
difference from the original's own state and requests. Offline; never launches
anything."""
import argparse
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]
SETTINGS=ROOT/'rebuild/generated/pe_grounder_source.json'
LIBRARY=ROOT/'rebuild/build/obj/core/libpe_core.a'
FEET=(7,8)   # FullBodyBipedEffector.LeftFoot, RightFoot
# The serialized mask 620801 by Pathea.Layer (WhiteCat/PE-VC/Layer.cs) and ProjectSettings/TagManager.asset.
SOURCE_LAYERS={0:'Default',8:'AIPlayer',11:'SceneStatic',12:'VFVoxelTerrain',13:'Building',14:'TreeStatic',16:'Unwalkable',19:'GIEProductLayer'}
UNEVEN=0.03  # a foot request this large means the capture left flat ground
HARNESS=r'''
#include "pe_grounder.h"
#include <math.h>
#include <stdio.h>
typedef struct {PeVec3 origin,point,normal; float distance; int hit;} Ray;
typedef struct {Ray ray[7]; unsigned count,used; float gap; int overrun;} Rays;
static int v3(PeVec3 *v){return scanf("%f%f%f",&v->x,&v->y,&v->z)==3;}
static int q4(PeFbbikQuat *q){return scanf("%f%f%f%f",&q->x,&q->y,&q->z,&q->w)==4;}
static void p3(PeVec3 v){printf("%.9g %.9g %.9g ",v.x,v.y,v.z);}
static void p4(PeFbbikQuat q){printf("%.9g %.9g %.9g %.9g ",q.x,q.y,q.z,q.w);}
/* Serves the recorded hits in the order the original casts them. */
static bool cast(void *context,PeVec3 origin,PeVec3 direction,float limit,PeGrounderHit *hit){
    Rays *r=context;(void)direction;(void)limit;
    if(r->used>=r->count){r->overrun=1;return false;}
    const Ray *x=&r->ray[r->used++];
    float gap=sqrtf((origin.x-x->origin.x)*(origin.x-x->origin.x)+(origin.y-x->origin.y)*(origin.y-x->origin.y)+(origin.z-x->origin.z)*(origin.z-x->origin.z));
    if(gap>r->gap)r->gap=gap;
    if(!x->hit)return false;
    hit->point=x->point;hit->normal=x->normal;hit->distance=x->distance;return true;
}
int main(void){
    unsigned frames;PeGrounder g={0};
    if(scanf("%u",&frames)!=1||!pe_grounder_init(&g,pe_grounder_female_settings(),(PeGrounderStart){0}))return 2;
    for(unsigned f=0;f<frames;f++){
        int seed;Rays rays={0};PeGrounderInput in={.enabled=true,.ray=cast,.context=&rays};PeGrounderOutput out;
        /* Motion_Move_Human and IKCmpt rewrite these two settings while playing. */
        if(scanf("%f%f",&g.settings.spine_bend,&g.settings.max_foot_rotation_angle)!=2)return 3;
        if(scanf("%d%f%f%f",&seed,&in.time,&in.delta_time,&in.weight)!=4||!v3(&in.root_position)||!v3(&in.root_forward)||!v3(&in.root_right)||!v3(&in.root_up)||
           !v3(&in.foot[0])||!v3(&in.foot[1]))return 3;
        if(seed){
            for(unsigned i=0;i<PE_GROUNDER_LEGS;i++){
                PeGrounderLeg *l=&g.leg[i];
                if(scanf("%f",&l->last_time)!=1||!v3(&l->last_position)||scanf("%f",&l->ik_offset)!=1||!q4(&l->r)||!q4(&l->rotation_offset))return 3;
            }
            if(scanf("%f%f%f",&g.pelvis_last_time,&g.pelvis_height,&g.pelvis_damper)!=3||!v3(&g.pelvis_last_root)||!v3(&g.spine_offset))return 3;
        }
        if(scanf("%u",&rays.count)!=1||rays.count>7)return 3;
        for(unsigned i=0;i<rays.count;i++){
            Ray *x=&rays.ray[i];
            if(!v3(&x->origin)||scanf("%d",&x->hit)!=1||!v3(&x->point)||!v3(&x->normal)||scanf("%f",&x->distance)!=1)return 3;
        }
        if(!pe_grounder_update(&g,&in,&out)){puts("failed");continue;}
        printf("%d %d %d %u %d %.9g ",out.ran,out.grounded,out.root_grounded,rays.used,rays.overrun,rays.gap);
        p3(out.pelvis_offset);
        for(unsigned i=0;i<PE_GROUNDER_LEGS;i++){p3(out.foot_offset[i]);p4(out.foot_rotation[i]);}
        for(unsigned e=0;e<PE_FBBIK_EFFECTORS;e++)p3(out.effector_offset[e]);
        for(unsigned i=0;i<PE_GROUNDER_LEGS;i++){
            const PeGrounderLeg *l=&g.leg[i];
            printf("%.9g %.9g %d ",l->ik_offset,l->height_from_ground,l->grounded);p3(l->ik_position);p4(l->r);p4(l->rotation_offset);
        }
        printf("%.9g %.9g ",g.pelvis_height,g.pelvis_damper);p3(g.spine_offset);puts("");
    }
    return 0;
}
'''

def require(value,message):
    if not value:raise ValueError(message)
def sub(a,b):return [x-y for x,y in zip(a,b)]
def gap(a,b):return math.sqrt(sum((x-y)**2 for x,y in zip(a,b)))
def qmul(a,b):
    return (a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],a[3]*b[1]+a[1]*b[3]+a[2]*b[0]-a[0]*b[2],
            a[3]*b[2]+a[2]*b[3]+a[0]*b[1]-a[1]*b[0],a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2])
def turn(a,b):
    """Angle in degrees between two rotations; a zero quaternion counts as identity."""
    na=math.sqrt(sum(x*x for x in a));nb=math.sqrt(sum(x*x for x in b))
    if na<1e-6:a=(0,0,0,1);na=1
    if nb<1e-6:b=(0,0,0,1);nb=1
    return math.degrees(2*math.acos(min(1.,abs(sum(x*y for x,y in zip(a,b)))/(na*nb))))
def stats(values):
    return dict(max=max(values),mean=sum(values)/len(values),p95=sorted(values)[int(.95*(len(values)-1))]) if values else None

def settings_differences(source,record):
    """Live component against the generated settings; values are printed single precision."""
    out=[]
    def same(label,live,want):
        if isinstance(want,float) or isinstance(live,float):
            if live is None or abs(live-want)>1e-6*max(1.,abs(want)):out.append('%s: live %r, prefab %r'%(label,live,want))
        elif live!=want:out.append('%s: live %r, prefab %r'%(label,live,want))
    same('weight',record['weight'],source['weight']);same('layers',record['layers'],source['layers'])
    same('spineBend',record['spineBend'],source['spine_bend']);same('spineSpeed',record['spineSpeed'],source['spine_speed'])
    for key,want in source['solver'].items():same(key,(1 if record[key] else 0) if isinstance(record[key],bool) else record[key],want)
    horizontal=[0.]*9;vertical=[0.]*9
    for entry in record['spine']:
        if 0<=entry['effector']<9:horizontal[entry['effector']]=entry['horizontal'];vertical[entry['effector']]=entry['vertical']
    for e in range(9):
        same('spine %d horizontal'%e,float(horizontal[e]),float(source['spine_horizontal'][e]));same('spine %d vertical'%e,float(vertical[e]),float(source['spine_vertical'][e]))
    same('root',record['root'],source['root']);same('pelvis',record['pelvis'],source['pelvis']);same('feet',record['feet'],source['feet'])
    if record['legTransforms']!=record['feet']:out.append('grounding legs %r differ from the component feet %r'%(record['legTransforms'],record['feet']))
    if record['footEffectors']!=record['feet']:out.append('foot effector bones %r differ from the component feet %r'%(record['footEffectors'],record['feet']))
    for bit,name in sorted(SOURCE_LAYERS.items()):
        if record['layerNames'][bit]!=name:out.append('layer %d: live %r, source %r'%(bit,record['layerNames'][bit],name))
    if not record['enabled']:out.append('the grounder was disabled at attach')
    if any(abs(s-1)>1e-4 for s in record['rootLossyScale']):out.append('the root is scaled: %r'%record['rootLossyScale'])
    return out

def lines(frames,seed_all,live):
    """live: per-frame (spineBend, maxFootRotationAngle), the two settings the game rewrites while playing."""
    fmt=lambda values:' '.join('%.9g'%v for v in values);out=[str(len(frames))]
    for index,f in enumerate(frames):
        seed=seed_all or index==0;root=f['root'];out.append(fmt(live[index]))
        out.append('%d %s'%(seed,fmt([f['time'],f['dt'],f['weight']]+root[0]+root[1]+root[2]+root[3]+f['legs'][0]['position']+f['legs'][1]['position'])))
        if seed:
            for leg in f['legs']:out.append(fmt([leg['lastTime']]+leg['lastPosition']+[leg['ikOffset']]+leg['r']+leg['rotationOffset']))
            p=f['pelvis'];out.append(fmt([p['lastTime'],p['heightOffset'],p['damperF']]+p['lastRootPosition']+f['spineOffset']))
        out.append(str(len(f['rays'])))
        for ray in f['rays']:
            hit=ray['hit'];out.append(fmt(ray['origin'])+' %d '%hit+fmt((ray['point']+ray['normal']+[ray['hitDistance']]) if hit else [0]*7))
    return out

def replays(frames,seed_all,runs,build=None):
    """One compile, one replay per entry of runs; each entry is the per-frame live settings."""
    require(LIBRARY.is_file(),'Build the core library first: make -C rebuild')
    out=[]
    with tempfile.TemporaryDirectory(dir=build or ROOT/'rebuild/build') as folder:
        source=Path(folder)/'harness.c';binary=Path(folder)/'harness';source.write_text(HARNESS)
        subprocess.run(['cc','-std=c11','-O2','-I',str(ROOT/'rebuild/src'),str(source),str(LIBRARY),'-lm','-o',str(binary)],check=True)
        for live in runs:
            result=subprocess.run([str(binary)],input='\n'.join(lines(frames,seed_all,live))+'\n',capture_output=True,text=True,check=True)
            rows=result.stdout.splitlines();require(len(rows)==len(frames),'Grounder harness returned a different frame count')
            out.append([None if row=='failed' else [float(v) for v in row.split()] for row in rows])
    return out
def replay(frames,seed_all,live,build=None):return replays(frames,seed_all,[live],build)[0]

def live_settings(frames,record,source):
    """Per-frame spineBend and maxFootRotationAngle. Recorded when the probe wrote them per frame;
    otherwise fitted, one update at a time, to the original's own leg rotations and spine offset."""
    if all('spineBend' in f and 'maxFootRotationAngle' in f for f in frames):
        return [(f['spineBend'],f['maxFootRotationAngle']) for f in frames],dict(source='recorded per frame',
            spine_bend_values=sorted({f['spineBend'] for f in frames}),max_foot_rotation_angle=stats([f['maxFootRotationAngle'] for f in frames]))
    count=len(frames);bends=sorted({0.,float(source['spine_bend']),float(record['spineBend'])})
    def leg_error(f,row):
        if row is None:return math.inf
        c=unpack(row);post=f['post']
        return max(max(turn(post['legs'][i]['r'],c['legs'][i]['r']),turn(post['legs'][i]['rotationOffset'],c['legs'][i]['rotation_offset'])) for i in range(2))
    def spine_error(f,row):
        # With no bend the update leaves spineOffset alone, so the requests have to be compared too.
        if row is None:return math.inf
        c=unpack(row);added=[sub(a,b) for a,b in zip(f['post']['offsets'],f['offsets'])]
        return max([gap(f['post']['spineOffset'],c['spine'])]+[gap(added[e],c['effectors'][e]) for e in range(9) if e not in FEET])
    grid=[x*.5 for x in range(181)];tilt=lambda f,i:turn(f['post']['legs'][i]['rotationOffset'],(0,0,0,1))
    runs=[[(bends[0],g)]*count for g in grid]+[[(bends[0],min(90.,tilt(f,i))) for f in frames] for i in range(2)]
    runs+=[[(b,float(record['maxFootRotationAngle']))]*count for b in bends]
    rows=replays(frames,True,runs);angle_runs=rows[:len(grid)+2];bend_runs=rows[len(grid)+2:]
    live=[];residual=[];width=[];chosen=[];bend_residual=[]
    for n,f in enumerate(frames):
        errors=[leg_error(f,run[n]) for run in angle_runs];best=min(range(len(errors)),key=errors.__getitem__)
        angle=runs[best][n][1];near=[grid[k] for k in range(len(grid)) if errors[k]<=errors[best]+.01]
        spine=[spine_error(f,run[n]) for run in bend_runs];b=min(range(len(bends)),key=spine.__getitem__)
        live.append((bends[b],angle));residual.append(errors[best]);width.append(max(near)-min(near) if near else 0.);chosen.append(angle);bend_residual.append(spine[b])
    pinned=[a for a,w in zip(chosen,width) if w<=1.]
    return live,dict(source='fitted: the probe recorded these two settings only at attach',
        attach=dict(spineBend=record['spineBend'],maxFootRotationAngle=record['maxFootRotationAngle']),
        spine_bend_frames={str(b):sum(1 for x in live if x[0]==b) for b in bends},spine_offset_residual_m=stats(bend_residual),
        max_foot_rotation_angle=stats(chosen),leg_rotation_residual_degrees=stats(residual),
        # A frame pins the limit only while a foot is actually clamped by it.
        frames_where_the_limit_is_pinned_within_1_degree=len(pinned),pinned_limit_degrees=stats(pinned))

def unpack(row):
    it=iter(row);take=lambda n:[next(it) for _ in range(n)]
    out=dict(ran=bool(next(it)),grounded=bool(next(it)),root_grounded=bool(next(it)),rays=int(next(it)),overrun=bool(next(it)),ray_gap=next(it),pelvis=take(3),feet=[])
    for _ in range(2):out['feet'].append(dict(offset=take(3),rotation=take(4)))
    out['effectors']=[take(3) for _ in range(9)];out['legs']=[]
    for _ in range(2):out['legs'].append(dict(ik_offset=next(it),height=next(it),grounded=bool(next(it)),ik_position=take(3),r=take(4),rotation_offset=take(4)))
    out['pelvis_height']=next(it);out['pelvis_damper']=next(it);out['spine']=take(3);return out

def compare(frames,rows):
    names=('leg_ik_offset_m','leg_ik_position_m','leg_height_from_ground_m','leg_rotation_degrees','foot_rotation_degrees','pelvis_height_m','pelvis_damper',
           'pelvis_bone_move_m','foot_effector_offset_m','other_effector_offset_m','spine_offset_m','ray_origin_m')
    errors={n:[] for n in names};failed=overruns=unused=flags=0;worst=None
    for f,row in zip(frames,rows):
        if row is None:failed+=1;continue
        c=unpack(row);post=f['post'];overruns+=c['overrun'];unused+=c['rays']!=len(f['rays'])
        flags+=(c['grounded']!=post['grounded'])+(c['root_grounded']!=post['rootGrounded'])+sum(c['legs'][i]['grounded']!=post['legs'][i]['grounded'] for i in range(2))
        added=[sub(a,b) for a,b in zip(post['offsets'],f['offsets'])]
        for i in range(2):
            live=post['legs'][i];mine=c['legs'][i]
            errors['leg_ik_offset_m'].append(abs(live['ikOffset']-mine['ik_offset']));errors['leg_ik_position_m'].append(gap(live['ikPosition'],mine['ik_position']))
            errors['leg_height_from_ground_m'].append(abs(live['heightFromGround']-mine['height']))
            errors['leg_rotation_degrees'].append(max(turn(live['r'],mine['r']),turn(live['rotationOffset'],mine['rotation_offset'])))
            errors['foot_rotation_degrees'].append(turn(live['rotation'],qmul(c['feet'][i]['rotation'],f['legs'][i]['rotation'])))
            errors['foot_effector_offset_m'].append(gap(added[FEET[i]],[a+b for a,b in zip(c['feet'][i]['offset'],c['effectors'][FEET[i]])]))
        errors['pelvis_height_m'].append(abs(post['pelvis']['heightOffset']-c['pelvis_height']));errors['pelvis_damper'].append(abs(post['pelvis']['damperF']-c['pelvis_damper']))
        errors['pelvis_bone_move_m'].append(gap(sub(post['pelvisBone'],f['pelvisBone']),c['pelvis']))
        errors['other_effector_offset_m'].append(max(gap(added[e],c['effectors'][e]) for e in range(9) if e not in FEET))
        errors['spine_offset_m'].append(gap(post['spineOffset'],c['spine']));errors['ray_origin_m'].append(c['ray_gap'])
        size=max(errors['leg_ik_position_m'][-1],errors['leg_ik_position_m'][-2],errors['pelvis_bone_move_m'][-1])
        if worst is None or size>worst[0]:worst=(size,f['sample'])
    report={n:stats(v) for n,v in errors.items()}
    report.update(failed_frames=failed,frames_with_more_rays_than_recorded=overruns,frames_with_a_different_ray_count=unused,grounded_flag_differences=flags,worst_sample=worst and worst[1])
    return report

def probe_rays(frames,step):
    """The probe's repeated rays against the two hits the original keeps: root and heel."""
    root=[];heel=[];misses=0;layers={};colliders={}
    for f in frames:
        rays=f['rays'];post=f['post'];require(len(rays) in (1,4,7),'Unexpected ray count in frame %d'%f['sample'])
        first=rays[0]
        if first['hit']:root.append(max(gap(first['point'],post['rootHit']['point']),abs(first['hitDistance']-post['rootHit']['distance'])))
        cursor=1
        for i in range(2):
            if f['time']==f['legs'][i]['lastTime']:continue   # Leg.Process returned before casting; heelHit is stale
            require(cursor+3<=len(rays),'Leg rays missing in frame %d'%f['sample']);ray=rays[cursor];cursor+=3
            want=ray['point'] if ray['hit'] else [ray['origin'][0],ray['origin'][1]-3*step,ray['origin'][2]]
            heel.append(gap(want,post['legs'][i]['heelPoint']))
        for ray in rays:
            if not ray['hit']:misses+=1;continue
            layers[ray['layer']]=layers.get(ray['layer'],0)+1;kind=ray['collider'].split(':',1)[0];colliders[kind]=colliders.get(kind,0)+1
    return dict(root_hit_m=stats(root),heel_hit_m=stats(heel),missed_rays=misses,hits_by_layer=layers,hits_by_collider_type=colliders)

def coverage(frames,record):
    offsets=[abs(leg['ikOffset']) for f in frames for leg in f['post']['legs']];heights=[leg['heightFromGround'] for f in frames for leg in f['post']['legs']]
    tilt=[];moved=[];adapter=[]
    for f in frames:
        move=sub(f['post']['pelvisBone'],f['pelvisBone']);moved.append(math.sqrt(sum(x*x for x in move)))
        for i in range(2):
            adapter.append(gap(sub(f['post']['legs'][i]['position'],f['legs'][i]['position']),move))
            tilt.append(turn(f['post']['legs'][i]['rotationOffset'],(0,0,0,1)))
    largest=max(offsets) if offsets else 0.
    return dict(foot_offset_m=stats(offsets),root_height_over_ground_under_feet_m=dict(min=min(heights),max=max(heights)) if heights else None,
        pelvis_move_m=stats(moved),foot_rotation_offset_degrees=stats(tilt),
        frames_not_root_grounded=sum(not f['post']['rootGrounded'] for f in frames),frames_not_grounded=sum(not f['post']['grounded'] for f in frames),
        # The C owner takes the pelvis write as a pure translation of the foot.
        foot_translation_adapter_m=stats(adapter),
        uneven_ground_observed=largest>=UNEVEN,
        verdict='The capture left flat ground.' if largest>=UNEVEN else 'Flat ground only: this capture cannot qualify the grounder on uneven ground.')

def analyze(path,settings=None):
    source=settings or json.loads(SETTINGS.read_text())['settings'];header=record=end=None;frames=[]
    with Path(path).open() as stream:
        for line in stream:
            item=json.loads(line);kind=item.get('type')
            if kind=='header':header=item
            elif kind=='rig':
                require(record is None,'Capture reattached to a second grounder');record=item
            elif kind=='frame':frames.append(item)
            elif kind=='end':end=item
    require(header and header.get('kind')=='observed-original-grounder' and record and frames,'Not a complete grounder capture')
    def numbers(value):
        if isinstance(value,dict):return all(numbers(v) for v in value.values())
        if isinstance(value,list):return all(numbers(v) for v in value)
        return value is not None
    require(all(numbers(f) for f in frames),'Capture holds a nonfinite value')
    differences=settings_differences(source,record)
    require(not record['rotateSolver'] and record['quality']==source['solver']['quality'],'pe_grounder covers only world-up grounding at the serialized quality')
    mask=record['layers'];names=record['layerNames'];live,fitted=live_settings(frames,record,source)
    report=dict(schema='pe-grounder-comparison-1',capture=str(path),frames=len(frames),end=end and end.get('reason'),setting_differences=differences,
        layers={str(bit):names[bit] for bit in range(32) if mask>>bit&1},
        pre_grounder_subscribers=record['preGrounderSubscribers'],post_grounder_subscribers=record['postGrounderSubscribers'],pre_update_subscribers=record['preUpdateSubscribers'],
        coverage=coverage(frames,record),probe_rays_against_original=probe_rays(frames,record['maxStep']),
        # Each frame restarted from the original's own state: one update's arithmetic, no accumulated drift.
        live_settings=fitted,
        single_update=compare(frames,replay(frames,True,live)),
        # Started once from the first frame and left to run on the recorded inputs and hits.
        free_running=compare(frames,replay(frames,False,live)),
        scope='Observed original grounder state, rays and requests on the live player against pe_grounder. The rays are the probe\'s repeats of the original\'s, '
              'checked against the root and heel hits the original keeps. Collision in the rebuild is not involved.')
    return report

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture',type=Path);p.add_argument('--output',type=Path)
    a=p.parse_args(argv)
    try:
        report=analyze(a.capture);text=json.dumps(report,indent=2)+'\n'
        if a.output:
            with a.output.open('x') as stream:stream.write(text)
        print(text,end='')
    except (OSError,ValueError,KeyError,TypeError,IndexError,StopIteration,subprocess.CalledProcessError,json.JSONDecodeError) as error:
        print('Grounder comparison: %s'%error,file=sys.stderr);return 1
    return 0
if __name__=='__main__':raise SystemExit(main())
