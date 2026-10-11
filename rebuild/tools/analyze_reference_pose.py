#!/usr/bin/env python3
"""Validate observational Unity pose JSONL and optionally export recorded player parts.

Python stdlib only. No game launch, injection, input, Unity evaluation or graphics.
PEAC output is a recording of selected captured poses, not humanoid retargeting.
One file per source part/material preserves the current single-material format.
"""
from __future__ import annotations
from read_reference_adventure import source_label
import argparse
import bisect
import hashlib
import json
import math
from pathlib import Path
import shutil
import struct
import tempfile
import zlib

from export_reference_assets import IDENTITY, multiply, trs, property_map
from export_reference_actor import inverse_affine, matrix_values
from export_reference_player import validate_skin

MAX_BYTES = 256*1024*1024
MAX_LINE = 4*1024*1024
MAX_FRAMES = 3600
TOLERANCE = .001


def fail(message):
    raise ValueError(message)


def finite_number(value, name):
    if isinstance(value, bool) or not isinstance(value, (int,float)) or not math.isfinite(value):
        fail('Invalid finite number: '+name)
    return value


def integer(value, name, minimum=None):
    if isinstance(value,bool) or not isinstance(value,int) or (minimum is not None and value<minimum):
        fail('Invalid integer: '+name)
    return value


def boolean(value, name):
    if not isinstance(value,bool): fail('Invalid boolean: '+name)
    return value


def vector(value, size, name):
    if not isinstance(value,list) or len(value)!=size:
        fail('Invalid vector: '+name)
    return [finite_number(x,name) for x in value]


def matrix(value, name):
    values=vector(value,16,name)
    if max(abs(a-b) for a,b in zip(values[12:],[0,0,0,1]))>1e-5:
        fail('Non-affine matrix: '+name)
    return [values[i:i+4] for i in range(0,16,4)]


def flat(value):
    return [x for row in value for x in row]


def matrix_error(a,b):
    return max(abs(x-y) for x,y in zip(flat(a),flat(b)))


def close_matrix(a,b,name):
    error=matrix_error(a,b)
    # Unity's source float matrix products accumulate error at world offsets.
    tolerance=TOLERANCE+2e-5*max(abs(x) for x in flat(b))
    if error>tolerance: fail(f'{name} matrix mismatch ({error:g} > {tolerance:g})')
    return error


def decode(text):
    def pairs(entries):
        result={}
        for key,value in entries:
            if key in result: fail('Duplicate JSON key: '+key)
            result[key]=value
        return result
    return json.loads(text,object_pairs_hook=pairs,parse_constant=lambda value:fail('Nonfinite JSON constant: '+value))


def transform(row):
    identity=integer(row['id'],'transform id')
    if identity==0: fail('Null transform id')
    integer(row['parentId'],'parent id')
    boolean(row['activeInHierarchy'],'transform active')
    if not isinstance(row['path'],str) or not row['path'] or len(row['path'])>8192:
        fail('Invalid transform path')
    local=matrix(row['local'],'local');world=matrix(row['world'],'world')
    position=vector(row['localPosition'],3,'position')
    rotation=vector(row['localRotation'],4,'rotation')
    scale=vector(row['localScale'],3,'scale')
    if abs(sum(x*x for x in rotation)-1)>.002: fail('Non-unit transform quaternion')
    rebuilt=trs(dict(zip('xyz',position)),dict(zip('xyzw',rotation)),dict(zip('xyz',scale)))
    return identity,local,world,close_matrix(local,rebuilt,'local TRS')


def validate_frame(row):
    integer(row['frame'],'frame',0);integer(row['sample'],'sample',0)
    for field in ('time','realtime','delta','unscaledDelta','fixedTime','fixedDelta'):
        if finite_number(row[field],field)<0: fail('Negative frame clock')
    if not isinstance(row['transforms'],list) or len(row['transforms'])>1024:
        fail('Too many model transforms')
    records={};local={};world={};maximum=0.
    for item in row['transforms']:
        key,loc,glob,error=transform(item)
        if key in records: fail('Duplicate transform id')
        records[key]=item;local[key]=loc;world[key]=glob;maximum=max(maximum,error)
    for item in (row['playerRoot'],row['modelRoot']):
        key,loc,glob,error=transform(item);maximum=max(maximum,error)
        if key in records:
            if item['path']!=records[key]['path'] or item['parentId']!=records[key]['parentId']:
                fail('Conflicting root transform identity')
            maximum=max(maximum,close_matrix(glob,world[key],'root duplicate world'))
            maximum=max(maximum,close_matrix(loc,local[key],'root duplicate local'))
        records[key]=item;local[key]=loc;world[key]=glob
    paths=[item['path'] for item in records.values()]
    if len(set(paths))!=len(paths): fail('Duplicate transform path')
    root=row['modelRoot'];external={root['id'],row['playerRoot']['id']}
    for key,item in records.items():
        parent=item['parentId']
        if key not in external and not item['path'].startswith(root['path']+'/'):
            fail('Transform outside model root')
        if parent==key: fail('Self-parent transform')
        if parent in records:
            expected=records[parent]['path']+'/'+item['path'].rsplit('/',1)[-1]
            if item['path']!=expected: fail('Transform path/parent disagree')
            maximum=max(maximum,close_matrix(world[key],multiply(world[parent],local[key]),'parent/world'))
        elif key not in external:
            fail('Missing model transform parent')
    if not isinstance(row['animators'],list) or len(row['animators'])>16: fail('Too many animators')
    animator_ids=set()
    for animator in row['animators']:
        identity=integer(animator['id'],'animator id')
        if identity==0 or identity in animator_ids: fail('Invalid or duplicate animator id')
        animator_ids.add(identity)
        integer(animator['avatarId'],'Avatar id')
        for key in ('enabled','activeInHierarchy','applyRootMotion'): boolean(animator[key],key)
        if animator['path'] not in paths: fail('Animator transform path missing')
        finite_number(animator['speed'],'Animator speed')
        root_world=matrix(animator['rootWorld'],'Animator root')
        root_inverse=matrix(animator['rootInverseWorld'],'Animator root inverse')
        maximum=max(maximum,close_matrix(multiply(root_world,root_inverse),IDENTITY,'Animator inverse'))
        found=next(key for key,item in records.items() if item['path']==animator['path'])
        maximum=max(maximum,close_matrix(root_world,world[found],'Animator root world'))
        if len(animator['layers'])>32 or len(animator['parameters'])>512: fail('Animator record bound exceeded')
        for value in animator['parameters'].values():
            if value is not None and not isinstance(value,bool): finite_number(value,'Animator parameter')
        for bone in animator['humanBones'].values():
            if integer(bone,'human bone id') not in records: fail('Unknown human bone id')
        for layer in animator['layers']:
            finite_number(layer['weight'],'layer weight')
            boolean(layer['inTransition'],'inTransition')
            for field in ('current','next'):
                state=layer[field]
                boolean(state['loop'],'state loop')
                for name in ('fullPathHash','shortNameHash'): integer(state[name],name)
                for name in ('normalizedTime','length','speed','speedMultiplier'): finite_number(state[name],name)
            for field in ('currentClips','nextClips'):
                if len(layer[field])>64: fail('Too many layer clips')
                for clip in layer[field]:
                    weight=finite_number(clip['weight'],'clip weight')
                    if weight<0 or weight>1.001: fail('Invalid clip weight')
                    if clip['length'] is not None and finite_number(clip['length'],'clip length')<0: fail('Negative clip duration')
    if len(row['renderers'])>64: fail('Too many skin renderers')
    for renderer in row['renderers']:
        boolean(renderer['enabled'],'renderer enabled')
        if renderer['path'] not in paths or len(renderer['boneIds'])>1024: fail('Invalid renderer record')
        for identity in renderer['boneIds']:
            integer(identity,'renderer bone id')
            if identity and identity not in records: fail('Unknown renderer bone id')
    return records,world,maximum


def rig_mapping(rig,frame,records):
    sex=rig['sex'];gender=frame['gender']
    if sex not in ('male','female') or not isinstance(gender,str) or gender.lower()!=sex:
        fail('Capture gender does not match source rig')
    avatar_name=rig['avatar']['m_Name']
    matches=[a for a in frame['animators'] if a['enabled'] and a['activeInHierarchy'] and a['avatar']==avatar_name]
    if len(matches)!=1: fail('Expected one active matching Avatar animator')
    animator=matches[0]
    avatar=rig['avatar']['m_Avatar'];paths=dict(rig['avatar']['m_TOS'])
    name_hashes=avatar['m_SkeletonNameIDArray']
    path_hashes=avatar['m_AvatarSkeleton']['data']['m_ID']
    by_name={n:paths[p] for n,p in zip(name_hashes,path_hashes)}
    mapping={};missing=[]
    for part in rig['parts']:
        for name in part['bone_names']:
            if name in mapping or name in missing: continue
            expected=by_name.get(zlib.crc32(name.encode()))
            found=[identity for identity,item in records.items()
                   if item['path'].startswith(animator['path']+'/') and
                   (item['path']==animator['path']+'/'+expected if expected is not None
                    else item['path'].rsplit('/',1)[-1]==name)]
            if len(found)>1: fail('Ambiguous captured bone: '+name)
            if not found: missing.append(name)
            else: mapping[name]=found[0]
    skin_ids={identity for renderer in frame['renderers'] if renderer['enabled'] and
              (renderer['path']==animator['path'] or renderer['path'].startswith(animator['path']+'/'))
              for identity in renderer['boneIds'] if identity}
    return {'animator_id':animator['id'],'animator_path':animator['path'],'avatar':avatar_name,
            'avatar_instance_id':animator['avatarId'],'model_root_id':frame['modelRoot']['id'],
            'bones':{name:{'id':identity,'path':records[identity]['path'],
                            'relative_path':records[identity]['path'][len(animator['path'])+1:],
                            'referenced_by_enabled_skin':identity in skin_ids} for name,identity in mapping.items()},
            'missing_bones':missing}




def combined_skin_comparison(rig, frame, mapping):
    """Corroborate known eight-part AvatarData slot order; not a mesh identity proof."""
    if len(rig['parts']) != 8 or mapping['missing_bones']:
        return None
    expected=[]
    for index in (5,6,7,4,0,2,1,3):  # HairF/HairT/HairB/Head/Torso/Hands/Legs/Feet
        part=rig['parts'][index]
        for _ in part['triangles']:
            expected.extend(mapping['bones'][name]['id'] for name in part['bone_names'])
    rows=[]
    for renderer in frame['renderers']:
        if renderer['enabled'] and renderer['path']==mapping['animator_path']:
            rows.append({'path':renderer['path'],'mesh_name':renderer['mesh'],
                         'captured_bone_entries':len(renderer['boneIds']),
                         'expected_bone_entries':len(expected),'exact_order_match':renderer['boneIds']==expected})
    return {'renderers':rows,'method':'AvatarData slot order; CustomModelTmpls duplicates each skin bone array for every submesh.',
            'limitation':'Matching bone order corroborates the source appearance selection, not runtime mesh/texture/blendshape identity.'}


def structural_mapping(mapping):
    # First-person visibility can disable a renderer without changing its bones.
    return {**{key:value for key,value in mapping.items() if key!='bones'},
            'bones':{name:{key:value for key,value in bone.items() if key!='referenced_by_enabled_skin'}
                     for name,bone in mapping['bones'].items()}}


def hypothesis_rows(path):
    if path is None: return {}
    if path.stat().st_size>64*1024*1024: fail('Hypothesis too large')
    value=decode(path.read_text())
    if value['schema']!=1 or value['coordinate_space']!='model-local' or len(value['frames'])>MAX_FRAMES:
        fail('Invalid hypothesis schema')
    result={}
    for row in value['frames']:
        sample=integer(row['sample'],'hypothesis sample',0);integer(row['frame'],'hypothesis frame',0)
        if sample in result or len(row['bone_matrices'])>1024: fail('Duplicate/oversized hypothesis frame')
        result[sample]={'frame':row['frame'],'bones':{name:matrix(m,'hypothesis bone') for name,m in row['bone_matrices'].items()}}
    return result


def add_spans(spans,frame,animator):
    for index,layer in enumerate(animator['layers']):
        current=layer['current'];next_state=layer['next']
        signature=(index,current['fullPathHash'],next_state['fullPathHash'],layer['inTransition'],
                   tuple((c['name'],c['id']) for c in layer['currentClips']),
                   tuple((c['name'],c['id']) for c in layer['nextClips']))
        previous=next((s for s in reversed(spans) if s['layer_index']==index),None)
        if previous is None or previous['_signature']!=signature:
            previous={'_signature':signature,'layer_index':index,'layer_name':layer['name'],
                      'first_sample':frame['sample'],'first_frame':frame['frame'],'last_sample':frame['sample'],
                      'current_state':current['fullPathHash'],'next_state':next_state['fullPathHash'],
                      'in_transition':layer['inTransition'],'normalized_time':[current['normalizedTime']]*2,
                      'next_normalized_time':[next_state['normalizedTime']]*2,'weights':{},
                      'animator_speed_range':[animator['speed']]*2,'parameters_first':animator['parameters']}
            spans.append(previous)
        previous['last_sample']=frame['sample'];previous['last_frame']=frame['frame']
        previous['normalized_time'][1]=current['normalizedTime'];previous['next_normalized_time'][1]=next_state['normalizedTime']
        previous['animator_speed_range']=[min(previous['animator_speed_range'][0],animator['speed']),max(previous['animator_speed_range'][1],animator['speed'])]
        for kind in ('currentClips','nextClips'):
            for clip in layer[kind]:
                key=kind+':'+str(clip['name'])+':'+str(clip['id'])
                old=previous['weights'].setdefault(key,[clip['weight'],clip['weight']])
                old[0]=min(old[0],clip['weight']);old[1]=max(old[1],clip['weight'])


def analyze(capture,rig,*,allow_incomplete=False,first_sample=0,last_sample=None,hypothesis=None,retain=False):
    if not capture.is_file() or capture.stat().st_size>MAX_BYTES: fail('Missing or oversized pose capture')
    if not 1<=len({name for part in rig['parts'] for name in part['bone_names']})<=256:
        fail('Source rig must reference 1..256 distinct bones')
    combined=None
    header=None;ended=False;count=0;last=None;mapping=None;spans=[];samples=[];selected=0;maximum=0.;traits=None
    comparisons=0;matrix_max=0.;translation_sum=0.;translation_max=0.;seen_hypotheses=set()
    hypotheses=hypothesis_rows(hypothesis)
    with capture.open('rb') as stream:
        while line:=stream.readline(MAX_LINE+1):
            if stream.tell()>MAX_BYTES: fail('Capture grew beyond byte limit')
            if len(line)>MAX_LINE: fail('Oversized capture line')
            if not line.endswith(b'\n'): fail('Truncated capture line')
            row=decode(line);kind=row['type']
            if ended: fail('Data after capture end')
            if header is None:
                if kind!='header' or integer(row['schema'],'header schema')!=1 or row['matrixOrder']!='row-major' or row['phase']!='WaitForEndOfFrame':
                    fail('Unsupported pose header')
                if not 1<=integer(row['maxFrames'],'maxFrames')<=MAX_FRAMES or not 1<=integer(row['maxBytes'],'maxBytes')<=MAX_BYTES:
                    fail('Invalid capture bounds')
                header=row;continue
            if kind=='humanTraits':
                if traits is not None or count: fail('Duplicate or late human traits')
                if len(row['muscles'])>256 or len(row['bones'])>256: fail('Oversized human traits')
                for muscle in row['muscles']:
                    finite_number(muscle['min'],'muscle min');finite_number(muscle['max'],'muscle max')
                for bone in row['bones']:
                    if len(bone['muscleIndices'])!=3 or any(not -1<=integer(i,'muscle index')<len(row['muscles']) for i in bone['muscleIndices']):
                        fail('Invalid human trait muscle index')
                traits=row;continue
            if kind=='end':
                if integer(row['frames'],'end frames',0)!=count: fail('End frame count mismatch')
                ended=True;continue
            if kind!='frame': fail('Unknown capture record')
            records,world,error=validate_frame(row);maximum=max(maximum,error)
            if row['sample']!=count or count>=header['maxFrames']: fail('Nonsequential sample or frame limit exceeded')
            if last and (row['frame']<=last['frame'] or row['realtime']<=last['realtime'] or row['time']<last['time'] or row['fixedTime']<last['fixedTime']):
                fail('Nonmonotonic capture frame/clock')
            count+=1;last={k:row[k] for k in ('frame','realtime','time','fixedTime')}
            if row['sample']<first_sample or (last_sample is not None and row['sample']>last_sample): continue
            current_mapping=rig_mapping(rig,row,records)
            if mapping is None:
                mapping=current_mapping
                combined=combined_skin_comparison(rig,row,mapping)
            elif structural_mapping(mapping)!=structural_mapping(current_mapping):
                fail('Avatar or bone hierarchy changed during selected capture')
            animator=next(a for a in row['animators'] if a['id']==mapping['animator_id'])
            add_spans(spans,row,animator);selected+=1
            root_inverse=inverse_affine(world[row['modelRoot']['id']])
            bone_worlds={name:multiply(root_inverse,world[bone['id']]) for name,bone in mapping['bones'].items()}
            if retain: samples.append({'sample':row['sample'],'frame':row['frame'],'realtime':row['realtime'],'bones':bone_worlds})
            if row['sample'] in hypotheses:
                reference=hypotheses[row['sample']]
                if reference['frame']!=row['frame']: fail('Hypothesis frame/sample mismatch')
                for path,hyp in reference['bones'].items():
                    names=[name for name,bone in mapping['bones'].items() if bone['relative_path']==path]
                    if len(names)!=1: fail('Unknown hypothesis bone path')
                    actual=bone_worlds[names[0]];matrix_max=max(matrix_max,matrix_error(actual,hyp))
                    distance=math.sqrt(sum((actual[r][3]-hyp[r][3])**2 for r in range(3)))
                    translation_sum+=distance*distance;translation_max=max(translation_max,distance);comparisons+=1
                seen_hypotheses.add(row['sample'])
    if header is None or not count or not selected: fail('Capture contains no selected frames')
    if not ended and not allow_incomplete: fail('Capture end record missing; use --allow-incomplete only for analysis')
    if set(hypotheses)!=seen_hypotheses: fail('Hypothesis samples missing from selected capture')
    for span in spans: del span['_signature']
    report={'schema':1,'header':header,'complete':ended,'frames':count,'selected_frames':selected,
            'maximum_trs_hierarchy_matrix_error':maximum,'rig_mapping':mapping,'animation_spans':spans,
            'human_traits':traits,'combined_skin_comparison':combined,'hypothesis_comparison':None,'general_retargeting_validated':False}
    if hypotheses:
        report['hypothesis_comparison']={'bone_comparisons':comparisons,'max_absolute_matrix_error':matrix_max,
            'translation_rms_m':math.sqrt(translation_sum/comparisons) if comparisons else None,
            'translation_max_m':translation_max,'coordinate_space':'model-local',
            'interpretation':'Measured against the supplied hypothesis only; captures may include blending, IK and body customization.'}
    return report,samples


def sample_matrices(samples,times,at,names):
    right=bisect.bisect_right(times,at)
    if right==0: left=right=0
    elif right>=len(times): left=right=len(times)-1
    else: left=right-1
    factor=0 if left==right else (at-times[left])/(times[right]-times[left])
    return [[[(1-factor)*samples[left]['bones'][name][r][c]+factor*samples[right]['bones'][name][r][c]
              for c in range(4)] for r in range(4)] for name in names]



def safe_basename(value, suffix):
    return (isinstance(value,str) and value.endswith(suffix) and 1<=len(value)<=200 and
            value not in ('.','..') and '..' not in value and
            all(c in 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-.' for c in value) and Path(value).name==value)


def solid_png(color):
    """Explicit fallback for material slots with no diffuse image; no image library."""
    rgba=bytes(round(max(0.,min(1.,finite_number(color.get(k,1.),'material color')))*255) for k in 'rgba')
    def chunk(kind,data):
        return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
    return (b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',1,1,8,6,0,0,0))+
            chunk(b'IDAT',zlib.compress(b'\0'+rgba))+chunk(b'IEND',b''))


def export_recording(output,rig,rig_path,samples,report,fps,parts):
    if not report['complete']: fail('Incomplete capture cannot produce a recording')
    if not samples or len(samples)<2: fail('Recording requires at least two captured poses')
    if not math.isfinite(fps) or not 1<=fps<=240: fail('Recording FPS must be 1..240')
    times=[sample['realtime']-samples[0]['realtime'] for sample in samples]
    # PEAC2 timestamps use the serialized floats, avoiding a duplicate terminal
    # timestamp when a nearly-grid double duration rounds down to f32.
    fps=struct.unpack('<f',struct.pack('<f',fps))[0]
    duration=struct.unpack('<f',struct.pack('<f',times[-1]))[0];frames=math.ceil(duration*fps)+1
    if not 0<duration<=120 or frames>20000: fail('Recording duration/frame bound exceeded')
    if not parts or len(parts)>64 or len(set(parts))!=len(parts): fail('Select 1..64 distinct parts')
    if any(not 0<=index<len(rig['parts']) for index in parts): fail('Invalid part index')
    if sum(len(rig['parts'][index]['materials']) for index in parts)>64: fail('Scene exceeds 64 material parts')
    material_map={(m['asset'],m['path_id']):m for m in rig['materials']}
    generated=[];total_bytes=0
    for index in parts:
        if not 0<=index<len(rig['parts']): fail('Invalid part index')
        part=rig['parts'][index];names=part['bone_names'];binds=part['inverse_binds']
        if not 1<=len(names)<=256 or len(names)!=len(binds): fail('Invalid skin joint count')
        if any(name not in report['rig_mapping']['bones'] for name in names): fail('Part has unmapped capture bones')
        for bind in binds: inverse_affine(matrix(flat(bind),'inverse bind'))
        validate_skin(part['skin'],len(names))
        vertices=part['positions'];normals=part['normals'];uvs=part['uv']
        if not 1<=len(vertices)<=1000000 or not len(vertices)==len(normals)==len(uvs)==len(part['skin']): fail('Invalid rig vertex count')
        estimated = len(vertices)*56 + frames*len(names)*48 + len(names)*48 + 72
        if total_bytes+estimated>MAX_BYTES: fail('Actor part would exceed byte limit')
        vertex_data=bytearray()
        for position,normal,uv,skin in zip(vertices,normals,uvs,part['skin']):
            p=vector(position,3,'position');n=vector(normal,3,'normal');v=vector(uv,2,'UV')
            if sum(x*x for x in n)<1e-12: fail('Zero rig normal')
            weights=[skin[f'weight[{i}]'] for i in range(4)];joints=[skin[f'boneIndex[{i}]'] for i in range(4)]
            vertex_data.extend(struct.pack('<12f4H',*p,*n,*v,*weights,*joints))
        palette_data=bytearray()
        for frame in range(frames):
            worlds=sample_matrices(samples,times,times[-1] if frame==frames-1 else min(frame/fps,times[-1]),names)
            for world,bind in zip(worlds,binds):
                palette=multiply(world,bind);inverse_affine(palette)
                palette_data.extend(struct.pack('<12f',*matrix_values(palette)))
        if len(part['triangles'])!=len(part['materials']): fail('Material/submesh count mismatch')
        for slot,(triangles,material_key) in enumerate(zip(part['triangles'],part['materials'])):
            indices=[i for tri in triangles for i in tri]
            if not indices: continue
            if len(indices)>3000000 or any(len(tri)!=3 for tri in triangles) or any(isinstance(i,bool) or not isinstance(i,int) or not 0<=i<len(vertices) for i in indices):
                fail('Invalid part triangle indices')
            filename=f'player-part-{index}-material-{slot}.peac'
            binary=bytearray(struct.pack('<4s5I',b'PEAC',2,len(vertices),len(indices),len(names),1))
            binary.extend(vertex_data);binary.extend(struct.pack('<'+'I'*len(indices),*indices))
            for bind in binds: binary.extend(struct.pack('<12f',*matrix_values(bind)))
            binary.extend(struct.pack('<32sfIfI',b'captured',fps,frames,duration,0));binary.extend(palette_data)
            total_bytes+=len(binary)
            if total_bytes>MAX_BYTES: fail('Combined actor export exceeds byte limit')
            (output/filename).write_bytes(binary)
            material=material_map[tuple(material_key)];texture=material.get('diffuse_file')
            if texture:
                if not safe_basename(texture,'.png'): fail('Unsafe rig texture path')
                source=rig_path.parent/texture
                if not source.is_file() or source.stat().st_size>64*1024*1024: fail('Missing or oversized rig texture')
                shutil.copyfile(source,output/texture)
                generated_texture=False
            else:
                texture=f'player-part-{index}-material-{slot}-solid.png'
                saved=material.get('properties',{}).get('m_Colors',[])
                color=property_map(saved).get('_Color',dict(r=1.,g=1.,b=1.,a=1.))
                (output/texture).write_bytes(solid_png(color))
                generated_texture=True
            generated.append({'file':filename,'part_index':index,'resource':part['resource'],'slot':slot,
                              'texture':texture,'texture_sha256':sha256(output/texture),'generated_solid_diffuse':generated_texture,
                              'material':material,'vertices':len(vertices),'triangles':len(indices)//3})
    if not generated: fail('Recording has no nonempty material parts')
    (output/'recording.peas').write_text('PEAS1\n'+''.join(p['file']+' '+p['texture']+'\n' for p in generated),encoding='ascii')
    manifest={'schema':1,'peac_version':2,'terminal_at_duration':True,'scene':'recording.peas','parts':generated,'clip':'captured','fps':fps,'frames':frames,'duration':duration,'loop':False,
              'first_capture_sample':samples[0]['sample'],'last_capture_sample':samples[-1]['sample'],
              'coordinate_space':'inverse(captured modelRoot world) * captured boneWorld * source inverseBind',
              'timebase':'recorded realtimeSinceStartup; includes pauses and speed changes',
              'interpolation':'componentwise affine matrix interpolation between recorded poses; guard frame clamps to final pose',
              'materials':'Separate PEAC per material; no flattened atlas or substitute shared texture',
              'limitations':['Recorded motion only; not generalized humanoid retargeting.',
                             'Default source mesh driven by captured bones; does not reproduce runtime clothing, morphs or shader state.',
                             'Bone samples include all animation blending and post-animation/IK effects at end of frame.']}
    (output/'recording-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return manifest


def sha256(path):
    with path.open('rb') as stream: return hashlib.file_digest(stream,'sha256').hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture',type=Path);parser.add_argument('--rig',type=Path,required=True)
    parser.add_argument('--report',type=Path,required=True)
    parser.add_argument('--hypothesis',type=Path)
    parser.add_argument('--allow-incomplete',action='store_true')
    parser.add_argument('--first-sample',type=int,default=0);parser.add_argument('--last-sample',type=int)
    parser.add_argument('--export-dir',type=Path);parser.add_argument('--fps',type=float,default=30.)
    parser.add_argument('--part-index',type=int,action='append')
    args=parser.parse_args()
    try:
        capture=args.capture.expanduser().resolve();rig_path=args.rig.expanduser().resolve();report_path=args.report.expanduser().resolve()
        export_dir=args.export_dir.expanduser().resolve() if args.export_dir else None
        if report_path in (capture,rig_path) or (args.hypothesis and report_path==args.hypothesis.resolve()): fail('Report must not overwrite inputs')
        if export_dir and any(export_dir==source or export_dir in source.parents for source in (capture,rig_path)):
            fail('Actor output must be separate from source directories')
        if args.first_sample<0 or (args.last_sample is not None and args.last_sample<args.first_sample): fail('Invalid sample interval')
        if not rig_path.is_file() or rig_path.stat().st_size>64*1024*1024: fail('Missing or oversized source rig')
        rig=decode(rig_path.read_text())
        if integer(rig['schema'],'rig schema')!=1 or not 1<=len(rig['parts'])<=64: fail('Unsupported source rig')
        report,samples=analyze(capture,rig,allow_incomplete=args.allow_incomplete,first_sample=args.first_sample,last_sample=args.last_sample,hypothesis=args.hypothesis,retain=export_dir is not None)
        report['sources']=[{'path':source_label(p),'sha256':sha256(p)} for p in [capture,rig_path,Path(__file__).resolve()]]
        if args.hypothesis: report['sources'].append({'path':source_label(args.hypothesis),'sha256':sha256(args.hypothesis)})
        if export_dir:
            export_dir.parent.mkdir(parents=True,exist_ok=True)
            with tempfile.TemporaryDirectory(prefix='.pose-recording-',dir=export_dir.parent) as temporary:
                staging=Path(temporary)
                recording=export_recording(staging,rig,rig_path,samples,report,args.fps,args.part_index or list(range(len(rig['parts']))))
                recording['sources']=report['sources']
                (staging/'recording-manifest.json').write_text(json.dumps(recording,indent=2)+'\n')
                export_dir.mkdir(parents=True,exist_ok=True)
                for file in sorted(staging.iterdir()): file.replace(export_dir/file.name)
                report['recording_manifest']=str(export_dir/'recording-manifest.json')
        report_path.parent.mkdir(parents=True,exist_ok=True)
        with tempfile.NamedTemporaryFile('w',dir=report_path.parent,delete=False,prefix='.pose-report-') as stream:
            temporary=Path(stream.name);json.dump(report,stream,indent=2);stream.write('\n')
        temporary.replace(report_path)
        print(f'Validated {report["selected_frames"]}/{report["frames"]} frames; complete={report["complete"]}; missing bones={len(report["rig_mapping"]["missing_bones"])}')
    except (OSError,ValueError,KeyError,TypeError,IndexError,struct.error,OverflowError) as error:
        parser.exit(1,f'Pose analysis failed: {error}\n')
    return 0

if __name__=='__main__': raise SystemExit(main())
