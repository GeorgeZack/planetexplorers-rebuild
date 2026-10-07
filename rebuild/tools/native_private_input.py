#!/usr/bin/env python3
"""Bounded ordinary native input on a newly owned Xvfb display; never the desktop."""
from __future__ import annotations
import argparse
from contextlib import contextmanager
import ctypes as C
from dataclasses import dataclass
import fcntl
import hashlib
import json
import math
import os
from pathlib import Path
import select
import signal
import subprocess
import sys
import tempfile
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
BINARY = ROOT / 'rebuild/build/planet_explorers'
XVFB = ROOT / 'rebuild/build/private-xvfb/usr/bin/Xvfb'
WIDTH, HEIGHT = 1280, 720
MAX_HOLD, MAX_LIFETIME = 5.0, 3600.0
INPUT_ACTIONS = frozenset(('key','click','button','burst','look'))
SETUP_LEASE_SECONDS, RELEASE_GRACE_SECONDS = 5.0, 1.0
MAX_LEASE_SECONDS = MAX_HOLD + .12 + RELEASE_GRACE_SECONDS
WATCHDOG_INTERVAL = .05
SERVER_TAIL = ['-screen', '0', '1280x720x24', '-nolisten', 'tcp', '-noreset', '-ac']
KEYS = set('abcdefghijklmnopqrstuvwxyz0123456789') | {
    'Escape', 'Return', 'space', 'Tab', 'BackSpace', 'Shift_L', 'Control_L', 'Alt_L',
    'Left', 'Right', 'Up', 'Down', 'Home', 'End', 'Page_Up', 'Page_Down',
} | {f'F{i}' for i in range(1, 13)} | {f'KP_{i}' for i in range(10)}

class ToolError(RuntimeError):
    pass

def require(condition, message):
    if not condition:
        raise ToolError(message)

def bounded_seconds(value, maximum=MAX_HOLD):
    try:
        value = float(value)
    except (TypeError, ValueError):
        raise ToolError('Duration must be a finite number') from None
    require(math.isfinite(value) and 0 < value <= maximum, f'Duration must be >0 and <={maximum} seconds')
    return value

def chord(value):
    parts = value.split('+')
    require(1 <= len(parts) <= 4 and len(set(parts)) == len(parts) and all(k in KEYS for k in parts), 'Use 1–4 distinct supported key names')
    require(not {'Control_L', 'Alt_L', 'BackSpace'}.issubset(parts), 'Server termination chords are unsupported')
    return parts

def screenshot_name(value):
    require(isinstance(value, str) and 4 < len(value) <= 120 and Path(value).name == value and
            value.endswith('.png') and not value.startswith('.') and all(c.isascii() and (c.isalnum() or c in '-_.') for c in value), 'Screenshot must be a new simple .png filename')
    return value

def digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()

def append_log(directory, record):
    record = {'wall_time': time.time(), 'monotonic': time.monotonic(), **record}
    with (directory / 'inputs.jsonl').open('a', encoding='utf8') as stream:
        stream.write(json.dumps(record, sort_keys=True) + '\n')

@dataclass(frozen=True)
class Process:
    start: str
    parent: int
    exe: str
    argv: tuple[str, ...]
    environment: dict[str, str]

def process(pid):
    require(type(pid) is int and pid > 1, 'Invalid process identity')
    p = Path('/proc') / str(pid)
    fields = (p / 'stat').read_text().rsplit(')', 1)[1].split()
    environ = {}
    for pair in (p / 'environ').read_bytes().split(b'\0'):
        if b'=' in pair:
            key, value = pair.split(b'=', 1)
            # Do not retain or log unrelated process environment/secrets.
            if key in (b'DISPLAY', b'WAYLAND_DISPLAY', b'PE_NATIVE_PRIVATE_SESSION'):
                environ[key.decode()] = value.decode(errors='replace')
    return Process(fields[19], int(fields[1]), os.readlink(p / 'exe'),
                   tuple(v.decode(errors='replace') for v in (p / 'cmdline').read_bytes().split(b'\0') if v), environ)

def validate_processes(session, directory, read_process=process, now=None):
    """Pure injectable provenance checks; tests supply synthetic /proc records."""
    require(isinstance(session,dict), 'Malformed session registry')
    require(session.get('schema') == 1 and session.get('directory') == str(directory), 'Session directory/schema mismatch')
    display = session.get('display', '')
    require(isinstance(display, str) and display.startswith(':') and display[1:].isdecimal() and len(display) <= 7, 'Not an explicit private X display')
    token = session.get('token', '')
    require(isinstance(token, str) and len(token) == 32 and all(c in '0123456789abcdef' for c in token), 'Invalid private session token')
    deadline = session.get('deadline')
    require(type(deadline) in (int, float) and math.isfinite(deadline), 'Invalid session deadline')
    require((time.monotonic() if now is None else now) < deadline, 'Private session lifetime has expired')
    current = {}
    expected = {'game': str(BINARY.resolve()), 'server': str(XVFB.resolve()), 'supervisor': str(Path(sys.executable).resolve())}
    for role, executable in expected.items():
        value = read_process(session.get(role))
        require(value.start == session.get(role + '_start') and value.exe == executable, f'{role} PID/start time/executable changed')
        current[role] = value
    require(current['game'].parent == session['supervisor'] and current['server'].parent == session['supervisor'], 'Processes are no longer children of their owning supervisor')
    command = current['server'].argv
    require(len(command) == 3 + len(SERVER_TAIL) and command[0] == str(XVFB) and command[1] == '-displayfd' and command[2].isdecimal() and list(command[3:]) == SERVER_TAIL, 'Server is not the dedicated displayfd Xvfb child')
    progress = str(directory / 'progress.pesave')
    require(current['game'].argv == (str(BINARY), '--sandbox', '--load', progress, '--save', progress), 'Game was not launched with the ordinary private progress copy')
    environment = current['game'].environment
    require(environment.get('DISPLAY') == display and 'WAYLAND_DISPLAY' not in environment and environment.get('PE_NATIVE_PRIVATE_SESSION') == token, 'Game is not bound to this owned private display')
    return current

def load_checked(directory):
    require(directory.is_dir() and not (directory / 'session.json').is_symlink(), 'Missing or redirected private session')
    session = json.loads((directory / 'session.json').read_text())
    validate_processes(session, directory)
    require(not (directory / 'baseline.pesave').is_symlink() and digest(directory / 'baseline.pesave') == session.get('baseline_sha256'), 'Untouched baseline hash changed')
    require(digest(BINARY) == session.get('binary_sha256'), 'Native executable changed since this session began')
    return session

@contextmanager
def command_lock(directory, timeout=0):
    require(directory.is_dir(), 'Session directory does not exist')
    with (directory / 'command.lock').open('a') as stream:
        until = time.monotonic() + timeout
        while True:
            try:
                fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
                break
            except BlockingIOError:
                require(time.monotonic() < until, 'Another private input command is active')
                time.sleep(.02)
        try:
            yield
        finally:
            fcntl.flock(stream, fcntl.LOCK_UN)

class XInput:
    """Only constructed after provenance checks; no default-display fallback."""
    def __init__(self, session):
        self.x = C.CDLL('libX11.so.6')
        self.t = C.CDLL('libXtst.so.6')
        D, U, I = C.c_void_p, C.c_ulong, C.c_int
        def bind(lib, name, args, result=I):
            fn = getattr(lib, name)
            fn.argtypes, fn.restype = args, result
        for name, args, result in [
            ('XOpenDisplay', [C.c_char_p], D), ('XDefaultRootWindow', [D], U),
            ('XQueryTree', [D,U,C.POINTER(U),C.POINTER(U),C.POINTER(C.POINTER(U)),C.POINTER(C.c_uint)], I),
            ('XFetchName', [D,U,C.POINTER(D)], I), ('XFree', [D], I),
            ('XSetInputFocus', [D,U,I,U], I), ('XFlush', [D], I), ('XCloseDisplay', [D], I),
            ('XStringToKeysym', [C.c_char_p], U), ('XKeysymToKeycode', [D,U], C.c_ubyte),
            ('XGetInputFocus', [D,C.POINTER(U),C.POINTER(I)], I),
            ('XInternAtom', [D,C.c_char_p,I], U),
            ('XGetWindowProperty', [D,U,U,C.c_long,C.c_long,I,U,C.POINTER(U),C.POINTER(I),C.POINTER(U),C.POINTER(U),C.POINTER(D)], I),
        ]:
            bind(self.x, name, args, result)
        bind(self.t, 'XTestFakeKeyEvent', [D,C.c_uint,I,U])
        bind(self.t, 'XTestFakeButtonEvent', [D,C.c_uint,I,U])
        bind(self.t, 'XTestFakeMotionEvent', [D,I,I,I,U])
        bind(self.t, 'XTestFakeRelativeMotionEvent', [D,I,I,U])
        require(session.get('display', '').startswith(':'), 'Explicit private display required')
        self.display = self.x.XOpenDisplay(session['display'].encode())
        require(self.display, 'Cannot open the owned private X display')
        try:
            root = self.x.XDefaultRootWindow(self.display)
            parent, root_return, children, count = U(), U(), C.POINTER(U)(), C.c_uint()
            require(self.x.XQueryTree(self.display,root,C.byref(root_return),C.byref(parent),C.byref(children),C.byref(count)), 'Cannot inspect private window tree')
            matches = []
            try:
                require(count.value <= 256, 'Unexpected private window count')
                for k in range(count.value):
                    window, name = children[k], D()
                    if not self.x.XFetchName(self.display,window,C.byref(name)) or not name:
                        continue
                    try:
                        title = C.string_at(name).decode(errors='replace')
                    finally:
                        self.x.XFree(name)
                    if title == 'Planet Explorers | C rebuild' and self.window_pid(window) == session['game']:
                        matches.append(window)
            finally:
                if children:
                    self.x.XFree(C.cast(children,D))
            require(len(matches) == 1, 'Expected one native game window with the owning PID; try again after loading')
            self.x.XSetInputFocus(self.display,matches[0],2,0)
            self.flush()
            focused, revert = U(), I()
            self.x.XGetInputFocus(self.display,C.byref(focused),C.byref(revert))
            require(focused.value == matches[0], 'Native private window did not acquire focus')
        except BaseException:
            self.close()
            raise
    def window_pid(self, window):
        U, I, D = C.c_ulong, C.c_int, C.c_void_p
        atom = self.x.XInternAtom(self.display,b'_NET_WM_PID',1)
        if not atom:
            return None
        kind, fmt, count, remaining, data = U(), I(), U(), U(), D()
        result = self.x.XGetWindowProperty(self.display,window,atom,0,1,0,6,C.byref(kind),C.byref(fmt),C.byref(count),C.byref(remaining),C.byref(data))
        try:
            return int(C.cast(data,C.POINTER(U))[0]) if result == 0 and kind.value == 6 and fmt.value == 32 and count.value == 1 and data else None
        finally:
            if data:
                self.x.XFree(data)
    def code(self, name):
        value = self.x.XKeysymToKeycode(self.display,self.x.XStringToKeysym(name.encode()))
        require(value, f'Key unavailable on private display: {name}')
        return value
    def key(self, code, down):
        require(self.t.XTestFakeKeyEvent(self.display,code,int(down),0), 'Private key injection failed')
    def button(self, button, down):
        require(self.t.XTestFakeButtonEvent(self.display,button,int(down),0), 'Private button injection failed')
    def motion(self, x, y):
        require(self.t.XTestFakeMotionEvent(self.display,-1,x,y,0), 'Private pointer motion failed')
    def relative(self, x, y):
        require(self.t.XTestFakeRelativeMotionEvent(self.display,x,y,0), 'Private relative motion failed')
    def flush(self):
        self.x.XFlush(self.display)
    def close(self):
        if self.display:
            self.x.XCloseDisplay(self.display)
            self.display = None

def perform_input(action, args, target, record, sleep=time.sleep, clock=time.monotonic):
    """All pressed controls are released in finally, including interrupted bursts."""
    pressed_keys, pressed_buttons = [], set()
    def key(code, down):
        target.key(code,down)
        record({'event':'key','code':code,'down':down})
    def button(number, down):
        if down:
            pressed_buttons.add(number)
        target.button(number,down)
        if not down:
            pressed_buttons.discard(number)
        record({'event':'button','button':number,'down':down})
    try:
        if action == 'key':
            codes = [target.code(k) for k in args.keys]
            for code in codes:
                pressed_keys.append(code)
                key(code,True)
            target.flush()
            sleep(args.seconds)
        elif action in ('click','button'):
            if action == 'click':
                target.motion(args.x,args.y)
                record({'event':'absolute-motion','x':args.x,'y':args.y})
                target.flush()
                sleep(.12)
            button(args.button,True)
            target.flush()
            sleep(args.seconds)
        elif action == 'burst':
            until = clock() + args.seconds
            while clock() < until:
                button(args.button,True)
                target.flush()
                sleep(min(.1,max(0,until-clock())))
                button(args.button,False)
                target.flush()
                sleep(min(.1,max(0,until-clock())))
        elif action == 'look':
            steps = max(1,max(abs(args.dx),abs(args.dy))//10)
            for step in range(steps):
                dx = round((step+1)*args.dx/steps)-round(step*args.dx/steps)
                dy = round((step+1)*args.dy/steps)-round(step*args.dy/steps)
                target.relative(dx,dy)
                record({'event':'relative-motion','dx':dx,'dy':dy})
                target.flush()
                sleep(.02)
        else:
            raise ToolError('Not an input action')
    finally:
        # Attempt every release even if another release or event-log write fails.
        failures = []
        for code in reversed(pressed_keys):
            try:
                key(code,False)
            except Exception as error:
                failures.append(str(error))
        for number in sorted(pressed_buttons):
            try:
                button(number,False)
            except Exception as error:
                failures.append(str(error))
        target.flush()
        if failures:
            raise ToolError('Control release failed: ' + '; '.join(failures))

def set_parent_death(signum, expected_parent):
    """Linux prctl, with the fork/registration race closed before proceeding."""
    libc = C.CDLL(None,use_errno=True)
    prctl = libc.prctl
    prctl.argtypes = [C.c_int,C.c_ulong,C.c_ulong,C.c_ulong,C.c_ulong]
    prctl.restype = C.c_int
    if prctl(1,signum,0,0,0) != 0:  # PR_SET_PDEATHSIG
        raise OSError(C.get_errno(),'Cannot set Linux parent-death signal')
    if os.getppid() != expected_parent:
        # Parent already died before registration; do not launch an orphan.
        os.kill(os.getpid(),signum)
        raise ToolError('Launching parent changed during parent-death registration')

def child_parent_death(parent):
    # The CLI is single-threaded. This hook runs after fork and before exec.
    # SIGKILL also works if the supervisor suffers an uncatchable termination.
    return lambda:set_parent_death(signal.SIGKILL,parent)

def install_signal_handlers():
    for signum in (signal.SIGTERM,signal.SIGINT,signal.SIGHUP):
        signal.signal(signum,interrupted)

def input_seconds(action,args):
    if action == 'look':
        return max(1,max(abs(args.dx),abs(args.dy))//10)*.02
    return args.seconds + (.12 if action == 'click' else 0)

class InputLease:
    """Persistent ownership survives command SIGKILL/Xlib fatal exit."""
    def __init__(self,directory,session):
        self.directory = directory
        self.path = directory/'input-lease.json'
        identity = process(os.getpid())
        self.value = {'schema':1,'token':session['token'],'nonce':uuid.uuid4().hex,
                      'pid':os.getpid(),'start':identity.start,'exe':identity.exe}
    def write(self,phase,seconds,create=False):
        now = time.monotonic()
        self.value.update(phase=phase,issued=now,deadline=now+seconds)
        fd,name = tempfile.mkstemp(prefix='.input-lease-',suffix='.tmp',dir=self.directory)
        temporary = Path(name)
        try:
            with os.fdopen(fd,'w') as stream:
                json.dump(self.value,stream,sort_keys=True)
                stream.write('\n')
            if create:
                # Atomic no-replace publication: an abandoned prior command
                # must never be hidden by a newly arriving operator command.
                try:
                    os.link(temporary,self.path)
                except FileExistsError:
                    raise ToolError('Existing input lease; wait for supervisor teardown') from None
            else:
                require(not self.path.is_symlink() and json.loads(self.path.read_text()).get('nonce') == self.value['nonce'], 'Input lease ownership changed')
                os.replace(temporary,self.path)
        finally:
            temporary.unlink(missing_ok=True)
    def __enter__(self):
        self.write('setup',SETUP_LEASE_SECONDS,create=True)
        return self
    def arm(self,action,args):
        require(action in INPUT_ACTIONS,'Unsupported lease action')
        self.value['action'] = action
        self.write('input',input_seconds(action,args)+RELEASE_GRACE_SECONDS)
    def __exit__(self,kind,value,traceback):
        if kind is None:
            require(not self.path.is_symlink() and json.loads(self.path.read_text()).get('nonce') == self.value['nonce'], 'Input lease ownership changed before release')
            self.path.unlink()
        # Failed/abrupt commands leave a lease. The supervisor aborts the
        # private session rather than allowing unknown held input to persist.
        return False

def lease_problem(directory,session,read_process=process,now=None,clock=time.monotonic):
    path = directory/'input-lease.json'
    try:
        if path.is_symlink():
            return 'redirected input lease'
        snapshot = path.read_text()
    except FileNotFoundError:
        return None
    except OSError:
        return 'unreadable input lease'
    try:
        value = json.loads(snapshot)
        require(isinstance(value,dict) and value.get('schema') == 1 and value.get('token') == session['token'],'input lease session mismatch')
        require(value.get('phase') in ('setup','input'),'invalid input lease phase')
        require(value['phase'] != 'input' or value.get('action') in INPUT_ACTIONS,'invalid input lease action')
        issued,deadline = value.get('issued'),value.get('deadline')
        require(type(issued) in (int,float) and type(deadline) in (int,float) and math.isfinite(issued) and math.isfinite(deadline),'invalid input lease clock')
        # Sample after the atomic read: a publication/arm may occur between
        # the supervisor loop clock sample and this read.
        current = clock() if now is None else now
        limit = SETUP_LEASE_SECONDS if value['phase'] == 'setup' else MAX_LEASE_SECONDS
        require(issued<=current and 0<deadline-issued<=limit+.000001,'unbounded input lease')
        require(current<deadline,'input lease expired')
        require(type(value.get('pid')) is int and value['pid']>1 and value['pid'] not in (session['server'],session['game'],session['supervisor']),'invalid input command PID')
        require(value.get('exe') == str(Path(sys.executable).resolve()),'input command executable mismatch')
        owner = read_process(value['pid'])
        require(owner.start == value.get('start') and owner.exe == value['exe'],'input command identity changed')
    except (ToolError,OSError,KeyError,IndexError,ValueError) as error:
        # Successful input may remove its lease and exit while /proc is being
        # inspected. An arm/new command may also replace the snapshot. Only
        # report a fault for the still-current complete atomic lease; otherwise
        # let the next bounded poll inspect the new state.
        try:
            if not path.is_symlink() and path.read_text() != snapshot:
                return None
        except FileNotFoundError:
            return None
        except OSError:
            return 'unreadable input lease'
        return 'unreadable input lease' if isinstance(error,json.JSONDecodeError) else str(error)
    return None

def supervise(game,server,directory,session,clock=time.monotonic,sleep=time.sleep,read_process=process):
    while True:
        result = game.poll()
        if result is not None:
            return result
        if server.poll() is not None:
            append_log(directory,{'action':'private-server-exited','server_returncode':server.returncode})
            return 125
        now = clock()
        if now>=session['deadline']:
            append_log(directory,{'action':'lifetime-expired'})
            return 124
        problem = lease_problem(directory,session,read_process=read_process,clock=clock)
        if problem:
            append_log(directory,{'action':'input-watchdog-abort','reason':problem})
            return 125
        sleep(min(WATCHDOG_INTERVAL,session['deadline']-now))

def end_owned_children(*children):
    # A hung input owner must not receive ten more seconds of game updates
    # while a graceful child timeout runs. This is session abort/teardown.
    for child in children:
        if child and child.poll() is None:
            child.kill()
    for child in children:
        if child:
            child.wait(timeout=5)


def start(directory, baseline, lifetime):
    require(BINARY.is_file() and os.access(BINARY,os.X_OK), 'Build the native game first')
    require(XVFB.is_file() and os.access(XVFB,os.X_OK), 'Private Xvfb dependency is missing; no desktop fallback exists')
    require(baseline.is_file() and 16 <= baseline.stat().st_size <= 256*1024*1024, 'Baseline must be a native save between16 bytes and256MiB')
    data = baseline.read_bytes()
    require(data[:4] == b'PECS', 'Expected an existing native PECS save, not a fixture generator or Unity archive')
    directory.mkdir(mode=0o700,parents=True,exist_ok=False)
    (directory/'baseline.pesave').write_bytes(data)
    (directory/'baseline.pesave').chmod(0o444)
    (directory/'progress.pesave').write_bytes(data)
    (directory/'command.lock').touch()
    baseline_hash = hashlib.sha256(data).hexdigest()
    server = game = None
    read_fd = write_fd = None
    session = None
    try:
        with (directory/'xvfb.log').open('w') as server_log, (directory/'native.log').open('w') as game_log:
            read_fd, write_fd = os.pipe()
            server = subprocess.Popen([str(XVFB),'-displayfd',str(write_fd),*SERVER_TAIL],pass_fds=(write_fd,),stdout=server_log,stderr=subprocess.STDOUT,start_new_session=True,preexec_fn=child_parent_death(os.getpid()))
            os.close(write_fd)
            write_fd = None
            require(select.select([read_fd],[],[],15)[0], 'Private Xvfb startup timed out')
            number = os.read(read_fd,128).decode().strip()
            require(number.isdecimal() and len(number)<=6, 'Private Xvfb did not provide a display number')
            # This read end MUST remain open until the game and server exit.
            # Xvfb may write its displayfd again after a client disconnects.
            token = uuid.uuid4().hex
            env = os.environ.copy()
            env.pop('WAYLAND_DISPLAY',None)
            env.update(DISPLAY=':'+number,PE_NATIVE_PRIVATE_SESSION=token,LIBGL_ALWAYS_SOFTWARE='1',GALLIUM_DRIVER='llvmpipe',PE_SANDBOX_COMBAT_TRACE='1',TMPDIR=str(ROOT/'rebuild/build/compiler-tmp'))
            (ROOT/'rebuild/build/compiler-tmp').mkdir(exist_ok=True)
            progress = str(directory/'progress.pesave')
            game = subprocess.Popen([str(BINARY),'--sandbox','--load',progress,'--save',progress],cwd=ROOT,env=env,stdout=game_log,stderr=subprocess.STDOUT,start_new_session=True,preexec_fn=child_parent_death(os.getpid()))
            session = {'schema':1,'directory':str(directory),'display':':'+number,'token':token,'deadline':time.monotonic()+lifetime,'lifetime':lifetime,'source':str(baseline),'baseline_sha256':baseline_hash,'binary_sha256':digest(BINARY)}
            for role, pid in [('supervisor',os.getpid()),('server',server.pid),('game',game.pid)]:
                session[role], session[role+'_start'] = pid, process(pid).start
            (directory/'session.json').write_text(json.dumps(session,indent=2)+'\n')
            append_log(directory,{'action':'start',**session})
            print(json.dumps(session),flush=True)
            return supervise(game,server,directory,session)
    finally:
        # Do not wait for an input lock: its owner may be hung or gone. Stop
        # both owned children before waiting, clearing the private X state.
        # This is abort/teardown, not an invented game save or successful play.
        try:
            end_owned_children(game,server)
        finally:
            if write_fd is not None:
                os.close(write_fd)
            if read_fd is not None:
                os.close(read_fd)
            append_log(directory,{'action':'supervisor-exit','game_returncode':game.returncode if game else None,'baseline_unchanged':digest(directory/'baseline.pesave')==baseline_hash})

def stop_owned(session, directory):
    require(hasattr(os,'pidfd_open') and hasattr(signal,'pidfd_send_signal'), 'Linux pidfd signaling is required; no PID-only kill fallback')
    fd = os.pidfd_open(session['game'])
    try:
        validate_processes(session,directory)
        signal.pidfd_send_signal(fd,signal.SIGTERM)
    finally:
        os.close(fd)
    append_log(directory,{'action':'stop-requested'})

def parser():
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest='action',required=True)
    for action in ('start','status','stop','key','click','button','burst','look','shot'):
        q = sub.add_parser(action)
        q.add_argument('directory',type=Path)
        if action == 'start':
            q.add_argument('baseline',type=Path)
            q.add_argument('--lifetime',type=float,default=1800)
        elif action == 'key':
            q.add_argument('keys')
            q.add_argument('seconds',type=float,nargs='?',default=.15)
        elif action in ('click','button'):
            if action == 'click':
                q.add_argument('x',type=int)
                q.add_argument('y',type=int)
            q.add_argument('button',type=int,nargs='?',default=1)
            q.add_argument('seconds',type=float,nargs='?',default=.12)
        elif action == 'burst':
            q.add_argument('seconds',type=float)
            q.add_argument('button',type=int,nargs='?',default=1)
        elif action == 'look':
            q.add_argument('dx',type=int)
            q.add_argument('dy',type=int)
        elif action == 'shot':
            q.add_argument('filename')
    return p

def validate_arguments(args):
    args.directory = args.directory.resolve()
    if args.action == 'start':
        args.baseline = args.baseline.resolve()
        args.lifetime = bounded_seconds(args.lifetime,MAX_LIFETIME)
    if args.action in ('key','click','button','burst'):
        args.seconds = bounded_seconds(args.seconds)
    if args.action == 'key':
        args.keys = chord(args.keys)
    if args.action in ('click','button','burst'):
        require(args.button in (1,3), 'Only left1/right3 buttons are supported')
    if args.action == 'click':
        require(0<=args.x<WIDTH and 0<=args.y<HEIGHT, 'UI click is outside the private1280x720 display')
    if args.action == 'look':
        require(abs(args.dx)<=1000 and abs(args.dy)<=1000, 'Relative movement is bounded to1000 pixels per axis')
    if args.action == 'shot':
        args.filename = screenshot_name(args.filename)
    return args

def main(argv=None):
    args = validate_arguments(parser().parse_args(argv))
    directory = args.directory
    if args.action == 'start':
        return start(directory,args.baseline,args.lifetime)
    with command_lock(directory):
        session = load_checked(directory)
        append_log(directory,{'action':args.action,'phase':'begin','arguments':{k:str(v) if isinstance(v,Path) else v for k,v in vars(args).items() if k not in ('directory','action')}})
        try:
            if args.action == 'status':
                print(json.dumps(session,indent=2))
            elif args.action == 'stop':
                stop_owned(session,directory)
            elif args.action == 'shot':
                image = directory/args.filename
                require(not image.exists() and not image.is_symlink(), 'Screenshot already exists; choose a new filename')
                env = os.environ.copy()
                env.pop('WAYLAND_DISPLAY',None)
                env['DISPLAY'] = session['display']
                validate_processes(session,directory)
                subprocess.run(['ffmpeg','-nostdin','-loglevel','error','-f','x11grab','-video_size','1280x720','-i',session['display'],'-frames:v','1','-n',str(image)],env=env,check=True,timeout=min(20,max(.1,session['deadline']-time.monotonic())))
                print(image)
            else:
                require(session['deadline']-time.monotonic()>SETUP_LEASE_SECONDS+MAX_LEASE_SECONDS, 'Too close to session deadline to start input')
                with InputLease(directory,session) as lease:
                    target = XInput(session)
                    try:
                        validate_processes(session,directory)
                        lease.arm(args.action,args)
                        perform_input(args.action,args,target,lambda record:append_log(directory,record))
                    finally:
                        target.close()
            append_log(directory,{'action':args.action,'phase':'complete'})
            return 0
        except BaseException as error:
            append_log(directory,{'action':args.action,'phase':'failed','error':str(error)})
            raise

def interrupted(signum, frame):
    raise ToolError(f'Interrupted by signal{signum}; releasing owned input/processes')

if __name__ == '__main__':
    install_signal_handlers()
    try:
        set_parent_death(signal.SIGHUP,os.getppid())
        sys.exit(main())
    except (ToolError,OSError,ValueError,KeyError,IndexError,subprocess.SubprocessError) as error:
        print(f'native-private-input: {error}',file=sys.stderr)
        sys.exit(1)
