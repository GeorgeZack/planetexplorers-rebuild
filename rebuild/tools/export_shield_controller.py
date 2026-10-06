#!/usr/bin/env python3
"""Stage a callback-free copy of the exact source controller for isolated clones.

Offline only: emits a fresh local bundle, never launches or edits installed files.
The bundle contains only AnimatorController, AnimationClip and AssetBundle. All
state-machine behaviour references and all clip events are stripped BEFORE load.
Original mask/blend/state data and clip motion payloads are preserved. Runtime
loading/evaluation remains an explicit, separately supervised qualification.
"""
import argparse
import copy
import hashlib
import json
import struct
from pathlib import Path

from inventory_shield_layers import contract, sha

ROOT=Path(__file__).resolve().parents[2]
CONTROLLER_ID=779
CONTROLLER_NAME='ReferenceShieldCompositionNoCallbacks'
ASSET_NAME='reference_shield_controller'
TEMPLATE='AssetBundles/Item/b_puja_signal_tower01.unity3d'
MAX_BUNDLE=128*1024*1024


def require(value,message):
    if not value:raise ValueError(message)


def pointers(value,path=''):
    if isinstance(value,dict):
        if set(value)=={'m_FileID','m_PathID'}:
            if value['m_PathID']:yield path,value
        else:
            for key,item in value.items():yield from pointers(item,path+'/'+key)
    elif isinstance(value,(list,tuple)):
        for index,item in enumerate(value):yield from pointers(item,path+'/'+str(index))


def exact_tree(a,b):
    """Source blend tables contain NaNs; compare their float bit patterns."""
    if type(a) is not type(b):return False
    if isinstance(a,float):return struct.pack('<d',a)==struct.pack('<d',b)
    if isinstance(a,dict):return a.keys()==b.keys() and all(exact_tree(a[k],b[k]) for k in a)
    if isinstance(a,(tuple,list)):return len(a)==len(b) and all(exact_tree(x,y) for x,y in zip(a,b))
    return a==b


def controller_blob(raw):
    from UnityPy.helpers.Tpk import get_typetree_node
    from UnityPy.helpers.UnityVersion import UnityVersion
    from UnityPy.helpers.TypeTreeHelper import read_typetree
    from UnityPy.streams import EndianBinaryReader
    require(len(raw)>=8,'Truncated controller bytes')
    offset=(4+struct.unpack_from('<I',raw)[0]+3)&~3
    require(offset+4<=len(raw),'Invalid controller name length')
    # m_ControllerSize is a native-memory size, NOT the serialized extent.
    # Parse the exact ControllerConstant field to locate its byte boundary.
    node=get_typetree_node(91,UnityVersion.from_str('5.2.4f1')).m_Children[2]
    require(node.m_Name=='m_Controller','Unexpected controller field layout')
    reader=EndianBinaryReader(raw,endian='<');reader.Position=offset+4
    read_typetree(node,reader,check_read=False)
    require(reader.Position<=len(raw),'Invalid controller extent')
    return offset+4,raw[offset+4:reader.Position]


def clip_without_events(obj,original):
    # Events are the final aligned field in this exact source layout. Serialize
    # only that vector to measure its extent, retaining EVERY preceding byte,
    # including unknown source padding and compressed/humanoid motion payloads.
    from UnityPy.helpers.TypeTreeHelper import write_typetree
    from UnityPy.streams import EndianBinaryWriter
    node=obj._get_typetree_node()
    require(node.m_Children[-1].m_Name=='m_Events','Events are no longer terminal')
    writer=EndianBinaryWriter(endian=obj.reader.endian)
    write_typetree(original['m_Events'],node.m_Children[-1],writer,obj.assets_file)
    raw=obj.get_raw_data();size=len(writer.bytes)
    require(4<=size<=len(raw),'Invalid event-vector extent')
    return raw[:-size]+b'\0\0\0\0'


def sanitize_controller(original):
    contract(original)
    require(set(original)=={'m_Name','m_ControllerSize','m_Controller','m_TOS','m_AnimationClips',
                            'm_StateMachineBehaviourVectorDescription','m_StateMachineBehaviours'},
            'Unknown controller fields; callback safety must be re-audited')
    desc=original['m_StateMachineBehaviourVectorDescription']
    require(set(desc)=={'m_StateMachineBehaviourRanges','m_StateMachineBehaviourIndices'},'Unknown behaviour table')
    value=copy.deepcopy(original)
    value['m_Name']=CONTROLLER_NAME
    value['m_StateMachineBehaviours']=[]
    value['m_StateMachineBehaviourVectorDescription']={key:[] for key in desc}
    require(all(path.startswith('/m_AnimationClips/') and p['m_FileID']==0 for path,p in pointers(value)),
            'Sanitized controller has a nonlocal/nonclip object reference')
    return value


def sanitize_clip(original):
    value=copy.deepcopy(original)
    require('m_Events' in value,'Clip events field absent')
    value['m_Events']=[]
    require(not list(pointers(value)),'Clip retains object references after event removal')
    return value


def disable_clone_ik(controller):
    """Explicit isolated-evaluation adapter, never modifies the source object."""
    value=copy.deepcopy(controller)
    layers=value['m_Controller']['m_LayerArray']
    require(layers[0]['data']['m_IKPass'] is True,'Source Base Layer IK changed')
    require(all(not v['data']['m_IKPass'] for v in layers[1:]),'Unexpected source layer IK')
    layers[0]['data']['m_IKPass']=False
    states=value['m_Controller']['m_StateMachineArray'][0]['data']['m_StateConstantArray']
    matches=[v['data'] for v in states if v['data']['m_FullPathID']==2858398145]
    require(len(matches)==1 and matches[0]['m_IKOnFeet'] is True,'Source Base Move foot IK changed')
    matches[0]['m_IKOnFeet']=False
    return value


def clone_graph(obj,controller,disable_ik):
    source_graph=controller_blob(obj.get_raw_data())[1]
    if not disable_ik:return controller,source_graph,[]
    modified=disable_clone_ik(controller)
    before=controller_blob(obj.save_typetree(controller))[1]
    after=controller_blob(obj.save_typetree(modified))[1]
    require(len(before)==len(after)==len(source_graph),'IK flag serialization size changed')
    offsets=[i for i,(a,b) in enumerate(zip(before,after)) if a!=b]
    require(len(offsets)==2 and all(before[i]==source_graph[i]==1 and after[i]==0 for i in offsets),
            'IK adapter must change exactly two true-to-false bytes')
    patched=bytearray(source_graph)
    for i in offsets:patched[i]=0
    return modified,bytes(patched),offsets


def serialized_asset(bundle):
    values=[f for f in bundle.files.values() if hasattr(f,'objects')]
    require(len(values)==1 and len(bundle.files)==1,'Expected one serialized bundle file')
    return values[0]


def validate_bundle(path,controller,clips,graph_bytes=None,clip_bytes=None):
    import UnityPy
    env=UnityPy.load(str(path));require(len(env.files)==1,'Multiple bundle roots')
    bundle=next(iter(env.files.values()));sf=serialized_asset(bundle)
    require(sf.unity_version=='5.2.4f1' and sf.header.version==15,'Bundle serialization/version mismatch')
    require(sf._m_target_platform==25,'Bundle must retain source Linux64 platform')
    require(not sf.externals and not sf.script_types,'Bundle has external/script dependencies')
    require(set(sf.objects)=={CONTROLLER_ID,1,*clips},'Unexpected bundle object set')
    require({o.type.name for o in sf.objects.values()}=={'AnimatorController','AnimationClip','AssetBundle'},'Unsafe object type')
    require(exact_tree(sf.objects[CONTROLLER_ID].read_typetree(),controller),'Controller roundtrip differs')
    if graph_bytes is not None:
        require(controller_blob(sf.objects[CONTROLLER_ID].get_raw_data())[1]==graph_bytes,'Controller graph bytes changed')
    for identity,expected in clips.items():
        actual=sf.objects[identity].read_typetree()
        require(exact_tree(actual,expected),'Clip roundtrip differs: '+str(identity))
        if clip_bytes is not None:
            require(sf.objects[identity].get_raw_data()==clip_bytes[identity],'Clip motion bytes changed')
        require(not actual['m_Events'] and not list(pointers(actual)),'Unsafe clip references/events')
    meta=sf.objects[1].read_typetree()
    expected_ref={'m_FileID':0,'m_PathID':CONTROLLER_ID}
    require(meta['m_Container']==[(ASSET_NAME,{'preloadIndex':0,'preloadSize':len(clips)+1,'asset':expected_ref})],'Unexpected asset container')
    require(not meta['m_Dependencies'] and not meta['m_IsStreamedSceneAssetBundle'],'Bundle dependencies/scenes forbidden')
    preload=meta['m_PreloadTable'];require(preload==[{'m_FileID':0,'m_PathID':i} for i in [CONTROLLER_ID,*sorted(clips)]],'Preload dependencies differ')
    require(meta['m_MainAsset']=={'preloadIndex':0,'preloadSize':0,'asset':{'m_FileID':0,'m_PathID':0}},'Unexpected main asset')
    return {'objects':len(sf.objects),'animation_clips':len(clips),'callback_objects':0,'clip_events':0,
            'external_dependencies':0,'controller_name':CONTROLLER_NAME,'load_asset_name':ASSET_NAME}


def export(game,output,disable_ik=False):
    import UnityPy
    require(not output.exists(),'Output must be a fresh directory')
    require(game not in output.parents and output!=game,'Output must not modify game installation')
    source=game/'PE_Client_Data/sharedassets1.assets';template=game/TEMPLATE
    source_hash,template_hash=sha(source),sha(template)
    sf=next(iter(UnityPy.load(str(source)).files.values()))
    require(sf.unity_version=='5.2.4f1' and sf.header.version==15 and sf._m_target_platform==25,'Unsupported installed source')
    controller_original=sf.objects[CONTROLLER_ID].read_typetree();controller=sanitize_controller(controller_original)
    source_graph=controller_blob(sf.objects[CONTROLLER_ID].get_raw_data())[1]
    controller,clone_bytes,ik_offsets=clone_graph(sf.objects[CONTROLLER_ID],controller,disable_ik)
    ids={p['m_PathID'] for p in controller['m_AnimationClips']}
    require(1 not in ids and CONTROLLER_ID not in ids and len(ids)<=1024,'Unexpected clip reference set')
    original_clips={identity:sf.objects[identity].read_typetree() for identity in ids}
    require(all(sf.objects[i].type.name=='AnimationClip' for i in ids),'Controller reference is not a clip')
    clips={identity:sanitize_clip(value) for identity,value in original_clips.items()}
    clip_bytes={identity:clip_without_events(sf.objects[identity],value) for identity,value in original_clips.items()}
    bundle=next(iter(UnityPy.load(str(template)).files.values()));template_sf=serialized_asset(bundle)
    require(bundle.version_engine=='5.2.4f1' and template_sf.header.version==15,'Template must match Unity5.2.4')
    metadata=[o for o in template_sf.objects.values() if o.type.name=='AssetBundle']
    require(len(metadata)==1 and metadata[0].path_id==1,'Unexpected template AssetBundle identity')
    meta_obj=copy.copy(metadata[0]);meta_type=copy.copy(meta_obj.serialized_type)
    require(meta_type.class_id==142 and all(t.class_id!=142 for t in sf.types),'AssetBundle type conflict')
    sf.types=[t for t in sf.types if t.class_id in (74,91)] + [meta_type]
    require({t.class_id for t in sf.types}=={74,91,142} and len(sf.types)==3,'Unexpected serialized type table')
    sf.script_types=[];sf.externals=[]
    sf.objects={identity:sf.objects[identity] for identity in [CONTROLLER_ID,*sorted(ids)]}
    serialized=sf.objects[CONTROLLER_ID].save_typetree(controller);offset,rewritten=controller_blob(serialized)
    require(len(rewritten)==len(source_graph),'Controller blob length changed')
    sf.objects[CONTROLLER_ID].set_raw_data(serialized[:offset]+clone_bytes+serialized[offset+len(source_graph):])
    for identity,value in clip_bytes.items():sf.objects[identity].set_raw_data(value)
    meta_obj.assets_file=sf;meta_obj.serialized_type=meta_type;meta_obj.type_id=142;meta_obj.class_id=142
    reference={'m_FileID':0,'m_PathID':CONTROLLER_ID}
    meta={'m_Name':'ReferenceShieldComposition','m_PreloadTable':[{'m_FileID':0,'m_PathID':i} for i in [CONTROLLER_ID,*sorted(ids)]],
          'm_Container':[[ASSET_NAME,{'preloadIndex':0,'preloadSize':len(ids)+1,'asset':reference}]],
          'm_MainAsset':{'preloadIndex':0,'preloadSize':0,'asset':{'m_FileID':0,'m_PathID':0}},
          'm_RuntimeCompatibility':1,'m_AssetBundleName':'reference-shield-controller.unity3d',
          'm_Dependencies':[],'m_IsStreamedSceneAssetBundle':False}
    meta_obj.save_typetree(meta);sf.objects[1]=meta_obj
    # Fresh CAB identity avoids colliding with either original resource cache.
    cab='CAB-'+hashlib.sha256((source_hash+'shield-controller-no-callbacks-v1'+str(disable_ik)).encode()).hexdigest()[:32]
    sf.name=cab;sf.parent=bundle;bundle.files={cab:sf}
    payload=bundle.save(packer='none');require(len(payload)<=MAX_BUNDLE,'Bundle exceeds128MiB')
    output.mkdir(parents=True);path=output/'reference-shield-controller.unity3d';path.write_bytes(payload)
    verified=validate_bundle(path,controller,clips,clone_bytes,clip_bytes)
    require(sha(source)==source_hash and sha(template)==template_hash,'Source input changed')
    manifest={'schema':'pe-shield-controller-clone-1','source_asset_sha256':source_hash,'template_sha256':template_hash,
              'source_asset':str(source),'template':str(template),'controller_source_id':CONTROLLER_ID,
              'bundle':path.name,'bundle_sha256':sha(path),'bundle_bytes':len(payload),'cab':cab,
              'changes':['Controller renamed; all state-behaviour reference/range/index arrays removed',
                         'Every AnimationClip event removed; all other decoded clip data preserved',
                         'Only controller+referenced clips+fresh bundle metadata retained; scripts and externals removed'],
              'source_unique_behaviours':len({(p['m_FileID'],p['m_PathID']) for p in controller_original['m_StateMachineBehaviours']}),
              'source_events_removed':sum(len(c['m_Events']) for c in original_clips.values()),
              'controller_graph_masks_states_equal':not disable_ik,'ik_disabled':disable_ik,
              'ik_disabled_graph_byte_offsets':ik_offsets,
              'clone_graph_bytes_sha256':hashlib.sha256(clone_bytes).hexdigest(),
              'controller_graph_bytes_sha256':hashlib.sha256(source_graph).hexdigest(),
              'clip_prefix_bytes_preserved':True,
              'validated':verified,'runtime_load_validated':False,'runtime_composition_validated':False,
              'limits':['For future isolated Transform+Animator clones only; never assign to live player',
                        'Stripped callbacks alter runtime controller behavior; explicit clone layer/parameter/clock control is required',
                        'No original file, installed runtime asset or gameplay save modified',
                        'Offline roundtrip is not proof Unity will accept or safely evaluate the bundle']}
    (output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return manifest


def check(game,output):
    import UnityPy
    manifest=json.loads((output/'manifest.json').read_text())
    require(manifest.get('schema')=='pe-shield-controller-clone-1','Wrong staged manifest')
    source=game/'PE_Client_Data/sharedassets1.assets';template=game/TEMPLATE
    require(sha(source)==manifest['source_asset_sha256'] and sha(template)==manifest['template_sha256'],'Source hashes differ')
    path=output/'reference-shield-controller.unity3d'
    require(path.stat().st_size<=MAX_BUNDLE and sha(path)==manifest['bundle_sha256'],'Bundle hash/size differs')
    sf=next(iter(UnityPy.load(str(source)).files.values()))
    original=sf.objects[CONTROLLER_ID].read_typetree();controller=sanitize_controller(original)
    controller,clone_bytes,ik_offsets=clone_graph(sf.objects[CONTROLLER_ID],controller,manifest.get('ik_disabled',False))
    require(ik_offsets==manifest.get('ik_disabled_graph_byte_offsets',[]),'IK mutation offsets differ')
    ids={p['m_PathID'] for p in controller['m_AnimationClips']}
    clip_trees={i:sf.objects[i].read_typetree() for i in ids}
    clips={i:sanitize_clip(c) for i,c in clip_trees.items()}
    data={i:clip_without_events(sf.objects[i],c) for i,c in clip_trees.items()}
    validated=validate_bundle(path,controller,clips,clone_bytes,data)
    require(validated==manifest['validated'],'Validation manifest differs')
    return manifest


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game-dir',type=Path,default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    p.add_argument('--output',required=True,type=Path)
    p.add_argument('--disable-ik',action='store_true',help='isolated clone only: disable Base Layer IK callback pass and Base Move foot IK, preserving masks')
    p.add_argument('--check',action='store_true',help='verify existing stage read-only against exact source and raw motion bytes')
    a=p.parse_args(argv)
    try:
        game=a.game_dir.expanduser().resolve();output=a.output.expanduser().resolve()
        report=check(game,output) if a.check else export(game,output,a.disable_ik)
    except (OSError,ValueError,ImportError,KeyError,IndexError) as e:p.error(str(e))
    print(json.dumps({key:report[key] for key in ('bundle_bytes','validated','runtime_load_validated')}))


if __name__=='__main__':main()
