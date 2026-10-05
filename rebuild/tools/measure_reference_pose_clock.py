#!/usr/bin/env python3
"""Measure captured Animator clip clocks; optionally align authored event estimates.

Observation-only. Does not infer collider activation, hit eligibility or damage.
Requires a complete capture and the matching source player rig.
"""
import argparse
import json
import math
from pathlib import Path
import statistics
import tempfile

from analyze_reference_pose import analyze, decode, sha256


def dominant(layer):
    clips=[clip for clip in layer['currentClips'] if clip['weight']>=.99]
    return clips[0] if len(clips)==1 else None


def compact_frames(capture,animator_id,first,last,layer_name):
    rows=[]
    with capture.open() as stream:
        for line in stream:
            frame=decode(line)
            if frame['type']!='frame' or frame['sample']<first or (last is not None and frame['sample']>last):continue
            animator=next(a for a in frame['animators'] if a['id']==animator_id)
            layers=[layer for layer in animator['layers'] if layer['name']==layer_name]
            if len(layers)!=1:raise ValueError('Expected one requested animation layer')
            rows.append({key:frame[key] for key in ('sample','frame','time','realtime')} |
                        {'animator_speed':animator['speed'],'first_person':animator['parameters'].get('FirstPerson'),
                         'anim_speed_parameter':animator['parameters'].get('AnimSpeed'),
                         'layer':layers[0],'model_world':frame['modelRoot']['world']})
    return rows


def realtime_at(rows,scaled):
    for left,right in zip(rows,rows[1:]):
        if left['time']<=scaled<=right['time'] and right['time']>left['time']:
            factor=(scaled-left['time'])/(right['time']-left['time'])
            real=left['realtime']+factor*(right['realtime']-left['realtime'])
            return {'recording_seconds':real-rows[0]['realtime'],'bracket_samples':[left['sample'],right['sample']]}
    return None


def measure(rows,source_clip=None):
    groups={};switches=[];previous_fp=object()
    for row in rows:
        if row['first_person']!=previous_fp:
            switches.append({'sample':row['sample'],'recording_seconds':row['realtime']-rows[0]['realtime'],
                             'FirstPerson':row['first_person']});previous_fp=row['first_person']
    for left,right in zip(rows,rows[1:]):
        a,b=left['layer'],right['layer'];ca,cb=dominant(a),dominant(b)
        if (a['inTransition'] or b['inTransition'] or not ca or not cb or ca['id']!=cb['id'] or
            a['current']['fullPathHash']!=b['current']['fullPathHash'] or left['animator_speed']!=right['animator_speed'] or
            left['first_person']!=right['first_person']):continue
        normalized=b['current']['normalizedTime']-a['current']['normalizedTime'];dt=right['time']-left['time']
        if normalized<0 or dt<=0:continue
        key=(cb['name'],right['animator_speed'],right['first_person'],b['current']['fullPathHash'])
        group=groups.setdefault(key,[])
        group.append({'clip_seconds':normalized*cb['length'],'scaled_seconds':dt,
                      'ratio':normalized*cb['length']/dt,'reported_state_length':b['current']['length'],
                      'samples':[left['sample'],right['sample']]})
    clocks=[]
    for key,pairs in groups.items():
        ratio=sum(p['clip_seconds'] for p in pairs)/sum(p['scaled_seconds'] for p in pairs)
        clocks.append({'clip':key[0],'animator_speed':key[1],'FirstPerson':key[2],'full_path_hash':key[3],
                       'pair_count':len(pairs),'clip_seconds_per_scaled_second':ratio,
                       'ratio_divided_by_animator_speed':ratio/key[1] if key[1] else None,
                       'individual_ratio_min':min(p['ratio'] for p in pairs),
                       'individual_ratio_median':statistics.median(p['ratio'] for p in pairs),
                       'individual_ratio_max':max(p['ratio'] for p in pairs),
                       'reported_state_length_range':[min(p['reported_state_length'] for p in pairs),max(p['reported_state_length'] for p in pairs)]})
    points=[[r['model_world'][k] for k in (3,7,11)] for r in rows]
    result={'schema':1,'first_sample':rows[0]['sample'],'last_sample':rows[-1]['sample'],
            'recording_realtime_start':rows[0]['realtime'],'recording_scaled_time_start':rows[0]['time'],
            'FirstPerson_changes':switches,'clip_clocks':clocks,
            'model_root_start':points[0],'model_root_end':points[-1],
            'model_root_travel_m':sum(math.dist(a,b) for a,b in zip(points,points[1:])),
            'method':'Stable adjacent same-state/nontransition pairs with one clip weight>=.99 and unchanged Animator speed/FirstPerson. Ratio = delta normalizedTime * runtime clip.length / delta Time.time.',
            'activation_observed':False,'event_alignment':None}
    if source_clip:
        points=[]
        for row in rows:
            layer=row['layer'];clip=dominant(layer)
            if not layer['inTransition'] and clip and clip['name']==source_clip['m_Name']:
                points.append((row['time'],layer['current']['normalizedTime']*clip['length']))
        if len(points)<3 or any(b[1]<a[1] for a,b in zip(points,points[1:])):
            raise ValueError('Select one coherent occurrence with at least three stable source clip samples')
        tx=statistics.mean(x for x,y in points);ty=statistics.mean(y for x,y in points)
        denominator=sum((x-tx)**2 for x,y in points)
        if denominator<=0:raise ValueError('Invalid event-fit time interval')
        slope=sum((x-tx)*(y-ty) for x,y in points)/denominator
        if slope<=0:raise ValueError('Nonpositive clip clock')
        entry=tx-ty/slope
        residual=max(abs(y-slope*(x-entry)) for x,y in points)
        events=[]
        for event in source_clip['m_Events']:
            scaled=entry+event['time']/slope
            events.append({'source_seconds':event['time'],'function':event['functionName'],'data':event['data'],
                           'estimated_scaled_time':scaled,'recording_alignment':realtime_at(rows,scaled)})
        result['event_alignment']={'source_clip':source_clip['m_Name'],'stable_samples':len(points),
            'fitted_clip_seconds_per_scaled_second':slope,'estimated_normalized_zero_scaled_time':entry,
            'estimated_entry_recording_alignment':realtime_at(rows,entry),'fit_max_clip_seconds_residual':residual,
            'events':events,'limitation':'Estimated authored-event alignment only. No event callbacks or attack/collider active flags were captured. End-of-frame pose can include blending/IK.'}
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture',type=Path);parser.add_argument('--rig',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True);parser.add_argument('--layer',default='FullBody')
    parser.add_argument('--first-sample',type=int,default=0);parser.add_argument('--last-sample',type=int)
    parser.add_argument('--source-clip',type=Path)
    args=parser.parse_args()
    try:
        capture=args.capture.expanduser().resolve();rig_path=args.rig.expanduser().resolve();output=args.output.expanduser().resolve()
        inputs=[capture,rig_path,Path(__file__).resolve()]
        if args.source_clip:inputs.append(args.source_clip.resolve())
        if output in inputs:raise ValueError('Output must not overwrite an input')
        if rig_path.stat().st_size>64*1024*1024:raise ValueError('Oversized rig')
        rig=decode(rig_path.read_text())
        report,_=analyze(capture,rig,first_sample=args.first_sample,last_sample=args.last_sample)
        rows=compact_frames(capture,report['rig_mapping']['animator_id'],args.first_sample,args.last_sample,args.layer)
        source=None
        if args.source_clip:
            if args.source_clip.stat().st_size>16*1024*1024:raise ValueError('Oversized source clip JSON')
            source=decode(args.source_clip.read_text())
        result=measure(rows,source);result['sources']=[{'path':str(p),'sha256':sha256(p)} for p in inputs]
        output.parent.mkdir(parents=True,exist_ok=True)
        with tempfile.NamedTemporaryFile('w',dir=output.parent,delete=False,prefix='.pose-clock-') as stream:
            temporary=Path(stream.name);json.dump(result,stream,indent=2);stream.write('\n')
        temporary.replace(output)
        print(f'Measured {len(result["clip_clocks"])} clip/speed combinations over {len(rows)} frames')
    except (OSError,ValueError,KeyError,TypeError,IndexError,ZeroDivisionError) as error:
        parser.exit(1,f'Pose clock analysis failed: {error}\n')
    return 0

if __name__=='__main__':raise SystemExit(main())
