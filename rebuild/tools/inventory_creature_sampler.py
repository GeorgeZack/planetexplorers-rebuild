#!/usr/bin/env python3
"""Read-only wolf7/rhino4 sampler prerequisites. No curve decode, runtime export or launch."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import zlib

DESCRIPTORS={
 'wolf':dict(prototype=7,bundle='m_lupus_wolf.unity3d',sha256='f8e274a9e1c1ae479f88bf12067a440fad8bbf372258fc04bf732b582681aa38',
             animator=9221401832128272006,renderer=95028923182282536,auxiliary=1664068957102402253),
 'rhino':dict(prototype=4,bundle='m_coelodonta_rhino.unity3d',sha256='393ddf14feb608886486d39532c8b3b5cc0036495188c7831a0274ee5224c295',
              animator=-1210783088705582868,renderer=9061803555269046225,auxiliary=None)}


def pointer(value):
    if not isinstance(value,dict) or value.get('m_FileID')!=0 or type(value.get('m_PathID')) is not int:
        raise ValueError('Expected local object pointer')
    return value['m_PathID']


def numbers(value,keys):
    values=[value[k] for k in keys]
    if not all(type(v) in (int,float) and math.isfinite(v) for v in values):
        raise ValueError('Nonfinite transform/pose')
    return values


def hierarchy(transforms,game_objects,root):
    nodes=[];seen=set();hashes=set()
    def visit(identity,path,parent):
        if identity in seen or len(seen)>=256:raise ValueError('Cyclic/oversized hierarchy')
        seen.add(identity);t=transforms[identity];g=game_objects[pointer(t['m_GameObject'])]
        crc=zlib.crc32(path.encode())
        if crc in hashes:raise ValueError('Duplicate path or path-hash collision')
        hashes.add(crc)
        position=numbers(t['m_LocalPosition'],'xyz');rotation=numbers(t['m_LocalRotation'],'xyzw');scale=numbers(t['m_LocalScale'],'xyz')
        if abs(sum(v*v for v in rotation)-1)>.002 or any(abs(v)<1e-12 for v in scale):raise ValueError('Invalid transform TRS')
        nodes.append(dict(id=identity,path=path,path_crc32=crc,parent=parent,active=bool(g['m_IsActive']),position=position,rotation=rotation,scale=scale))
        for ref in t['m_Children']:
            child=pointer(ref)
            if pointer(transforms[child]['m_Father'])!=identity:raise ValueError('Inconsistent child/parent relationship')
            name=game_objects[pointer(transforms[child]['m_GameObject'])]['m_Name']
            if not name or '/' in name:raise ValueError('Invalid transform path component')
            visit(child,path+'/'+name if path else name,identity)
    visit(root,'',None);return nodes


def avatar_mapping(avatar,transforms,game_objects,animator_root):
    tos=avatar['m_TOS'];paths={}
    for key,path in tos:
        if key in paths or zlib.crc32(path.encode())!=key:raise ValueError('Invalid Avatar path map')
        paths[key]=path
    skeleton=avatar['m_Avatar']['m_AvatarSkeleton']['data'];ids=skeleton['m_ID'];parents=skeleton['m_Node']
    if not ids or len(ids)>256 or len(ids)!=len(parents) or len(set(ids))!=len(ids) or any(i not in paths for i in ids):
        raise ValueError('Invalid Avatar skeleton identity')
    for index,node in enumerate(parents):
        parent=node['m_ParentId']
        if not -1<=parent<index or (index==0 and parent!=-1) or (index>0 and parent<0):raise ValueError('Invalid Avatar parent order')
        if index and paths[ids[parent]]!=paths[ids[index]].rpartition('/')[0]:raise ValueError('Avatar hierarchy differs from path map')
    candidates=[];direct_missing=None
    for root in [animator_root,*[pointer(p) for p in transforms[animator_root]['m_Children']]]:
        nodes=hierarchy(transforms,game_objects,root);by_hash={n['path_crc32']:n for n in nodes}
        missing=[key for key in paths if key not in by_hash or by_hash[key]['path']!=paths[key]]
        if root==animator_root:direct_missing=missing
        if not missing:candidates.append((root,nodes))
    if len(candidates)!=1:raise ValueError('Avatar binding root missing or ambiguous')
    root,nodes=candidates[0];a=avatar['m_Avatar'];root_index=a['m_RootMotionBoneIndex']
    if not 0<=root_index<len(ids):raise ValueError('Invalid Avatar root motion index')
    return dict(binding_root=root,animator_root=animator_root,direct_animator_missing_paths=direct_missing,
                skeleton_nodes=len(ids),human_bone_count=len(a['m_HumanSkeletonIndexArray']),
                root_motion_bone_index=root_index,root_motion_path=paths[ids[root_index]],nodes=nodes)


def controller_leaves(controller,clips):
    names=dict(controller['m_TOS']);refs=controller['m_AnimationClips'];result=[]
    if len(refs)>256:raise ValueError('Oversized controller clip list')
    for machine in controller['m_Controller']['m_StateMachineArray']:
        for entry in machine['data']['m_StateConstantArray']:
            state=entry['data'];path=names.get(state['m_FullPathID'])
            if not path or not math.isfinite(state['m_Speed']):raise ValueError('Invalid controller state')
            leaves=[]
            for ti,tree in enumerate(state['m_BlendTreeConstantArray']):
                nodes=tree['data']['m_NodeArray']
                if len(nodes)>256:raise ValueError('Oversized blend tree')
                for ni,entry in enumerate(nodes):
                    node=entry['data'];children=node['m_ChildIndices']
                    if any(type(c) is not int or not 0<=c<len(nodes) for c in children):raise ValueError('Invalid blend child')
                    if children:continue
                    clip_index=node['m_ClipID']
                    if clip_index==4294967295:continue
                    if not 0<=clip_index<len(refs):raise ValueError('Invalid leaf clip index')
                    identity=pointer(refs[clip_index])
                    if identity not in clips:raise ValueError('Unresolved controller clip')
                    leaves.append(dict(tree=ti,node=ni,clip_index=clip_index,clip_id=identity,clip_name=clips[identity]['m_Name']))
            result.append(dict(path=path,speed=state['m_Speed'],loop=bool(state['m_Loop']),leaves=leaves))
    if len(result)>256:raise ValueError('Oversized controller states')
    return result


def clip_inventory(identity,clip,path_hashes):
    muscle=clip['m_MuscleClip'];data=muscle['m_Clip']['data'];start=muscle['m_StartTime'];end=muscle['m_StopTime']
    if not all(math.isfinite(v) for v in (start,end,clip['m_SampleRate'])) or not 0<=start<end<=120:raise ValueError('Invalid bounded clip clock')
    bindings=clip['m_ClipBindingConstant']['genericBindings']
    if len(bindings)>2048:raise ValueError('Oversized clip bindings')
    missing=set();special=set();transform_count=0
    for b in bindings:
        if b['classID']==4 and b['customType']==0 and not b['isPPtrCurve'] and pointer(b['script'])==0 and b['attribute'] in (1,2,3):
            transform_count+=1
            if b['path'] not in path_hashes:missing.add(b['path'])
        else:special.add((b['classID'],b['attribute'],b['customType'],b['path']))
    return dict(id=identity,name=clip['m_Name'],start=start,end=end,duration=end-start,sample_rate=clip['m_SampleRate'],legacy=bool(clip['m_Legacy']),
                streamed_curve_count=data['m_StreamedClip']['curveCount'],dense_curve_count=data['m_DenseClip']['m_CurveCount'],
                transform_binding_count=transform_count,unresolved_transform_path_hashes=sorted(missing),
                non_trs_bindings=[dict(class_id=c,attribute=a,custom_type=t,path_hash=p) for c,a,t,p in sorted(special)])


def inventory(bundle,descriptor):
    import UnityPy
    if not bundle.is_file() or not 0<bundle.stat().st_size<=64*1024*1024:raise ValueError('Missing/oversized installed bundle')
    before=hashlib.sha256(bundle.read_bytes()).hexdigest()
    if before!=descriptor['sha256']:raise ValueError('Installed reference bundle hash changed; audit descriptor explicitly')
    objects={o.path_id:o for o in UnityPy.load(str(bundle)).objects}
    if len(objects)>4096:raise ValueError('Oversized source object table')
    def read(identity,kind):
        if identity not in objects or objects[identity].type.name!=kind:raise ValueError('Missing/wrong source object type')
        return objects[identity].read_typetree()
    transforms={i:o.read_typetree() for i,o in objects.items() if o.type.name=='Transform'}
    game_objects={i:o.read_typetree() for i,o in objects.items() if o.type.name=='GameObject'}
    by_owner={pointer(t['m_GameObject']):i for i,t in transforms.items()}
    animator=read(descriptor['animator'],'Animator');avatar_id=pointer(animator['m_Avatar']);controller_id=pointer(animator['m_Controller'])
    avatar=read(avatar_id,'Avatar');controller=read(controller_id,'AnimatorController');root=by_owner[pointer(animator['m_GameObject'])]
    mapping=avatar_mapping(avatar,transforms,game_objects,root);nodes=mapping['nodes'];path_hashes={n['path_crc32'] for n in nodes}
    node_by_id={n['id']:n for n in nodes};renderer=read(descriptor['renderer'],'SkinnedMeshRenderer');bones=[pointer(b) for b in renderer['m_Bones']]
    if not renderer['m_Enabled'] or len(set(bones))!=len(bones) or any(b not in node_by_id for b in bones):raise ValueError('Invalid live skin references')
    mesh_id=pointer(renderer['m_Mesh']);mesh=read(mesh_id,'Mesh');submeshes=mesh['m_SubMeshes'];materials=renderer['m_Materials']
    if len(bones)!=len(mesh['m_BindPose']) or len(submeshes)!=len(materials):raise ValueError('Mesh/bind/material counts disagree')
    groups=[]
    for i,(sub,ref) in enumerate(zip(submeshes,materials)):
        material_id=pointer(ref);material=read(material_id,'Material')
        groups.append(dict(index=i,material_id=material_id,material_name=material['m_Name'],first_byte=sub['firstByte'],
                           index_count=sub['indexCount'],topology=sub['topology'],first_vertex=sub['firstVertex'],vertex_count=sub['vertexCount']))
    clips={i:o.read_typetree() for i,o in objects.items() if o.type.name=='AnimationClip'}
    leaf_states=controller_leaves(controller,clips);clip_data=[clip_inventory(i,t,path_hashes) for i,t in sorted(clips.items())]
    aux=descriptor['auxiliary'];aux_data=None
    if aux is not None:
        if aux not in node_by_id or aux in bones:raise ValueError('Expected auxiliary pounce transform differs')
        aux_data=dict(transform_id=aux,path=node_by_id[aux]['path'],parent=node_by_id[aux]['parent'],skin_joint=False)
    after=hashlib.sha256(bundle.read_bytes()).hexdigest()
    if before!=after:raise ValueError('Reference bundle changed during inspection')
    return dict(prototype=descriptor['prototype'],bundle=str(bundle.resolve()),sha256=before,bytes=bundle.stat().st_size,
                animator=dict(id=descriptor['animator'],owner_transform=root,avatar_id=avatar_id,avatar_name=avatar['m_Name'],
                              controller_id=controller_id,controller_name=controller['m_Name'],apply_root_motion=animator['m_ApplyRootMotion'],
                              enabled=bool(animator['m_Enabled']),has_transform_hierarchy=animator['m_HasTransformHierarchy']),
                avatar_mapping=mapping,controller_states=leaf_states,clips=clip_data,
                live_renderer=dict(id=descriptor['renderer'],mesh_id=mesh_id,skin_bones=bones,submeshes=groups),auxiliary_pounce=aux_data,
                unresolved_clip_path_hashes=sorted({h for c in clip_data for h in c['unresolved_transform_path_hashes']}),
                runtime_sampler_qualified=False,
                blockers=['Serialized Animator root and exact Avatar path root differ; installed generic binding must be observed on an isolated clone.',
                          'Clip bindings include missing transform paths and class95/custom8 channels; do not silently discard or invent transforms.',
                          'Streamed curves remain undecoded; no sampled poses or native actor output produced.',
                          'Unity runtime Avatar validity, evaluation behavior and retained-clone cleanup are unobserved.'])


def compact_report(result):
    """Preserve identities and blockers without repeating full rest hierarchies/trees."""
    species=[]
    for source in result['species']:
        mapping=source['avatar_mapping']
        species.append(dict(prototype=source['prototype'],bundle=source['bundle'],sha256=source['sha256'],bytes=source['bytes'],
            animator=source['animator'],avatar_mapping={k:v for k,v in mapping.items() if k!='nodes'},
            hierarchy_node_count=len(mapping['nodes']),controller_state_count=len(source['controller_states']),
            controller_clip_ids=sorted({leaf['clip_id'] for state in source['controller_states'] for leaf in state['leaves']}),
            clips=[{k:v for k,v in clip.items() if k not in ('non_trs_bindings','unresolved_transform_path_hashes')} for clip in source['clips']],
            non_trs_binding_kinds=sorted({(b['class_id'],b['attribute'],b['custom_type'],b['path_hash']) for c in source['clips'] for b in c['non_trs_bindings']}),
            unresolved_clip_path_hashes=source['unresolved_clip_path_hashes'],live_renderer=source['live_renderer'],
            auxiliary_pounce=source['auxiliary_pounce'],runtime_sampler_qualified=False,blockers=source['blockers']))
    return dict(schema=result['schema'],evidence=result['evidence'],tool_sha256=result['tool_sha256'],compact=True,species=species)


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game-dir',type=Path,default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    p.add_argument('--species',choices=['wolf','rhino','both'],default='both');p.add_argument('--output',type=Path,required=True)
    p.add_argument('--check',action='store_true',help='compare existing report without writing')
    p.add_argument('--summary',action='store_true',help='bounded provenance summary without full hierarchy/state trees')
    args=p.parse_args(argv)
    try:
        selected=DESCRIPTORS if args.species=='both' else {args.species:DESCRIPTORS[args.species]}
        result=dict(schema=1,evidence='Read-only installed generic-creature sampler prerequisite inventory; no runtime qualification or actor export.',
                    tool_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                    species=[inventory(args.game_dir/'AssetBundles/Monster'/d['bundle'],d) for d in selected.values()])
        if args.summary:result=compact_report(result)
        text=json.dumps(result,indent=2,allow_nan=False)+'\n'
        if args.check:
            if args.output.read_text()!=text:raise ValueError('Inventory report differs')
        else:
            with args.output.open('x') as f:f.write(text)
        print('Creature prerequisite inventory '+('checked' if args.check else 'written')+'; runtime sampler remains unqualified')
    except (OSError,ValueError,KeyError,TypeError,ImportError) as error:p.exit(1,str(error)+'\n')
    return 0


if __name__=='__main__':raise SystemExit(main())
