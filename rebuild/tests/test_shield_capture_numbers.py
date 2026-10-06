#!/usr/bin/env python3
"""Run the production sentinel policy on installed Mono, without loading Unity."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
GAME=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers'
RUNNER=r'''
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#define LOAD(name,ret,args) ret (*name) args; do {void *p=dlsym(lib,#name); if(!p)return 2;memcpy(&name,&p,sizeof p);}while(0)
int main(int argc,char **argv){
 if(argc!=4)return 2;
 void *lib=dlopen(argv[1],RTLD_NOW|RTLD_GLOBAL);if(!lib){fprintf(stderr,"%s\n",dlerror());return 2;}
 LOAD(mono_set_assemblies_path,void,(const char *));
 LOAD(mono_jit_init_version,void *,(const char *,const char *));
 LOAD(mono_domain_assembly_open,void *,(void *,const char *));
 LOAD(mono_assembly_get_image,void *,(void *));
 LOAD(mono_class_from_name,void *,(void *,const char *,const char *));
 LOAD(mono_class_get_method_from_name,void *,(void *,const char *,int));
 LOAD(mono_runtime_invoke,void *,(void *,void *,void **,void **));
 LOAD(mono_object_unbox,void *,(void *));
 LOAD(mono_print_unhandled_exception,void,(void *));
 mono_set_assemblies_path(argv[2]);void *d=mono_jit_init_version("shield-numeric-test","v2.0.50727");
 void *a=mono_domain_assembly_open(d,argv[3]);if(!a)return 2;
 void *c=mono_class_from_name(mono_assembly_get_image(a),"PeReference","NumberTests");
 void *m=c?mono_class_get_method_from_name(c,"Run",0):NULL;if(!m)return 2;
 void *error=NULL,*result=mono_runtime_invoke(m,NULL,NULL,&error);
 if(error){mono_print_unhandled_exception(error);return 1;}
 return result?*(int *)mono_object_unbox(result):2;
}
'''
TESTS=r'''
using System;
namespace PeReference {
public static class NumberTests {
 static void Check(bool value){if(!value)throw new Exception("Numeric policy assertion failed");}
 static void Reject(Action action,string context){try{action();}catch(InvalidOperationException e){Check(e.Message.Contains(context));return;}throw new Exception("Nonfinite active value accepted");}
 public static int Run(){
  string reason;
  Check(ShieldCaptureNumbers.Finite(1.25f,"matrix")=="1.25");
  Reject(delegate{ShieldCaptureNumbers.Finite(Single.NaN,"bone.arm.model[2,3]");},"bone.arm.model[2,3]");
  Reject(delegate{ShieldCaptureNumbers.Finite(Single.PositiveInfinity,"clip.weight");},"clip.weight");
  Check(ShieldCaptureNumbers.CurrentClock(Single.NaN,1,false,0,0,"inactive",out reason)=="null"&&reason=="inactive-clipless-layer");
  Check(ShieldCaptureNumbers.CurrentClock(Single.NegativeInfinity,5,false,0,0,"inactive",out reason)=="null");
  Check(ShieldCaptureNumbers.CurrentClock(4.5f,1,false,0,0,"inactive",out reason)=="4.5"&&reason==null);
  Reject(delegate{ShieldCaptureNumbers.CurrentClock(Single.NaN,0,false,0,0,"base",out reason);},"base");
  Reject(delegate{ShieldCaptureNumbers.CurrentClock(Single.NaN,3,false,0,0,"block",out reason);},"block");
  Reject(delegate{ShieldCaptureNumbers.CurrentClock(Single.NaN,1,true,0,0,"transition",out reason);},"transition");
  Reject(delegate{ShieldCaptureNumbers.CurrentClock(Single.NaN,1,false,1,0,"weighted",out reason);},"weighted");
  Reject(delegate{ShieldCaptureNumbers.CurrentClock(Single.NaN,1,false,0,1,"clip-bearing",out reason);},"clip-bearing");
  Check(ShieldCaptureNumbers.NextClock(Single.NaN,false,"absent",out reason)=="null"&&reason=="not-transitioning");
  Check(ShieldCaptureNumbers.NextClock(0f,false,"absent",out reason)=="null");
  Check(ShieldCaptureNumbers.NextClock(2.5f,true,"next",out reason)=="2.5"&&reason==null);
  Reject(delegate{ShieldCaptureNumbers.NextClock(Single.NaN,true,"active-next",out reason);},"active-next");
  Check(ShieldCaptureNumbers.StateLength(Single.PositiveInfinity,0,"length",out reason)=="null"&&reason=="frozen-effective-duration");
  Check(ShieldCaptureNumbers.StateLength(1.5f,0,"length",out reason)=="1.5"&&reason==null);
  Check(ShieldCaptureNumbers.StateLength(1.5f,1,"length",out reason)=="1.5"&&reason==null);
  Reject(delegate{ShieldCaptureNumbers.StateLength(Single.NaN,0,"length-NaN",out reason);},"length-NaN");
  Reject(delegate{ShieldCaptureNumbers.StateLength(Single.NegativeInfinity,0,"length-negative-infinity",out reason);},"length-negative-infinity");
  Reject(delegate{ShieldCaptureNumbers.StateLength(Single.PositiveInfinity,1,"length-moving",out reason);},"length-moving");
  Reject(delegate{ShieldCaptureNumbers.StateLength(Single.PositiveInfinity,-1,"length-reverse",out reason);},"length-reverse");
  Reject(delegate{ShieldCaptureNumbers.StateLength(1,Single.NaN,"length-speed",out reason);},"length-speed.animatorSpeed");
  return 0;
 }
}}
'''


class NumberTests(unittest.TestCase):
    def test_probe_preserves_context_and_uses_strict_active_policy(self):
        source=(ROOT/'rebuild/tools/ReferenceShieldCompositionProbe.cs').read_text()
        self.assertIn('CurrentClock(s.normalizedTime,i,transition,weight,currentClips.Length,context+',source)
        self.assertIn('NextClock(next.normalizedTime,transition,context+',source)
        self.assertIn('StateLength(s.length,a.speed,context+',source)
        self.assertIn('N(a.speed,"animator.speed")',source)
        self.assertIn('N(s.speed,context+',source)
        self.assertIn('N(s.speedMultiplier,context+',source)
        self.assertIn('N(clips[i].weight,context+',source)
        self.assertIn('N(m[r,c],context+',source)
        self.assertIn('N(a.GetFloat(p.name),"parameter."+p.name)',source)
    def test_actual_production_policy_offline_mono(self):
        managed=GAME/'PE_Client_Data/Managed';mono=GAME/'PE_Client_Data/Mono/x86_64/libmono.so'
        if not mono.is_file() or not (managed/'Mono.CSharp.dll').is_file():
            self.skipTest('installed original Mono required; no Unity native calls')
        temp_root=ROOT/'rebuild/build/compiler-tmp';temp_root.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='shield-number-',dir=temp_root) as directory:
            p=Path(directory);env=dict(os.environ,TMPDIR=str(temp_root))
            (p/'runner.c').write_text(RUNNER);(p/'Tests.cs').write_text(TESTS)
            def run(args):subprocess.run(list(map(str,args)),check=True,env=env,capture_output=True,text=True)
            run(['cc','-std=c11','-Wall','-Wextra','-Werror',p/'runner.c','-ldl','-o',p/'runner'])
            run(['cc','-std=c11','-Wall','-Wextra','-Werror',ROOT/'rebuild/tools/reference_pose_compile.c','-ldl','-o',p/'compiler'])
            run([p/'compiler',mono,managed,'-noconfig','-nostdlib','-target:library',f'-r:{managed/"mscorlib.dll"}',
                 f'-r:{managed/"System.Core.dll"}',f'-out:{p/"Tests.dll"}',ROOT/'rebuild/tools/ShieldCaptureNumbers.cs',p/'Tests.cs'])
            run([p/'runner',mono,managed,p/'Tests.dll'])


if __name__=='__main__':unittest.main()
