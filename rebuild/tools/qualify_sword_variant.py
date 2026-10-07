#!/usr/bin/env python3
"""Compare pure installed Mono Vector3 magnitude with C; never launch the game.

Loads only the SHA-pinned Mono runtime and managed math/compiler assemblies in
bounded child processes. Builds fresh private artifacts under rebuild/build.
No Unity scene, GameObject, renderer, input, Steam launch or save is touched.
"""
import argparse
import ctypes as C
import hashlib
import json
import math
from pathlib import Path
import random
import struct
import subprocess
from export_content import ROOT
from export_capsule_history import UNITY_SHA256
PINS={'Managed/UnityEngine.dll':UNITY_SHA256,
      'Mono/x86_64/libmono.so':'e11e7d56ffa83acdfdd2325743e29a08952308021b50a7deb05662c1c811a1b7',
      'Managed/mscorlib.dll':'cd25a2f5c5bda4baa40cf07e34856e1f022fdd22c9ce7829c351686ebe7d6df7',
      'Managed/Mono.CSharp.dll':'65168869b7f6ceec85ccf072257866fbd3f19de235ba036d0a5a73ffdb0fb46e'}
ORACLE='''using System;using System.IO;using UnityEngine;
public class Oracle{public static int Main(string[] args){using(var w=new StreamWriter(args[1]))foreach(string line in File.ReadAllLines(args[0])){string[] p=line.Split('|');float[] v=new float[3];for(int i=0;i<3;i++)v[i]=BitConverter.ToSingle(BitConverter.GetBytes(Convert.ToUInt32(p[i],16)),0);float s=new Vector3(v[0],v[1],v[2]).magnitude;w.WriteLine(BitConverter.ToUInt32(BitConverter.GetBytes(s),0).ToString("x8")+"|"+(s>7?2:s>4.5f?1:0));}return 0;}}
'''
RUNNER='''#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#define LOAD(name,ret,args) ret (*name) args; do {void *p=dlsym(lib,#name);if(!p)return 2;memcpy(&name,&p,sizeof p);}while(0)
int main(int argc,char **argv){if(argc<5)return 2;void *lib=dlopen(argv[1],RTLD_NOW|RTLD_GLOBAL);if(!lib){puts(dlerror());return 2;}
LOAD(mono_set_assemblies_path,void,(const char*));LOAD(mono_jit_init_version,void*,(const char*,const char*));LOAD(mono_domain_assembly_open,void*,(void*,const char*));LOAD(mono_jit_exec,int,(void*,void*,int,char**));
mono_set_assemblies_path(argv[2]);void *d=mono_jit_init_version("sword-pure-math","v2.0.50727");void*a=mono_domain_assembly_open(d,argv[3]);if(!a)return 2;return mono_jit_exec(d,a,argc-3,argv+3);}
'''
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def f32(x):return struct.unpack('<f',struct.pack('<f',x))[0]
def bits(x):return struct.pack('>f',x).hex()
def cases():
    r=random.Random(0x5147)
    rows=[(0.,0.,0.),(2.7,3.6,0.),(4.2,5.6,0.),(1.5,3.,3.),(-2.2190206050872803,-2.0396177768707275,3.341542959213257)]
    for threshold in (4.5,7.):
        for _ in range(2048):
            a=[r.uniform(-1,1) for i in range(3)];norm=math.sqrt(sum(x*x for x in a));factor=threshold*(1+r.uniform(-3e-7,3e-7))/norm
            rows.append(tuple(x*factor for x in a))
    return [tuple(f32(x) for x in a) for a in rows]
def run(args):
    b=args.build.resolve();report=args.report.resolve();allowed=(ROOT/'rebuild/build').resolve()
    if not b.is_relative_to(allowed) or b==allowed:raise ValueError('Build must be a fresh child of rebuild/build')
    if not (report.is_relative_to(allowed) or report.is_relative_to((ROOT/'docs/reference').resolve())):raise ValueError('Report must be under rebuild/build or docs/reference')
    if b.exists() or report.exists():raise ValueError('Build directory and report must be fresh')
    game=args.game.resolve()/'PE_Client_Data'
    for rel,digest in PINS.items():
        if sha(game/rel)!=digest:raise ValueError('Unqualified installed math runtime '+rel)
    b.mkdir(parents=True);(b/'Oracle.cs').write_text(ORACLE);(b/'run.c').write_text(RUNNER)
    invoke=lambda a:subprocess.run([str(x) for x in a],check=True,timeout=60)
    invoke(['cc','-std=c11','-O2',ROOT/'rebuild/tools/reference_pose_compile.c','-ldl','-o',b/'compiler'])
    invoke(['cc','-std=c11','-O2',b/'run.c','-ldl','-o',b/'run'])
    managed=game/'Managed';mono=game/'Mono/x86_64/libmono.so'
    invoke([b/'compiler',mono,managed,'-noconfig','-nostdlib','-target:exe','-r:'+str(managed/'mscorlib.dll'),'-r:'+str(managed/'UnityEngine.dll'),'-out:'+str(b/'Oracle.exe'),b/'Oracle.cs'])
    invoke(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-shared','-fPIC','-I'+str(ROOT/'rebuild/src'),ROOT/'rebuild/src/pe_sword_variant.c','-lm','-o',b/'libvariant.so'])
    rows=cases();(b/'cases.txt').write_text('\n'.join('|'.join(map(bits,a)) for a in rows)+'\n')
    invoke([b/'run',mono,managed,b/'Oracle.exe',b/'cases.txt',b/'oracle.txt'])
    class Vec(C.Structure):_fields_=[(k,C.c_float) for k in ('x','y','z')]
    class VariantInput(C.Structure):
        _fields_=[('velocity',Vec),('movement_present',C.c_bool),('in_air',C.c_bool),
                  ('motor_present',C.c_bool),('spine_in_water',C.c_bool)]
    library=C.CDLL(str(b/'libvariant.so'))
    fn=library.pe_sword_variant_speed;fn.argtypes=[Vec,C.POINTER(C.c_float)];fn.restype=C.c_bool
    select=library.pe_sword_variant_select;select.argtypes=[VariantInput,C.POINTER(C.c_int)];select.restype=C.c_bool
    lines=(b/'oracle.txt').read_text().splitlines()
    if len(lines)!=len(rows):raise ValueError('Incomplete oracle output')
    mismatches=[];golden=[]
    for i,(a,line) in enumerate(zip(rows,lines)):
        expected,branch=line.split('|');out=C.c_float()
        if not fn(Vec(*a),C.byref(out)):raise ValueError('C rejected finite fixture')
        actual_branch=C.c_int(-1)
        if not select(VariantInput(Vec(*a),True,False,True,False),C.byref(actual_branch)):
            raise ValueError('C selector rejected finite grounded fixture')
        if bits(out.value)!=expected or actual_branch.value!=int(branch):mismatches.append(i)
        if i<5:golden.append(dict(input=a,speed_bits=expected,branch=int(branch)))
    sources=['rebuild/tools/qualify_sword_variant.py','rebuild/tools/reference_pose_compile.c','rebuild/src/pe_sword_variant.c','rebuild/src/pe_sword_variant.h','rebuild/generated/pe_sword_variant_source.h']
    result=dict(schema=1,cases=len(rows),exact_matches=len(rows)-len(mismatches),mismatch_count=len(mismatches),first_mismatches=mismatches[:20],golden=golden,
        runtime='SHA-pinned installed libmono embedding, UnityEngine.Vector3.magnitude/Mathf.Sqrt only; no game or engine initialization',
        comparison='Actual C pe_sword_variant_speed float bits AND pe_sword_variant_select enum; movement_present=true,motor_present=true,in_air=false,spine_in_water=false.',
        corpus='Seed0x5147,2048 vectors within relative3e-7 of each threshold4.5/7 plus5 explicit cases; exact binary float input encoding',
        installed=[dict(file=p,sha256=h) for p,h in PINS.items()],
        sources=[dict(file=p,sha256=sha(ROOT/p)) for p in sources],
        artifacts=[dict(file=p,sha256=sha(b/p)) for p in ('Oracle.cs','run.c','cases.txt','oracle.txt','libvariant.so')],
        reproduction=dict(tool='rebuild/tools/qualify_sword_variant.py',arguments='--game STEAM_GAME_DIR --build rebuild/build/FRESH_NAME --report docs/reference/FRESH_NAME.json'),
        limitations='Bounded arithmetic/branch corpus, not universal bit-exactness, live Action_Sword execution, root motion or pose qualification.')
    report.parent.mkdir(parents=True,exist_ok=True)
    with report.open('x') as f:json.dump(result,f,indent=2);f.write('\n')
    if mismatches:raise ValueError('Numerical comparison failed')
    print('Sword magnitude oracle:',len(rows),'exact float/branch matches')
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--game',type=Path,required=True);p.add_argument('--build',type=Path,required=True);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
    try:run(a)
    except (ValueError,OSError,subprocess.SubprocessError) as err:p.error(str(err))
if __name__=='__main__':main()
