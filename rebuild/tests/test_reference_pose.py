"""Offline tests: fake Mono calls only; never launch/attach the original game."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))
import reference_pose

FAKE = r'''
#include <stddef.h>
#include <string.h>
static int installs, calls;
void *mono_runtime_invoke(void *m,void *o,void **a,void **e){(void)o;(void)a;(void)e;if(!strcmp(m,"Install"))installs++;else calls++;return NULL;}
const char *mono_method_get_name(void *m){return m;}
void *mono_method_get_class(void *m){(void)m;return (void*)1;}
const char *mono_class_get_name(void *c){(void)c;return "PeSingletonRunner";}
const char *mono_class_get_namespace(void *c){(void)c;return "Pathea";}
void *mono_domain_get(void){return (void*)1;}
void *mono_domain_assembly_open(void *d,const char *p){(void)d;(void)p;return (void*)1;}
void *mono_assembly_get_image(void *a){return a;}
void *mono_class_from_name(void *i,const char *n,const char *c){(void)i;(void)n;(void)c;return (void*)1;}
void *mono_class_get_method_from_name(void *c,const char *n,int count){(void)c;(void)count;return (void*)n;}
void mono_print_unhandled_exception(void *e){(void)e;}
int counts(void){return 100*installs+calls;}
'''
HOST = r'''
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
typedef void *(*Invoke)(void *,void *,void **,void **);
static Invoke call;
static void *worker(void *x){(void)x;call("Update",NULL,NULL,NULL);return NULL;}
int main(int argc,char **argv){if(argc!=2)return 2;void *lib=dlopen(argv[1],RTLD_NOW);if(!lib)return 3;
void *p=dlsym(lib,"mono_runtime_invoke");memcpy(&call,&p,sizeof call);if(!call)return 4;
int (*counts)(void);p=dlsym(lib,"counts");memcpy(&counts,&p,sizeof counts);
call("Wrong",NULL,NULL,NULL);pthread_t thread;pthread_create(&thread,NULL,worker,NULL);pthread_join(thread,NULL);
if(counts()!=2)return 5; /* Neither wrong callback nor worker may install. */
call("Update",NULL,NULL,NULL);call("Update",NULL,NULL,NULL);
printf("%d\n",counts());return 0;}
'''

class PoseTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.temp.name)
        cls.fake, cls.host, cls.shim = [cls.root / n for n in ('fake.so', 'host', 'shim.so')]
        (cls.root / 'fake.c').write_text(FAKE)
        (cls.root / 'host.c').write_text(HOST)
        for command in (
            ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC', str(cls.root / 'fake.c'), '-o', str(cls.fake)],
            ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', str(cls.root / 'host.c'), '-ldl', '-pthread', '-o', str(cls.host)],
            ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC', str(TOOLS / 'reference_pose_shim.c'), '-ldl', '-o', str(cls.shim)]):
            subprocess.run(command, check=True, capture_output=True)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def invoke(self, **override):
        env = os.environ.copy()
        env.update(LD_PRELOAD=str(self.shim), PE_POSE_EXECUTABLE=str(self.host),
                   PE_POSE_MONO=str(self.fake), PE_POSE_ASSEMBLY='fake-probe.dll')
        env.update(override)
        return subprocess.run([str(self.host), str(self.fake)], env=env, capture_output=True,
                              text=True, timeout=10, check=True)

    def test_installs_once_main_thread_callback_only(self):
        result = self.invoke()
        self.assertEqual(result.stdout.strip(), '104')
        self.assertEqual(result.stderr.count('Probe installed'), 1)

    def test_wrong_executable_no_hook(self):
        result = self.invoke(PE_POSE_EXECUTABLE='/not-this-executable')
        self.assertEqual(result.stdout.strip(), '4')
        self.assertNotIn('[pose]', result.stderr)

    def test_wrong_mono_no_hook(self):
        self.assertEqual(self.invoke(PE_POSE_MONO='/not-this-mono').stdout.strip(), '4')

    def test_paths_profile_scoped_and_fresh(self):
        profile = self.root / 'profile'
        output, arm, stop, manifest = reference_pose.capture_paths(profile, profile / 'capture.jsonl')
        self.assertEqual(arm.suffix, '.arm')
        self.assertEqual(stop.suffix, '.stop')
        self.assertEqual(manifest.suffixes, ['.launch', '.json'])
        with self.assertRaises(ValueError):
            reference_pose.capture_paths(profile, self.root / 'outside.jsonl')
        profile.mkdir(exist_ok=True)
        arm.touch()
        try:
            with self.assertRaises(ValueError):
                reference_pose.capture_paths(profile, output)
        finally:
            arm.unlink()

    def test_whitespace_profile_rejected(self):
        with self.assertRaises(ValueError):
            reference_pose.capture_paths(self.root / 'bad profile', None)

    def test_default_outputs_unique_without_side_effects(self):
        profile = self.root / 'unused-profile'
        a = reference_pose.capture_paths(profile, None)
        b = reference_pose.capture_paths(profile, None)
        self.assertNotEqual(a, b)
        self.assertFalse(profile.exists())

if __name__ == '__main__':
    unittest.main()
