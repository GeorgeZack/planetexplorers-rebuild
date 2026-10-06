#!/usr/bin/env python3
"""Validate bounded sequential clone output; never infer gameplay parity."""
import argparse
import hashlib
import json
import math
from pathlib import Path
from reference_root_motion_bake import LENGTH, unique_pairs


def vector(value, count):
    if not isinstance(value, list) or len(value) != count or any(type(x) not in (int,float) or not math.isfinite(x) for x in value):
        raise ValueError('Malformed finite vector')
    if count == 4 and abs(sum(x*x for x in value)-1) > .002:
        raise ValueError('Nonunit rotation')
    return value


def distance(a,b):
    return math.sqrt(sum((x-y)**2 for x,y in zip(a,b)))


def multiply(a,b):
    x,y,z,w=a;X,Y,Z,W=b
    return [w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,w*Z+x*Y-y*X+z*W,w*W-x*X-y*Y-z*Z]


def rotation_error(a,b):
    return min(distance(a,b),distance(a,[-x for x in b]))


def analyze(path, require_motion=False):
    if path.stat().st_size > 4*1024*1024:
        raise ValueError('Capture size bound')
    raw=path.read_bytes();lines=raw.splitlines()
    if len(lines)>3610 or any(len(line)>16384 for line in lines):
        raise ValueError('Record count/line bound')
    records=[json.loads(line,object_pairs_hook=unique_pairs) for line in lines]
    if any(not isinstance(r,dict) for r in records):
        raise ValueError('Record must be object')
    if records and records[0].get('schema')=='pe-root-motion-bake-2':
        return analyze_frames(raw,records,require_motion)
    if not records or records[0].get('schema')!='pe-root-motion-bake-1' or records[-1].get('type')!='end' or records[-1].get('reason')!='complete':
        raise ValueError('Incomplete capture/schema')
    h=records[0]
    if h.get('rates')!=[30,60] or h.get('clip')!='RunSwordAttack' or h.get('sourcePathId')!=408 or h.get('skillId')!=20110014 or h.get('applyRootMotion') is not True or h.get('fireEvents') is not False or h.get('controllerAttached') is not False:
        raise ValueError('Source/isolation identity differs')
    source=records[1]
    if source.get('type')!='source' or source.get('avatar')!='FemaleAvatar' or source.get('controller')!='FemaleAnimator2' or abs(source.get('length',0)-LENGTH)>1e-7 or type(source.get('nodes')) is not int or not 2<=source['nodes']<=512:
        raise ValueError('Source skeleton/timing differs')
    cursor=2; summaries=[];clone_ids=[];total=0
    for trial,fps in enumerate((30,60)):
        count=math.ceil(LENGTH*fps)+1;rows=records[cursor:cursor+count];cursor+=count
        if len(rows)!=count or cursor>=len(records) or records[cursor]!={'type':'trialEnd','trial':trial,'samples':count}:
            raise ValueError('Trial ordering/count differs')
        cursor+=1;sum_pos=[0.,0.,0.];sum_rot=[0.,0.,0.,1.];previous=0;max_position=max_rotation=0.
        for i,r in enumerate(rows):
            if r.get('type')!='sample' or r.get('trial')!=trial or r.get('fps')!=fps or r.get('index')!=i:
                raise ValueError('Sample order/identity differs')
            for k in ('time','playableTime','dt'):
                if type(r.get(k)) not in (float,int) or not math.isfinite(r[k]):
                    raise ValueError('Nonfinite clock')
            target=min(i/fps,LENGTH)
            if abs(r['time']-target)>1e-6 or abs(r['playableTime']-r['time'])>.0001 or abs(r['dt']-(r['time']-previous))>1e-6 or (i>0 and r['dt']<=0):
                raise ValueError('Sequential clock mismatch')
            if type(r.get('cloneId')) is not int or r['cloneId']==source.get('animatorId') or r['cloneId']!=rows[0]['cloneId'] or type(r.get('frame')) is not int or r['frame']!=rows[0]['frame']:
                raise ValueError('Clone/frame changed during synchronous trial')
            previous=r['time']
            for k in ('position','deltaPosition','cumulativeDeltaPosition','animatorRootPosition'):vector(r.get(k),3)
            for k in ('rotation','deltaRotation','cumulativeDeltaRotation','animatorRootRotation'):vector(r.get(k),4)
            if i:
                sum_pos=[a+b for a,b in zip(sum_pos,r['deltaPosition'])]
                sum_rot=multiply(sum_rot,r['deltaRotation'])
            if distance(sum_pos,r['cumulativeDeltaPosition'])>1e-4 or rotation_error(sum_rot,r['cumulativeDeltaRotation'])>1e-4:
                raise ValueError('Cumulative delta record mismatch')
            displacement=[a-b for a,b in zip(r['position'],rows[0]['position'])]
            expected_rotation=multiply(rows[0]['rotation'],sum_rot)
            max_position=max(max_position,distance(displacement,sum_pos))
            max_rotation=max(max_rotation,rotation_error(r['rotation'],expected_rotation))
        last=rows[-1]; displacement=[a-b for a,b in zip(last['position'],rows[0]['position'])]
        inverse=[-x for x in rows[0]['rotation'][:3]]+[rows[0]['rotation'][3]]
        summaries.append({'trial':trial,'fps':fps,'samples':count,'clone_id':last['cloneId'],'duration':last['time'],
                          'endpoint_displacement':displacement,'endpoint_relative_rotation':multiply(inverse,last['rotation']),
                          'cumulative_delta_position':sum_pos,'max_applied_vs_delta_position_error':max_position,
                          'max_applied_vs_delta_quaternion_error':max_rotation})
        clone_ids.append(last['cloneId']);total+=count
    if cursor!=len(records)-1 or records[-1]['samples']!=total or len(set(clone_ids))!=2:
        raise ValueError('End count/clone isolation differs')
    endpoint_error=distance(summaries[0]['endpoint_displacement'],summaries[1]['endpoint_displacement'])
    rotation_difference=rotation_error(summaries[0]['endpoint_relative_rotation'],summaries[1]['endpoint_relative_rotation'])
    consistent=all(s['max_applied_vs_delta_position_error']<.001 and s['max_applied_vs_delta_quaternion_error']<.001 for s in summaries)
    moving=all(distance(s['endpoint_displacement'],[0,0,0])>1e-5 for s in summaries)
    qualified=consistent and moving and endpoint_error<.005 and rotation_difference<.001
    if require_motion and not qualified:
        raise ValueError('Capture did not establish consistent nonzero sequential clone motion')
    return {'schema':1,'kind':'isolated-running-knife-root-motion','capture_sha256':hashlib.sha256(raw).hexdigest(),
            'source_clip':408,'source_skill':20110014,'samples':total,'trials':summaries,
            'cross_rate_endpoint_position_error':endpoint_error,'cross_rate_endpoint_quaternion_error':rotation_difference,
            'clone_motion_qualified':qualified,'validation_tolerances':{'position_consistency_m':.001,'cross_rate_endpoint_m':.005,'quaternion_error':.001},
            'process_teardown_validated':False,'gameplay_motion_validated':False,
            'limits':'Clips evaluated from time0 at unit speed without transitions/IK/game motor. No live attack, collision, velocity correction, world trajectory or process-lifetime success inferred.'}


def scalar(x):
    if type(x) not in (int,float) or not math.isfinite(x):raise ValueError('Nonfinite scalar')
    return x


def interpolate(rows,time,key):
    for a,b in zip(rows,rows[1:]):
        if a['playableTime']<=time<=b['playableTime']:
            alpha=(time-a['playableTime'])/(b['playableTime']-a['playableTime'])
            left=a[key];right=b[key]
            if len(left)==4 and sum(x*y for x,y in zip(left,right))<0:right=[-x for x in right]
            result=[x+(y-x)*alpha for x,y in zip(left,right)]
            if len(result)==4:
                n=math.sqrt(sum(x*x for x in result));result=[x/n for x in result]
            return result
    raise ValueError('Comparison outside measured authored range')


def analyze_frames(raw,records,require_motion):
    h=records[0]
    if h.get('type')!='header' or h.get('playbackSpeeds') not in ([1,.5],[.1,.05],[.05,.025],[.025,.0125]) or h.get('clip')!='RunSwordAttack' or h.get('sourcePathId')!=408 or h.get('skillId')!=20110014 or h.get('applyRootMotion') is not True or h.get('fireEvents') is not False or h.get('controllerAttached') is not False:
        raise ValueError('Frame-driven source/isolation identity differs')
    profile,trial_limit={(1,.5):('standard',30),(.1,.05):('dense',60),
        (.05,.025):('denser',120),(.025,.0125):('ultra-dense',180)}[tuple(h['playbackSpeeds'])]
    # Earlier captures predate explicit profile metadata; their exact supported
    # speed pair remains authoritative. The new profile must declare its bounds.
    if (profile=='ultra-dense' or 'samplingProfile' in h or 'trialWallLimitSeconds' in h) and (
        h.get('samplingProfile')!=profile or type(h.get('trialWallLimitSeconds')) not in (int,float) or h['trialWallLimitSeconds']!=trial_limit):
        raise ValueError('Sampling profile/wall-time policy differs')
    if records[-1].get('type')!='end' or records[-1].get('reason')!='complete':raise ValueError('Incomplete frame-driven capture')
    source=records[1]
    if source.get('type')!='source' or source.get('avatar')!='FemaleAvatar' or source.get('controller')!='FemaleAnimator2' or abs(scalar(source.get('length'))-LENGTH)>1e-7 or type(source.get('nodes')) is not int or not 2<=source['nodes']<=512:
        raise ValueError('Frame-driven source differs')
    cursor=2;groups=[];summaries=[];ids=[];total=0
    for trial,speed in enumerate(h['playbackSpeeds']):
        rows=[]
        while cursor<len(records) and records[cursor].get('type')=='sample':
            rows.append(records[cursor]);cursor+=1
        if not 2<=len(rows)<=3600 or cursor>=len(records):raise ValueError('Frame-driven sample count')
        end=records[cursor];cursor+=1
        if end.get('type')!='trialEnd' or end.get('trial')!=trial or end.get('samples')!=len(rows):raise ValueError('Trial end mismatch')
        cumulative=[0,0,0];rotation=[0,0,0,1];max_position=max_rotation=max_clock=0.;previous_time=0;previous_frame=-1
        for i,r in enumerate(rows):
            if r.get('trial')!=trial or r.get('index')!=i or abs(scalar(r.get('playbackSpeed'))-speed)>1e-6 or r.get('directorMode')!='GameTime':raise ValueError('Trial sample identity/mode')
            if type(r.get('cloneId')) is not int or r['cloneId']!=rows[0]['cloneId'] or r['cloneId']==source.get('animatorId'):raise ValueError('Clone identity mismatch')
            if type(r.get('frame')) is not int or r['frame']<=previous_frame:raise ValueError('Nonadvancing engine frames')
            previous_frame=r['frame']
            for k in ('time','playableTime','dt','directorTime'):scalar(r.get(k))
            for k in ('position','deltaPosition','cumulativeDeltaPosition','animatorRootPosition'):vector(r.get(k),3)
            for k in ('rotation','deltaRotation','cumulativeDeltaRotation','animatorRootRotation'):vector(r.get(k),4)
            clock_error=abs((r['playableTime']-previous_time)-r['dt']*speed)
            if abs(r['playableTime']-r['time'])>.0001 or clock_error>.0001 or (i==0 and (abs(r['playableTime'])>1e-6 or r['dt']!=0)) or (i>0 and (not 0<r['dt']<=1 or r['playableTime']<=previous_time)):
                raise ValueError('Engine/playable clock mismatch')
            if i:
                cumulative=[a+b for a,b in zip(cumulative,r['deltaPosition'])];rotation=multiply(rotation,r['deltaRotation'])
            if distance(cumulative,r['cumulativeDeltaPosition'])>1e-4 or rotation_error(rotation,r['cumulativeDeltaRotation'])>1e-4:raise ValueError('Cumulative delta mismatch')
            displacement=[a-b for a,b in zip(r['position'],rows[0]['position'])]
            max_position=max(max_position,distance(displacement,cumulative));max_rotation=max(max_rotation,rotation_error(r['rotation'],multiply(rows[0]['rotation'],rotation)));max_clock=max(max_clock,clock_error)
            previous_time=r['playableTime']
        if rows[-2]['playableTime']>=LENGTH or rows[-1]['playableTime']<LENGTH or abs(scalar(end.get('terminalTime'))-rows[-1]['playableTime'])>1e-7:raise ValueError('Not first measured crossing of source end')
        groups.append(rows);ids.append(rows[0]['cloneId']);total+=len(rows)
        summaries.append(dict(trial=trial,playback_speed=speed,samples=len(rows),clone_id=ids[-1],
            observed_terminal_time=rows[-1]['playableTime'],terminal_overshoot=rows[-1]['playableTime']-LENGTH,
            maximum_authored_sample_gap=max(b['playableTime']-a['playableTime'] for a,b in zip(rows,rows[1:])),
            maximum_engine_frame_dt=max(r['dt'] for r in rows),
            max_clock_error=max_clock,max_applied_vs_delta_position_error=max_position,max_applied_vs_delta_quaternion_error=max_rotation))
    if cursor!=len(records)-1 or total>3600 or records[-1].get('samples')!=total or len(set(ids))!=2:raise ValueError('End count/clone mismatch')
    # Include every measured knot: the norm of a piecewise-linear position
    # difference reaches its maximum at a segment endpoint, not necessarily
    # at a 60Hz comparison tick. Keep that grid for quaternion diagnostics too.
    times=sorted({*[i/60 for i in range(math.ceil(LENGTH*60))],LENGTH,
                  *(r['playableTime'] for rows in groups for r in rows if 0<=r['playableTime']<=LENGTH)})
    max_position=max_rotation=0.;endpoints=[]
    for t in times:
        positions=[];rotations=[]
        for rows in groups:
            positions.append([a-b for a,b in zip(interpolate(rows,t,'position'),rows[0]['position'])])
            inverse=[-x for x in rows[0]['rotation'][:3]]+[rows[0]['rotation'][3]]
            rotations.append(multiply(inverse,interpolate(rows,t,'rotation')))
        max_position=max(max_position,distance(*positions));max_rotation=max(max_rotation,rotation_error(*rotations))
        if t==LENGTH:endpoints=positions
    qualified=all(s['max_applied_vs_delta_position_error']<.001 and s['max_applied_vs_delta_quaternion_error']<.001 for s in summaries) and max_position<.005 and max_rotation<.001 and all(distance(p,[0,0,0])>1e-5 for p in endpoints)
    if require_motion and not qualified:raise ValueError('Frame-driven clone motion failed consistency/common-time comparison')
    return dict(schema=2,kind='isolated-running-knife-frame-root-motion',capture_sha256=hashlib.sha256(raw).hexdigest(),source_clip=408,source_skill=20110014,
        samples=total,trials=summaries,sampling_profile=profile,trial_wall_limit_seconds=trial_limit,clone_motion_qualified=qualified,common_authored_end=LENGTH,
        all_authored_gaps_within_60hz=all(s['maximum_authored_sample_gap']<=1/60 for s in summaries),
        comparison_policy='linear position and normalized shortest quaternion interpolation only within measured brackets; no raw samples changed',
        comparison_samples=len(times),comparison_includes_all_measured_knots=True,maximum_cross_speed_position_error=max_position,maximum_cross_speed_quaternion_error=max_rotation,
        interpolated_source_end_displacements=endpoints,process_teardown_validated=False,gameplay_motion_validated=False,
        limits='Variable engine-frame samples, explicit terminal overshoot, clone-local configured speeds. No exact terminal sample or fixed-rate capture claimed; transitions, source motor/collision and process lifetime remain unverified.')


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture',type=Path);p.add_argument('--report',type=Path);p.add_argument('--require-motion',action='store_true');a=p.parse_args()
    try: result=analyze(a.capture,a.require_motion)
    except (OSError,ValueError,KeyError,TypeError) as e:p.error(str(e))
    text=json.dumps(result,indent=2)+'\n'
    if a.report:a.report.write_text(text)
    else:print(text,end='')
if __name__=='__main__':main()
