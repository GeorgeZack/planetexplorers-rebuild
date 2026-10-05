#!/usr/bin/env python3
"""Bounded input/capture for the original Linux Planet Explorers reference only.

Requires Hyprland's Lua `hyprctl eval` interface, Xwayland DISPLAY, libX11 and
libXtst for key/look, grim for screenshots, and Wayland virtual-pointer support
for clicks and drags. First pointer action compiles the bundled helper/protocol using cc,
wayland-scanner and pkg-config wayland-client into ignored rebuild/build.
Nothing is downloaded and no desktop configuration is changed.

Commands: layout [--width 1280 --height 720], shot OUTPUT.png,
click X Y [--button left|right|middle], drag X1 Y1 X2 Y2 [--button left],
key Shift_L+w [--duration 0.18],
look DX DY. Coordinates are client-local logical pixels; screenshots use the
current client rectangle. Only the unique exact-title `Planet Explorers`
window whose /proc/PID/exe basename is PE_Client.x86_64 is eligible; --pid
disambiguates multiple original clients. Every action focuses this window.

Keyboard uses fixed X11 evdev/QWERTY keycodes, avoiding wtype's transient
keymap remapping that made F5 act as Escape in the legacy Unity player.
Keys release in finally; holds are bounded to 5 seconds. Look motion uses
20-pixel increments with 30ms pauses (maximum 2000 pixels per axis). Clicks
first move four pixels within the client then return, refreshing Unity hover
state for repeated clicks. Drags pause 150ms after pressing, then use ten interpolated steps at 50ms intervals. Click holds are 150ms (below Unity's 250ms limit). Do not run while another
operator controls the desktop. This is a reference-test tool, not gameplay
input integration for the C rebuild.
"""
import argparse
from contextlib import contextmanager
import ctypes
import json
import math
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent
BUILD = HERE.parent / 'build/reference-input'
KEYS = dict(zip('1234567890', range(10, 20)))
KEYS.update(dict(zip('qwertyuiop', range(24, 34))))
KEYS.update(dict(zip('asdfghjkl', range(38, 47))))
KEYS.update(dict(zip('zxcvbnm', range(52, 59))))
KEYS.update({f'F{i}': 66 + i for i in range(1, 11)})
KEYS.update(F11=95, F12=96, Escape=9, BackSpace=22, Tab=23, Return=36,
            Control_L=37, Shift_L=50, Shift_R=62, Alt_L=64, space=65,
            Home=110, Up=111, Prior=112, Left=113, Right=114, End=115,
            Down=116, Next=117, Insert=118, Delete=119)
BUTTONS = {'left': 272, 'right': 273, 'middle': 274}


def run(*args, timeout=5):
    return subprocess.check_output(args, text=True, timeout=timeout).strip()


def process_is_original(pid):
    return isinstance(pid, int) and pid > 0 and Path(f'/proc/{pid}/exe').resolve().name == 'PE_Client.x86_64'


def window(pid=None):
    clients = json.loads(run('hyprctl', 'clients', '-j'))
    matches = [c for c in clients if c.get('title') == 'Planet Explorers'
               and (pid is None or c.get('pid') == pid) and process_is_original(c.get('pid'))]
    if len(matches) != 1:
        raise ValueError('Expected one original Planet Explorers client; use --pid if several are open.')
    client = matches[0]
    if not re.fullmatch(r'0x[0-9a-fA-F]+', client.get('address', '')):
        raise ValueError('Invalid compositor client address.')
    for key in ('at', 'size'):
        if not isinstance(client.get(key), list) or len(client[key]) != 2 or any(type(v) is not int for v in client[key]):
            raise ValueError('Invalid client rectangle.')
    if any(v < 8 or v > 16384 for v in client['size']):
        raise ValueError('Invalid client dimensions.')
    return client


def dispatch(expression):
    run('hyprctl', 'eval', 'hl.dispatch(' + expression + ')')


def focus(pid=None):
    client = window(pid)
    dispatch('hl.dsp.focus({window="address:' + client['address'] + '"})')
    time.sleep(0.1)
    active = json.loads(run('hyprctl', 'activewindow', '-j'))
    if active.get('address') != client['address']:
        raise ValueError('The original game did not receive focus; no input sent.')
    return window(client['pid'])


def desktop_bounds(monitors):
    rectangles = []
    for monitor in monitors:
        if monitor.get('disabled'):
            continue
        x, y = monitor['x'], monitor['y']
        width, height, scale = monitor['width'], monitor['height'], monitor['scale']
        if not all(isinstance(v, (int, float)) and math.isfinite(v) for v in (x, y, width, height, scale)) or scale <= 0:
            raise ValueError('Invalid monitor dimensions.')
        if int(monitor.get('transform', 0)) % 2:
            width, height = height, width
        rectangles.append((int(x), int(y), int(x + width / scale), int(y + height / scale)))
    if not rectangles:
        raise ValueError('No active monitors.')
    left, top = min(r[0] for r in rectangles), min(r[1] for r in rectangles)
    right, bottom = max(r[2] for r in rectangles), max(r[3] for r in rectangles)
    if not 8 <= right - left <= 65535 or not 8 <= bottom - top <= 65535:
        raise ValueError('Unsupported desktop bounds.')
    return left, top, right - left, bottom - top


def click_coordinates(client, x, y, bounds):
    width, height = client['size']
    if not 0 <= x < width or not 0 <= y < height:
        raise ValueError('Click coordinates must be inside the original client.')
    jitter_x = x - 4 if x >= 4 else x + 4
    left, top, desktop_width, desktop_height = bounds
    px, py = client['at'][0] + x - left, client['at'][1] + y - top
    jx = client['at'][0] + jitter_x - left
    if not 0 <= px < desktop_width or not 0 <= py < desktop_height or not 0 <= jx < desktop_width:
        raise ValueError('Click or jitter is outside the compositor desktop.')
    return px, py, desktop_width, desktop_height, jx


def build_pointer():
    protocol, source = HERE / 'wlr-virtual-pointer-unstable-v1.xml', HERE / 'reference_pointer.c'
    executable = BUILD / 'reference-pointer'
    if executable.exists() and executable.stat().st_mtime >= max(source.stat().st_mtime, protocol.stat().st_mtime):
        return executable
    BUILD.mkdir(parents=True, exist_ok=True)
    header, code = BUILD / 'virtual-pointer.h', BUILD / 'virtual-pointer.c'
    subprocess.run(['wayland-scanner', 'client-header', str(protocol), str(header)], check=True, timeout=15)
    subprocess.run(['wayland-scanner', 'private-code', str(protocol), str(code)], check=True, timeout=15)
    flags = shlex.split(run('pkg-config', '--cflags', '--libs', 'wayland-client'))
    temporary = executable.with_name(executable.name + f'.{os.getpid()}.tmp')
    try:
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-O2',
                        '-I', str(BUILD), str(source), str(code), '-o', str(temporary), *flags],
                       check=True, timeout=30)
        temporary.replace(executable)
    finally:
        temporary.unlink(missing_ok=True)
    return executable


@contextmanager
def xdisplay():
    x, xt = ctypes.CDLL('libX11.so.6'), ctypes.CDLL('libXtst.so.6')
    x.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x.XOpenDisplay.restype = ctypes.c_void_p
    x.XFlush.argtypes = [ctypes.c_void_p]
    x.XCloseDisplay.argtypes = [ctypes.c_void_p]
    xt.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
    xt.XTestFakeRelativeMotionEvent.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_ulong]
    display = x.XOpenDisplay(None)
    if not display:
        raise ValueError('Cannot open Xwayland DISPLAY.')
    try:
        yield x, xt, display
    finally:
        x.XCloseDisplay(display)


def key_codes(chord):
    names = chord.split('+')
    if not 1 <= len(names) <= 4 or any(name not in KEYS for name in names):
        raise ValueError('Unsupported chord; use fixed X11 names such as Shift_L+w, F5, space, Return.')
    codes = [KEYS[name] for name in names]
    if len(codes) != len(set(codes)):
        raise ValueError('Duplicate key in chord.')
    return codes


def send_key(chord, duration, pid):
    codes = key_codes(chord)
    if not math.isfinite(duration) or not 0 < duration <= 5:
        raise ValueError('Key duration must be greater than zero and at most 5 seconds.')
    focus(pid)
    with xdisplay() as (x, xt, display):
        pressed = []
        try:
            for code in codes:
                pressed.append(code)
                if not xt.XTestFakeKeyEvent(display, code, 1, 0):
                    raise RuntimeError('XTest rejected key press.')
            x.XFlush(display)
            time.sleep(duration)
        finally:
            for code in reversed(pressed):
                xt.XTestFakeKeyEvent(display, code, 0, 0)
            x.XFlush(display)


def look(dx, dy, pid):
    if max(abs(dx), abs(dy)) > 2000:
        raise ValueError('Look displacement is limited to 2000 pixels per axis.')
    focus(pid)
    with xdisplay() as (x, xt, display):
        steps = max(1, math.ceil(max(abs(dx), abs(dy)) / 20))
        px = py = 0
        for step in range(1, steps + 1):
            nx, ny = round(dx * step / steps), round(dy * step / steps)
            if not xt.XTestFakeRelativeMotionEvent(display, nx - px, ny - py, 0):
                raise RuntimeError('XTest rejected relative motion.')
            x.XFlush(display)
            time.sleep(0.03)
            px, py = nx, ny


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--pid', type=int)
    commands = parser.add_subparsers(dest='action', required=True)
    layout = commands.add_parser('layout')
    layout.add_argument('--width', type=int, default=1280)
    layout.add_argument('--height', type=int, default=720)
    shot = commands.add_parser('shot');shot.add_argument('output', type=Path)
    click = commands.add_parser('click');click.add_argument('x', type=int);click.add_argument('y', type=int)
    click.add_argument('--button', choices=BUTTONS, default='left')
    drag = commands.add_parser('drag')
    for coordinate in ('x', 'y', 'end_x', 'end_y'):
        drag.add_argument(coordinate, type=int)
    drag.add_argument('--button', choices=BUTTONS, default='left')
    key = commands.add_parser('key');key.add_argument('chord');key.add_argument('--duration', type=float, default=0.18)
    motion = commands.add_parser('look');motion.add_argument('dx', type=int);motion.add_argument('dy', type=int)
    args = parser.parse_args(argv)
    try:
        if args.pid is not None and args.pid <= 0:
            raise ValueError('PID must be positive.')
        if args.action == 'key':
            send_key(args.chord, args.duration, args.pid)
        elif args.action == 'look':
            look(args.dx, args.dy, args.pid)
        elif args.action in ('click', 'drag'):
            # Validate before compiling or focusing; requery after compilation.
            client = window(args.pid)
            bounds = desktop_bounds(json.loads(run('hyprctl', 'monitors', '-j')))
            click_coordinates(client, args.x, args.y, bounds)
            if args.action == 'drag':
                click_coordinates(client, args.end_x, args.end_y, bounds)
            helper = build_pointer()
            client = focus(client['pid'])
            bounds = desktop_bounds(json.loads(run('hyprctl', 'monitors', '-j')))
            px, py, width, height, jx = click_coordinates(client, args.x, args.y, bounds)
            coordinates = (px, py, width, height, BUTTONS[args.button], jx, py)
            if args.action == 'drag':
                end_x, end_y, *_ = click_coordinates(client, args.end_x, args.end_y, bounds)
                coordinates += (end_x, end_y)
            subprocess.run([str(helper), *map(str, coordinates)], check=True, timeout=6)
        elif args.action == 'shot':
            output = args.output.expanduser().resolve()
            if output.suffix.lower() != '.png' or not output.parent.is_dir():
                raise ValueError('Screenshot requires an explicit .png path in an existing directory.')
            client = focus(args.pid)
            time.sleep(0.3)
            client = window(client['pid'])
            px, py = client['at'];width, height = client['size']
            subprocess.run(['grim', '-g', f'{px},{py} {width}x{height}', str(output)], check=True, timeout=10)
            print(output)
        elif args.action == 'layout':
            if not 320 <= args.width <= 4096 or not 240 <= args.height <= 2160:
                raise ValueError('Layout size must be within 320x240 and 4096x2160.')
            client = focus(args.pid)
            target = 'window="address:' + client['address'] + '"'
            dispatch('hl.dsp.window.float({' + target + ',action="enable"})')
            dispatch('hl.dsp.window.resize({' + target + f',x={args.width},y={args.height},relative=false' + '})')
            dispatch('hl.dsp.window.center({' + target + '})')
            time.sleep(0.5)
        return 0
    except (OSError, ValueError, RuntimeError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print(f'Reference input failed: {error}', file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130


if __name__ == '__main__':
    sys.exit(main())
