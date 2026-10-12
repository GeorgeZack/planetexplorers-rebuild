#!/usr/bin/env python3
"""Bind installed AnimFollow CIL and female controller settings; never execute Unity."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
import cil_evidence
import cil_disasm
from export_content import ROOT, describe_source
from export_human_phy import MANAGED
from export_hit_reaction import f
from export_reference_player import Assets
EVIDENCE='docs/reference/animfollow-lifecycle-il.json'
ASSEMBLIES={'Assembly-CSharp.dll':('56d90949e732331be7644fc9185dc088c78ad4888a100f60d96b7aa093f4d7d3','7ddcc464-5353-4750-b969-fe628ee474da')}
METHODS_DIGEST='0ce488147dcfc9f7ff0adba4311175d11c2553e40dbb96440958a4f5ea96e950'
FIELDS_DIGEST='bfb49eae6e4a1cfbb79843d722f59e5edbeec515a63dbffa5ecd157a480bbdef'
DATA=MANAGED.parent
ASSETS = {'mainData': '177d35cee6842910112d6de5c963136c6fe1fa0c9fdb24dc47df75e5c0c1ad5c', 'resources.assets': '363d02dc06aa3cd5ecccbeb67ac417077e0552cb32d0173205e6950ba93a3cb5', 'sharedassets0.assets': '7266577940dc7ecf4392a241d668248410f98fdf17cad3573f38a27daed13b1b'}
OBJECTS={'AnimFollow_AF':(324352,2496),'PERagdollController':(324629,564)}
BODY_DIGESTS={'AnimFollow_AF': 'c7e4f2dfabdc73feae02ec649c52087615348af21d56da3f2baad4f5a308f12f', 'PERagdollController': '75247af888e99b929f587261f02fb6f1f08dc798ce66d472fff404542039e5cf'}
PARAMS=('getupAngularDrag','getupDrag','fallLerp','residualTorque','residualForce','residualJointTorque','residualIdleFactor','graceSpeed','noGhostLimit2','settledSpeed','masterFallAnimatorSpeedFactor','getup1AnimatorSpeedFactor','getup2AnimatorSpeedFactor','contactTorque','contactForce','contactJointTorque','getupLerp1','getupLerp2','wakeUpStrength','toContactLerp','fromContactLerp','maxTorque','maxForce','maxJointTorque','maxErrorWhenMatching')
def require(value,message):
    if not value:raise ValueError(message)
def digest(value):return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode()).hexdigest()
def load_evidence(root=ROOT):
    evidence,_=cil_evidence.load(root/EVIDENCE,ASSEMBLIES,METHODS_DIGEST,{})
    fields=evidence['assemblies']['Assembly-CSharp.dll']['fields']
    require(digest(fields)==FIELDS_DIGEST,'AnimFollow field layout evidence changed')
    return evidence

def decode_component(raw,fields,classes):
    # Unity5 serialized header is 32 bytes for these pinned installed objects:
    # GameObject PPtr(12), enabled(4), MonoScript PPtr(12), empty name(4).
    require(len(raw)>=32 and struct.unpack_from('<I',raw,28)[0]==0,'Unexpected component header')
    cursor=32;values={};layout=[]
    def read(t):
        nonlocal cursor
        if t.endswith('[]'):
            count=read('i4');require(0<=count<=128,'Invalid serialized array bound')
            return [read(t[:-2]) for _ in range(count)]
        if t.startswith('class:'):
            value=struct.unpack_from('<iq',raw,cursor);cursor+=12;return dict(file=value[0],id=value[1])
        if t=='string':
            count=read('i4');require(0<=count<=128 and cursor+count<=len(raw),'Invalid serialized string')
            value=raw[cursor:cursor+count].decode();cursor=(cursor+count+3)&~3;return value
        if t=='bool':
            value=raw[cursor];require(value in (0,1),'Invalid serialized bool');cursor=(cursor+4)&~3;return bool(value)
        if t in ('r4','i4'):
            value=struct.unpack_from('<f' if t=='r4' else '<i',raw,cursor)[0];cursor+=4
            require(not isinstance(value,float) or math.isfinite(value),'Nonfinite serialized float');return value
        if t=='valuetype:33':return [read('r4') for _ in range(3)]
        raise ValueError('Unsupported serialized field '+t)
    try:
        for cls in classes:
            for field in fields:
                flags=field['flags']
                if field['declaring_type']!=cls or not ((flags&7)==6 and not flags&0xb0 or field['serialize_field']):continue
                offset=cursor;values[field['name']]=read(field['type'])
                layout.append(dict(token=field['token'],name=field['name'],type=field['type'],offset=offset,end=cursor))
        require(cursor==len(raw),'Serialized component not fully consumed')
    except (struct.error,IndexError,UnicodeError) as error:raise ValueError('Malformed serialized AnimFollow component') from error
    return dict(values=values,layout=layout)

def qualify_component(raw,cls,evidence):
    require(cls in OBJECTS,'Unknown AnimFollow component')
    _,size=OBJECTS[cls]
    require(len(raw)==size and hashlib.sha256(raw).hexdigest()==BODY_DIGESTS[cls],
            'Installed AnimFollow payload changed: '+cls)
    fields=evidence['assemblies']['Assembly-CSharp.dll']['fields']
    classes=('AnimFollow.RagdollControl_AF','PERagdollController') if cls=='PERagdollController' else ('AnimFollow.AnimFollow_AF',)
    return decode_component(raw,fields,classes)

def recover_components(evidence,data=DATA):
    for name,sha in ASSETS.items():require(hashlib.sha256((data/name).read_bytes()).hexdigest()==sha,'Unqualified installed '+name)
    assets=Assets(data);manager=next(o for o in assets.file('mainData').objects.values() if o.type.name=='ResourceManager')
    roots=[assets.resolve(manager,ref) for key,ref in manager.read_typetree()['m_Container'] if key=='prefab/playerprefab/femalemodel']
    require(len(roots)==1 and roots[0].assets_file.name=='resources.assets' and roots[0].path_id==47841,'Female model resource binding changed')
    file=assets.file('resources.assets');fields=evidence['assemblies']['Assembly-CSharp.dll']['fields'];result={}
    for cls,(identity,size) in OBJECTS.items():
        o=file.objects[identity];raw=o.get_raw_data()
        decoded=qualify_component(raw,cls,evidence)
        # The installed stripped MonoBehaviour tree decodes only the common
        # header. The derived field layout is bound to the assembly metadata.
        header=o.read_typetree(check_read=False);script=assets.resolve(o,header['m_Script']).read_typetree()
        require(script['m_ClassName']==cls and header['m_Enabled']==1,'Installed AnimFollow class/enabled state changed')
        result[cls]=dict(file='resources.assets',id=identity,raw_sha256=BODY_DIGESTS[cls],byte_length=size,
                         **decoded)
    return result

def hash_names(evidence):
    method=next(m for m in evidence['assemblies']['Assembly-CSharp.dll']['methods'] if m['declaring_type']=='AnimFollow.HashIDs')
    # Pinning the CIL binds order and StringToHash calls. Native string hashing
    # is not evaluated here; names are supplied to the caller for classification.
    return [method['strings']['0x%08x'%token] for _,op,token in cil_disasm.decode(bytes.fromhex(method['raw_il_hex'])) if op=='ldstr']

def build_outputs(root=ROOT,data=DATA,managed=MANAGED):
    evidence=load_evidence(root);cil_evidence.qualify_installed(evidence,ASSEMBLIES,managed)
    components=recover_components(evidence,data);control=components['PERagdollController']['values'];follow=components['AnimFollow_AF']['values'];names=hash_names(evidence)
    header='/* Generated by export_animfollow.py; do not edit. */\n#ifndef PE_ANIMFOLLOW_SOURCE_H\n#define PE_ANIMFOLLOW_SOURCE_H\n'
    header+='typedef struct {'+''.join('float '+k+';' for k in PARAMS)+'} PeAnimFollowSourceParams;\n'
    header+='static const PeAnimFollowSourceParams pe_animfollow_source={'+','.join('.%s=%s'%(k,f(control[k])) for k in PARAMS)+'};\n'
    header+='static const unsigned pe_animfollow_source_secondary_update=%d;\n'%follow['secondaryUpdate']
    for k in ('drag','angularDrag'):header+='static const float pe_animfollow_source_%s=%s;\n'%(k,f(follow[k]))
    lock_method=next(m for m in evidence['assemblies']['Assembly-CSharp.dll']['methods'] if m['declaring_type']=='AnimFollow.AnimFollow_AF' and m['name']=='set_active')
    lock_tag,=lock_method['strings'].values()
    header+='static const char *const pe_animfollow_source_lock_tag=%s;\n'%json.dumps(lock_tag)
    header+='static const char *const pe_animfollow_source_names[]={'+','.join(json.dumps(s) for s in names)+'};\n#endif\n'
    metadata=dict(schema=1,evidence=describe_source(root/EVIDENCE),artifact_sha256=ASSEMBLIES['Assembly-CSharp.dll'][0],methods_digest=METHODS_DIGEST,fields_digest=FIELDS_DIGEST,
        installed_sources=[dict(path='installed/PE_Client_Data/'+n,sha256=sha) for n,sha in ASSETS.items()],components=components,names=names,body_lock_tag=lock_tag,
        scope='Disconnected female lifecycle/scalar drive requests and active body policy. No PhysX, PD solve, joint updates, root orientation solve or Animator evaluation.',
        limitations=evidence['limitations']+['Installed component fields are decoded with a pinned serialization layout; constructor defaults are not substituted for serialized settings.','Get-up selection emits the compiled SetBool(hash,true) request, not an assumed SetTrigger implementation.'])
    return {'pe_animfollow_source.h':header.encode(),'pe_animfollow_source.json':(json.dumps(metadata,indent=2)+'\n').encode()}
def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');p.add_argument('--output',type=Path,default=ROOT/'rebuild/generated');a=p.parse_args(argv)
    try:
        for name,data in build_outputs().items():
            if a.check:require((a.output/name).is_file() and (a.output/name).read_bytes()==data,'Stale or missing '+name)
            else:a.output.mkdir(parents=True,exist_ok=True);(a.output/name).write_bytes(data)
    except (OSError,ValueError,KeyError,ImportError,IndexError) as error:print(error,file=sys.stderr);return 1
    return 0
if __name__=='__main__':raise SystemExit(main())
