"""Pure argument/provenance/mocked-input tests. Never open any X display."""
from dataclasses import replace
import importlib.util
import io
import json
import signal
import subprocess
import os
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('native_private_input', ROOT/'rebuild/tools/native_private_input.py')
m = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = m
SPEC.loader.exec_module(m)

class FakeInput:
    def __init__(self):
        self.events = []
    def code(self, name):
        return {'w':25,'Shift_L':50}[name]
    def key(self, code, down):
        self.events.append(('key',code,down))
    def button(self, number, down):
        self.events.append(('button',number,down))
    def motion(self, x, y):
        self.events.append(('absolute',x,y))
    def relative(self, x, y):
        self.events.append(('relative',x,y))
    def flush(self):
        pass

class PrivateInputTests(unittest.TestCase):
    def arguments(self, *values):
        return m.validate_arguments(m.parser().parse_args(values))
    def test_bounds_and_keys(self):
        self.assertEqual(self.arguments('key','unused','Shift_L+w','5').keys,['Shift_L','w'])
        for values in [('key','unused','w','nan'),('key','unused','w','inf'),('key','unused','w','0'),('button','unused','2'),('click','unused','1280','5'),('click','unused','0','-1'),('look','unused','1001','0'),('key','unused','Super_L'),('key','unused','w+w'),('key','unused','Control_L+Alt_L+BackSpace'),('start','unused','baseline','--lifetime','3601')]:
            with self.subTest(values=values), self.assertRaises(m.ToolError):
                self.arguments(*values)
        self.assertEqual(self.arguments('button','unused').seconds,.12)
        self.assertEqual(self.arguments('burst','unused','5','3').button,3)
    def test_screenshot_filename(self):
        self.assertEqual(m.screenshot_name('hunt-001.png'),'hunt-001.png')
        for name in ('../live.png','/tmp/a.png','.png','.hidden.png','file.jpg','nested/name.png','bad name.png','x'*121+'.png'):
            with self.subTest(name=name), self.assertRaises(m.ToolError):
                m.screenshot_name(name)
    def state(self):
        directory=Path('/isolated/new-session')
        token='a'*32
        session={'schema':1,'directory':str(directory),'display':':91','token':token,'deadline':1000,
                 'supervisor':101,'supervisor_start':'111','server':102,'server_start':'222','game':103,'game_start':'333'}
        progress=str(directory/'progress.pesave')
        records={
            101:m.Process('111',1,str(Path(sys.executable).resolve()),('python','helper'),{}),
            102:m.Process('222',101,str(m.XVFB.resolve()),(str(m.XVFB),'-displayfd','7',*m.SERVER_TAIL),{}),
            103:m.Process('333',101,str(m.BINARY.resolve()),(str(m.BINARY),'--sandbox','--load',progress,'--save',progress),{'DISPLAY':':91','PE_NATIVE_PRIVATE_SESSION':token}),
        }
        return directory,session,records
    def test_process_guard_valid_owned_chain(self):
        directory,session,records=self.state()
        self.assertEqual(m.validate_processes(session,directory,records.__getitem__,now=500)['game'],records[103])
    def test_pid_reuse_binary_parent_display_and_command_guards(self):
        directory,session,records=self.state()
        changes=[(103,{'start':'444'}),(102,{'exe':'/usr/bin/Xwayland'}),(103,{'parent':999}),
                 (103,{'environment':{'DISPLAY':':0','PE_NATIVE_PRIVATE_SESSION':'a'*32}}),
                 (103,{'environment':{'DISPLAY':':91','WAYLAND_DISPLAY':'wayland-1','PE_NATIVE_PRIVATE_SESSION':'a'*32}}),
                 (103,{'environment':{'DISPLAY':':91','WAYLAND_DISPLAY':'','PE_NATIVE_PRIVATE_SESSION':'a'*32}}),
                 (103,{'environment':{'DISPLAY':':91'}}),
                 (103,{'argv':(*records[103].argv,'--smoke','100')}),
                 (102,{'argv':('/usr/bin/Xvfb',':0')}),(101,{'start':'reused'})]
        for pid,change in changes:
            with self.subTest(pid=pid,change=change), self.assertRaises(m.ToolError):
                bad=dict(records);bad[pid]=replace(bad[pid],**change)
                m.validate_processes(session,directory,bad.__getitem__,now=500)
        for changes in ({'display':''},{'display':'localhost:0'},{'deadline':499},{'deadline':float('nan')},{'directory':'/other'},{'schema':2}):
            with self.subTest(changes=changes),self.assertRaises(m.ToolError):
                m.validate_processes({**session,**changes},directory,records.__getitem__,now=500)
    def test_button_does_not_warp_and_releases_on_interruption(self):
        target=FakeInput();args=self.arguments('button','unused','1','.3')
        def interrupt(seconds):
            raise KeyboardInterrupt()
        with self.assertRaises(KeyboardInterrupt):
            m.perform_input('button',args,target,lambda e:None,sleep=interrupt)
        self.assertEqual(target.events,[('button',1,True),('button',1,False)])
    def test_keys_release_reverse_order_even_when_logging_fails(self):
        target=FakeInput();args=self.arguments('key','unused','Shift_L+w','.3')
        def record(event):
            if event['code']==25 and event['down']:
                raise OSError('simulated journal failure')
        with self.assertRaises(OSError):
            m.perform_input('key',args,target,record,sleep=lambda s:None)
        self.assertEqual(target.events,[('key',50,True),('key',25,True),('key',25,False),('key',50,False)])
    def test_click_only_absolute_action_and_relative_total(self):
        target=FakeInput();args=self.arguments('click','unused','100','200','3','.1')
        m.perform_input('click',args,target,lambda e:None,sleep=lambda s:None)
        self.assertEqual(target.events,[('absolute',100,200),('button',3,True),('button',3,False)])
        target=FakeInput();args=self.arguments('look','unused','-103','77')
        m.perform_input('look',args,target,lambda e:None,sleep=lambda s:None)
        self.assertTrue(all(e[0]=='relative' for e in target.events))
        self.assertEqual((sum(e[1] for e in target.events),sum(e[2] for e in target.events)),(-103,77))
    def test_burst_bounded_and_release_on_interrupt(self):
        target=FakeInput();args=self.arguments('burst','unused','.35')
        now=[0.0]
        def sleep(seconds):now[0]+=seconds
        m.perform_input('burst',args,target,lambda e:None,sleep=sleep,clock=lambda:now[0])
        self.assertLessEqual(now[0],.35)
        self.assertEqual(target.events,[('button',1,True),('button',1,False),('button',1,True),('button',1,False)])
        self.assertFalse(any(e[0]=='absolute' for e in target.events))
    def test_unique_directory_baseline_and_displayfd_lifetime(self):
        with tempfile.TemporaryDirectory() as name:
            base=Path(name);binary=base/'game';xvfb=base/'Xvfb';source=base/'source.pesave';out=base/'new-session'
            for path in (binary,xvfb):path.write_bytes(b'executable');path.chmod(0o700)
            source.write_bytes(b'PECS'+b'\0'*28);original=source.read_bytes();fds=[];real_pipe=os.pipe
            def pipe():
                pair=real_pipe();fds.extend(pair);return pair
            class Child:
                def __init__(self,pid,kind):self.pid=pid;self.kind=kind;self.returncode=None
                def poll(self):
                    if self.kind=='game':os.fstat(fds[0]);self.returncode=0
                    return self.returncode
                def terminate(self):self.returncode=-15
                def kill(self):self.returncode=-9
                def wait(self,timeout=None):
                    # Both children must exit while the displayfd read end is live.
                    os.fstat(fds[0]);self.returncode=0 if self.returncode is None else self.returncode;return self.returncode
            server,game=Child(102,'server'),Child(103,'game')
            def popen(argv,**kwargs):
                if argv[0]==str(xvfb):
                    os.write(kwargs['pass_fds'][0],b'91\n');return server
                self.assertEqual(argv,[str(binary),'--sandbox','--load',str(out/'progress.pesave'),'--save',str(out/'progress.pesave')])
                self.assertEqual(kwargs['env']['DISPLAY'],':91');self.assertNotIn('WAYLAND_DISPLAY',kwargs['env']);return game
            proc=lambda pid:SimpleNamespace(start=str(pid))
            with patch.object(m,'BINARY',binary),patch.object(m,'XVFB',xvfb),patch.object(m.os,'pipe',pipe),patch.object(m.subprocess,'Popen',popen),patch.object(m,'process',proc),patch('sys.stdout',new=io.StringIO()):
                self.assertEqual(m.start(out,source,10),0)
                with self.assertRaises(FileExistsError):m.start(out,source,10)
            self.assertEqual(source.read_bytes(),original)
            self.assertEqual((out/'baseline.pesave').read_bytes(),original)
            self.assertEqual((out/'progress.pesave').read_bytes(),original)
            self.assertEqual((out/'baseline.pesave').stat().st_mode&0o777,0o444)
            self.assertEqual(json.loads((out/'session.json').read_text())['baseline_sha256'],m.digest(source))
            with self.assertRaises(OSError):os.fstat(fds[0])
            self.assertTrue(json.loads((out/'inputs.jsonl').read_text().splitlines()[-1])['baseline_unchanged'])
    def test_load_rejects_changed_baseline_without_connecting(self):
        with tempfile.TemporaryDirectory() as name:
            directory=Path(name);baseline=directory/'baseline.pesave';baseline.write_bytes(b'changed')
            (directory/'session.json').write_text(json.dumps({'baseline_sha256':'0'*64}))
            with patch.object(m,'validate_processes'),patch.object(m,'XInput',side_effect=AssertionError('must never connect')):
                with self.assertRaises(m.ToolError):m.load_checked(directory)

class LifecycleTests(unittest.TestCase):
    def session(self):
        return {'token':'b'*32,'server':102,'game':103,'supervisor':101,'deadline':100}
    def owner(self):
        return m.Process('444',1,str(Path(sys.executable).resolve()),('python','input'),{})
    def record(self,**changes):
        return {'schema':1,'token':'b'*32,'nonce':'c'*32,'pid':444,'start':'444',
                'exe':str(Path(sys.executable).resolve()),'phase':'input','action':'key',
                'issued':10.,'deadline':15.,**changes}
    def test_hup_handlers_installed(self):
        with patch.object(m.signal,'signal') as registration:
            m.install_signal_handlers()
        self.assertEqual([call.args[0] for call in registration.call_args_list],[signal.SIGTERM,signal.SIGINT,signal.SIGHUP])
        with self.assertRaises(m.ToolError):m.interrupted(signal.SIGHUP,None)
    def test_hup_releases_pressed_controls_and_preserves_abort_lease(self):
        with tempfile.TemporaryDirectory() as name,patch.object(m,'process',return_value=self.owner()):
            directory=Path(name);target=FakeInput()
            args=SimpleNamespace(keys=['Shift_L','w'],seconds=.2)
            def hangup(seconds):m.interrupted(signal.SIGHUP,None)
            with self.assertRaises(m.ToolError):
                with m.InputLease(directory,self.session()) as lease:
                    lease.arm('key',args)
                    m.perform_input('key',args,target,lambda event:None,sleep=hangup)
            self.assertEqual(target.events,[('key',50,True),('key',25,True),('key',25,False),('key',50,False)])
            self.assertTrue((directory/'input-lease.json').is_file())
    def test_lease_alive_missing_expired_reused_and_malformed(self):
        with tempfile.TemporaryDirectory() as name:
            directory=Path(name);path=directory/'input-lease.json';session=self.session()
            self.assertIsNone(m.lease_problem(directory,session,now=12))
            path.write_text(json.dumps(self.record()))
            self.assertIsNone(m.lease_problem(directory,session,lambda pid:self.owner(),now=12))
            self.assertIn('expired',m.lease_problem(directory,session,lambda pid:self.owner(),now=15))
            self.assertIn('identity changed',m.lease_problem(directory,session,lambda pid:replace(self.owner(),start='reused'),now=12))
            def missing(pid):raise ProcessLookupError('owner gone')
            self.assertIn('owner gone',m.lease_problem(directory,session,missing,now=12))
            for changes in ({'deadline':20.},{'phase':'other'},{'token':'wrong'},{'pid':103},{'issued':float('nan')},{'action':'shot'}):
                path.write_text(json.dumps(self.record(**changes)))
                self.assertIsNotNone(m.lease_problem(directory,session,lambda pid:self.owner(),now=12))
            path.write_text('{broken')
            self.assertIn('unreadable',m.lease_problem(directory,session,now=12))
    def test_lease_clock_is_sampled_after_publication(self):
        with tempfile.TemporaryDirectory() as name:
            directory=Path(name);path=directory/'input-lease.json'
            path.write_text(json.dumps(self.record(issued=12.01,deadline=13.01)))
            events=[];original=Path.read_text
            def read(target,*args,**kwargs):
                value=original(target,*args,**kwargs)
                if target==path:events.append('read')
                return value
            def clock():
                events.append('clock');return 12.02
            with patch.object(Path,'read_text',read):
                self.assertIsNone(m.lease_problem(directory,self.session(),lambda pid:self.owner(),clock=clock))
            self.assertEqual(events,['read','clock'])
    def test_successful_removal_or_replacement_during_owner_probe_is_not_fault(self):
        with tempfile.TemporaryDirectory() as name:
            directory=Path(name);path=directory/'input-lease.json'
            for replacement in (None,self.record(nonce='d'*32),self.record(phase='setup',action=None,issued=12.,deadline=17.)):
                path.write_text(json.dumps(self.record()))
                def departed(pid):
                    if replacement is None:path.unlink()
                    else:path.write_text(json.dumps(replacement))
                    raise ProcessLookupError('old command completed')
                self.assertIsNone(m.lease_problem(directory,self.session(),departed,now=12))
            # A fault for the unchanged lease remains actionable.
            path.write_text(json.dumps(self.record()))
            def dead(pid):raise ProcessLookupError('abandoned command')
            self.assertIn('abandoned command',m.lease_problem(directory,self.session(),dead,now=12))
    def test_success_clears_lease_and_failure_prevents_overwrite(self):
        with tempfile.TemporaryDirectory() as name, patch.object(m,'process',return_value=self.owner()):
            directory=Path(name);session=self.session()
            args=SimpleNamespace(seconds=.2)
            with m.InputLease(directory,session) as lease:
                lease.arm('key',args)
                self.assertEqual(json.loads((directory/'input-lease.json').read_text())['action'],'key')
            self.assertFalse((directory/'input-lease.json').exists())
            with self.assertRaises(KeyboardInterrupt):
                with m.InputLease(directory,session):raise KeyboardInterrupt()
            abandoned=(directory/'input-lease.json').read_bytes()
            with self.assertRaises(m.ToolError):
                with m.InputLease(directory,session):pass
            self.assertEqual((directory/'input-lease.json').read_bytes(),abandoned)
    def test_watchdog_does_not_wait_for_game_or_input_lock(self):
        class Child:
            returncode=None
            def poll(self):return None
            def wait(self,**kwargs):raise AssertionError('watchdog must poll, not block')
        with tempfile.TemporaryDirectory() as name:
            directory=Path(name);(directory/'input-lease.json').write_text(json.dumps(self.record()))
            now=[12.]
            def gone(pid):raise ProcessLookupError('command abruptly exited')
            with m.command_lock(directory):
                self.assertEqual(m.supervise(Child(),Child(),directory,self.session(),clock=lambda:now[0],sleep=lambda seconds:None,read_process=gone),125)
            events=[json.loads(line) for line in (directory/'inputs.jsonl').read_text().splitlines()]
            self.assertEqual(events[-1]['action'],'input-watchdog-abort')
            self.assertIn('abruptly exited',events[-1]['reason'])
            (directory/'input-lease.json').unlink()
            now[0]=99.98
            self.assertEqual(m.supervise(Child(),Child(),directory,self.session(),clock=lambda:now[0],sleep=lambda seconds:now.__setitem__(0,now[0]+seconds)),124)
            self.assertGreaterEqual(now[0],100)
    def test_teardown_signals_both_children_before_waiting(self):
        events=[]
        class Child:
            def __init__(self,name):self.name=name
            def poll(self):return None
            def kill(self):events.append(('kill',self.name))
            def wait(self,timeout):events.append(('wait',self.name))
        m.end_owned_children(Child('game'),Child('server'))
        self.assertEqual(events,[('kill','game'),('kill','server'),('wait','game'),('wait','server')])
    def test_linux_parent_death_with_short_non_graphical_subprocesses(self):
        # A separate wrapper owns/reaps every probe. No X display/game process
        # is launched, and subreaper state is confined to that wrapper process.
        script=r"""
import ctypes, importlib.util, os, signal, subprocess, sys
from pathlib import Path
path=Path(sys.argv[1])
spec=importlib.util.spec_from_file_location('private_probe',path)
m=importlib.util.module_from_spec(spec);sys.modules[spec.name]=m;spec.loader.exec_module(m)
libc=ctypes.CDLL(None,use_errno=True)
if libc.prctl(36,1,0,0,0):raise OSError(ctypes.get_errno(),'subreaper')
supervisor=r'''import importlib.util,os,subprocess,sys,time
p=sys.argv[1];s=importlib.util.spec_from_file_location('probe',p);m=importlib.util.module_from_spec(s);sys.modules[s.name]=m;s.loader.exec_module(m)
child=subprocess.Popen([sys.executable,'-c','import time;time.sleep(30)'],start_new_session=True,preexec_fn=m.child_parent_death(os.getpid()))
print(child.pid,flush=True)
time.sleep(30)
'''
parent=subprocess.Popen([sys.executable,'-c',supervisor,str(path)],stdout=subprocess.PIPE,text=True,preexec_fn=m.child_parent_death(os.getpid()))
pid=None
try:
 pid=int(parent.stdout.readline())
 parent.kill();parent.wait(timeout=3)
 got,status=os.waitpid(pid,0)
 expected=pid;pid=None
 if got!=expected or not os.WIFSIGNALED(status) or os.WTERMSIG(status)!=signal.SIGKILL:raise RuntimeError('parent-death did not kill owned child')
finally:
 if parent.poll() is None:parent.kill();parent.wait()
 if pid:
  try:os.kill(pid,signal.SIGKILL)
  except ProcessLookupError:pass
  try:os.waitpid(pid,os.WNOHANG)
  except ChildProcessError:pass
print('parent-death probe passed')
"""
        result=subprocess.run([sys.executable,'-c',script,str(ROOT/'rebuild/tools/native_private_input.py')],capture_output=True,text=True,timeout=10)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn('parent-death probe passed',result.stdout)

if __name__=='__main__':unittest.main()
