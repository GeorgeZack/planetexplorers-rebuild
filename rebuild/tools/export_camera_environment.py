#!/usr/bin/env python3
"""Guard active CameraForge activity-space and post-obstruction recovery rules."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
from export_content import ROOT, describe_source

UPDATE='Assets/PeCamera/Resources/Controllers/PECamera_Update.asset'
START='Assets/PeCamera/Resources/Controllers/PECamera_Start.asset'
GUARDS = {'Assets/PeCamera/Resources/Controllers/PECamera_Update.asset': '253ba07513eae22cf3f4e404d63a9942ddea7c13bfb12215bd476000f3760938', 'Assets/PeCamera/Resources/Controllers/PECamera_Start.asset': 'e1956bec6ee32bc4c9e304f05211b9114adc2f4d7f44c784fa6fddc206650694', 'Assets/PeCamera/Scripts/PeCamera.cs': '8c3436eec6f52e7a88ee95711f517f9fe80fb25552fa51496ecd90ccdf0eb63f', 'Assets/PeCamera/Scripts/ScriptModifier/ThirdPersonFollow.cs': '8ebbdcca0e791b6d374da00c958738ce401bea34e73ca773f1c9d3c8323dd0a1', 'Assets/PeCamera/Scripts/ScriptModifier/GeometryClamp.cs': 'e207bc211d09be330261845669e3b6f097674fc2a6dd904c03b14a7ba009670a', 'Assets/Camera Forge/Scripts/Utilities/Utils.cs': '59e8f0b2b6be7c18290df7b782cace3ce459e93049bc14d8961119bf3e23e526', 'Assets/Camera Forge/Scripts/Assets/ControllerAsset.cs': '2e69314593b98958148274e8eb445ba89c65572df4dd9755bde930d919d0206b', 'Assets/Camera Forge/Scripts/Core/Controller/PoseNode.cs': '815a41e9d966f7483b61ca0381a4c4a17586abc704ff44fc3b8767f0410a5730', 'Assets/Camera Forge/Scripts/Core/Modifier/Node.cs': '7c986e1cdd5ba83041bd49e1043a91fc9fda2829b987724827642ed1818622df', 'Assets/Camera Forge/Scripts/Core/Modifier/Var.cs': '623cfa397b9eab6d4673739a1546531fe7eb3bbad08a5f871b2d0396a83a1ca9', 'Assets/Camera Forge/Scripts/Assets/ModifierAsset.cs': 'b9fda7b03a6f2142c5093b6b7e1a0ea47981cc658bc01456a3b443237fe8fcae'}

class Reader:
    def __init__(self,data):self.data=data;self.at=0
    def take(self,n):
        if n<0 or self.at+n>len(self.data):raise ValueError('Truncated camera graph')
        result=self.data[self.at:self.at+n];self.at+=n;return result
    def integer(self):return struct.unpack('<i',self.take(4))[0]
    def number(self):return struct.unpack('<f',self.take(4))[0]
    def count(self):
        n=self.integer()
        if n<0 or n>4096:raise ValueError('Invalid camera graph count')
        return n
    def string(self):
        n=0
        for shift in range(0,35,7):
            b=self.take(1)[0];n|=(b&127)<<shift
            if b<128:return self.take(n).decode('utf-8')
        raise ValueError('Invalid camera graph string')
    def value(self):
        kind=self.integer()
        if kind==0:return None
        if kind<1 or kind>7:raise ValueError('Invalid graph value kind')
        boolean=self.take(1)[0];integer=self.integer();self.integer()
        number=self.number();vector=[self.number() for _ in range(4)]
        self.take(32);text=self.string()
        return dict(kind=kind,boolean=boolean,integer=integer,number=number,vector=vector,text=text)
    def node(self,kind,pose=False):
        self.take(8);slots={}
        for _ in range(self.count()):
            name=self.string();index=self.integer();slots[name]=dict(input=index,value=self.value() if index==-1 else None)
        result=dict(kind=kind,slots=slots)
        if pose:
            poses={}
            for _ in range(self.count()):name=self.string();poses[name]=self.integer()
            result['poses']=poses;payload=self.take(self.integer())
            if payload:result['modifier']=modifier(payload)
        return result

def modifier(payload):
    r=Reader(payload)
    if r.string()!='ModifierAsset' or r.integer()!=10000:raise ValueError('Invalid embedded modifier')
    count=r.count();types=[r.string() for _ in range(count)];nodes=[r.node(t) for t in types]
    output=r.string();output=r.node(output) if output else None;r.take(8)
    if r.at!=len(payload):raise ValueError('Trailing modifier payload')
    return dict(nodes=nodes,output=output)

def graph(path):
    raw=path.read_bytes();marker=b'\x0fControllerAsset';at=raw.find(marker)
    if at<0 or raw.find(marker,at+1)>=0:raise ValueError('Ambiguous controller payload')
    r=Reader(raw[at:]);assert r.string()=='ControllerAsset'
    if r.integer()!=10000:raise ValueError('Unsupported controller format')
    count,poses=r.count(),r.count()
    types=[r.string() for _ in range(count)];nodes=[r.node(t) for t in types]
    types=[r.string() for _ in range(poses)];poses=[r.node(t,True) for t in types]
    final=r.node('Final',True);r.take(8)
    return dict(nodes=nodes,poses=poses,final=final)

def build_outputs(root=ROOT):
    for name,expected in GUARDS.items():
        if hashlib.sha256((root/name).read_bytes()).hexdigest()!=expected:raise ValueError('Camera source changed: '+name)
    start=graph(root/START)['poses'][1]['modifier']['nodes']
    for index,name in ((6,'Dist'),(7,'DistWanted'),(8,'DistLevel')):
        if start[index]['kind']!='CameraForge.SetUserVar' or start[index]['slots']['Var']['value']['text']!=name:raise ValueError('Camera initial distance binding changed')
    if start[6]['slots']['Value']['value']['number']!=6 or start[7]['slots']['Value']['input']!=6 or start[8]['slots']['Value']['input']!=7:raise ValueError('Camera initial distance changed')
    update=graph(root/UPDATE);normal=update['poses'][1];cursor=update['poses'][24];clamp=update['poses'][3];finding=update['poses'][4]
    maximum=update['nodes'][7]
    build=update['nodes'][maximum['slots']['T']['input']]
    if build['kind']!='CameraForge.GetUserVar' or build['slots']['Var']['value']['text']!='Build Mode':raise ValueError('Distance maximum lost Build Mode binding')
    if normal['kind']!='CameraForge.ThirdPersonFollow' or normal['slots']['Distance Max']['input']!=7 or abs(normal['slots']['Distance Min']['value']['number']-1.8)>1e-6:
        raise ValueError('Normal camera distance graph changed')
    if maximum['kind']!='CameraForge.Lerp' or maximum['slots']['A']['value']['number']!=10 or maximum['slots']['B']['value']['number']!=20:
        raise ValueError('Normal/build camera maxima changed')
    if cursor['poses']['Prev']!=1 or clamp['kind']!='CameraForge.GeometryClamp' or clamp['poses']['Prev']!=24 or finding['poses']['Prev']!=3:
        raise ValueError('Camera modifier order changed')
    header='''/* Generated by export_camera_environment.py; regenerate, do not edit. */
#ifndef PE_CAMERA_ENVIRONMENT_SOURCE_H
#define PE_CAMERA_ENVIRONMENT_SOURCE_H
#define PE_CAMERA_ACTIVITY_MAX 50.f
#define PE_CAMERA_ACTIVITY_ACCURACY 4.f
#define PE_CAMERA_CLAMP_HOLD 2.f
#define PE_CAMERA_HOLD_DT_MIN .001f
#define PE_CAMERA_HOLD_DT_MAX .1f
#endif
'''
    contract=dict(activity=dict(minimum_argument_unused=.5,maximum=50,accuracy=4,weight=[0,1,0],pitch_min_included=-35,return_rule='max(minimum hit distance, weighted average*.5)',obstacle_mask=8468481,ignore_triggers=True),
        distance=dict(default=6,minimum=normal['slots']['Distance Min']['value']['number'],maximum_normal=10,maximum_build=20,smooth_time=.5,activity_coefficient=[.15,.15,1],activity_max_multiplier=2),
        recovery=dict(seconds=2,hold_dt_min=.001,hold_dt_max=.1,damp_dt='Time.deltaTime, unclamped',order=['wheel clears hold','positive hold decremented by clamped dt','prior geometry latch sets hold2 and clears latch','target activity calculated','unlocked smooth to target; locked only smooth outward if below minimum']),
        graph=dict(follow=1,cursor=24,geometry=3,yaw_pitch_finding=4,normal_follow=normal,distance_max_node=maximum,build_mode_input=build,initial_distance_nodes=start[6:9]))
    manifest=dict(format_version=1,sources=[describe_source(root/p) for p in GUARDS],contract=contract,
      limitations=['Source-derived; no new Unity/native physical observation.',
       'Native anchor and sphere collision remain host adapters; full near-plane rings, rise, yaw/pitch recovery and graph blends remain separate.',
       'Native pause/zero-dt/first-person freeze; immediate upper mode bound retained; one display update only.',
       'C float trigonometry is not a claim of bit-identical Unity physics sampling.'])
    return {'pe_camera_environment_source.h':header.encode(),'pe_camera_environment_source.json':(json.dumps(manifest,indent=2)+'\n').encode()}

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--check',action='store_true');parser.add_argument('--output',type=Path,default=ROOT/'rebuild/generated');args=parser.parse_args(argv)
    try:
        for name,data in build_outputs().items():
            path=args.output/name
            if args.check:
                if not path.is_file() or path.read_bytes()!=data:raise ValueError('Stale camera output '+name)
            else:path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
    except (OSError,ValueError,KeyError,struct.error) as e:print(e,file=sys.stderr);return 1
    print('Camera environment checked' if args.check else 'Camera environment generated');return 0
if __name__=='__main__':raise SystemExit(main())
