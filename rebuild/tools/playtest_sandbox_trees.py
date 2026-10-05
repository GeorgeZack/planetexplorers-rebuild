#!/usr/bin/env python3
"""Physical-input tree, legacy migration and zoom checks for the local C rebuild.

Requires an unlocked Hyprland/Xwayland desktop and recovered assets. Uses only
fresh fixtures/results under --output-dir; never opens a normal save or Steam.
Run with a single desktop operator. All input holds are bounded and released.
"""
import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import time
from contextlib import contextmanager
import rebuild_input as driver
from playtest_sandbox_actions import ROOT, HELPER, inspect, fresh_save, cleanup_process


@contextmanager
def session(folder, name, fixture):
    driver.transport.window = driver.window
    t = driver.transport
    log, saved = folder / f'{name}.log', folder / f'{name}-result.pesave'
    saved.unlink(missing_ok=True)
    with log.open('w') as stream:
        process = subprocess.Popen([str(driver.EXECUTABLE), '--load', str(fixture),
                                    '--save', str(saved)], cwd=ROOT, stdout=stream,
                                   stderr=subprocess.STDOUT)
        try:
            until = time.monotonic() + 30
            while 'Sandbox ready:' not in log.read_text():
                if process.poll() is not None or time.monotonic() > until:
                    raise RuntimeError(log.read_text())
                time.sleep(.1)
            if t.main(['--pid', str(process.pid), 'layout']):
                raise RuntimeError('Unable to arrange owned window')
            yield process.pid, saved, log
        finally:
            cleanup_process(process)
    if process.returncode:
        raise RuntimeError(log.read_text())


def capture(folder, name, pid):
    if driver.transport.main(['--pid', str(pid), 'shot', str(folder / f'{name}.png')]):
        raise RuntimeError('Unable to capture owned window')


def save(pid, path):
    fresh_save(pid, path)
    return inspect(path)


def position(fields):
    return tuple(map(float, fields['position'].strip('()').split(',')))


def wheel(pid):
    t = driver.transport
    t.focus(pid)
    with t.xdisplay() as (x, xt, display):
        xt.XTestFakeButtonEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
        for _ in range(3):
            try:
                if not xt.XTestFakeButtonEvent(display, 4, 1, 0):
                    raise RuntimeError('Wheel press rejected')
                x.XFlush(display)
                time.sleep(.03)
            finally:
                xt.XTestFakeButtonEvent(display, 4, 0, 0)
                x.XFlush(display)
            time.sleep(.1)


def run(folder):
    (folder / 'result.json').unlink(missing_ok=True)
    dig, fell, legacy = (folder / name for name in ('dig.pesave', 'fell.pesave', 'overlap-v4.pesave'))
    subprocess.run([str(HELPER), str(dig), str(fell)], cwd=ROOT, check=True, capture_output=True, timeout=30)
    subprocess.run([str(HELPER), '--migration', str(legacy)], cwd=ROOT, check=True, capture_output=True, timeout=30)
    t = driver.transport
    states = []
    with session(folder, 'tree', fell) as (pid, saved, log):
        time.sleep(.5)
        states.append(inspect(fell))
        for name, duration in (('contact', .4), ('blocked', .2)):
            t.send_key('w', duration, pid)
            states.append(save(pid, saved))
            capture(folder, 'tree-' + name, pid)
        a, b = position(states[1][0]), position(states[2][0])
        # Rounded trunks permit tangential sliding. Test suppressed forward
        # progress over a short hold, not indefinite pinning at a curved wall.
        assert abs(a[0] - b[0]) < .15 and abs(a[2] - b[2]) < .2, (a, b)
        assert position(states[0][0])[0] - a[0] > .5
        for state in states[1:]:
            assert state[1] == states[0][1]
            p = position(state[0])
            assert ((p[0] - 40.080023)**2 + (p[2] - 31.227984)**2)**.5 >= 1.299
        driver.hold('left', 8, pid)
        time.sleep(2)
        states.append(save(pid, saved))
        capture(folder, 'tree-depleted', pid)
        assert states[3][1][7] == (0, 0), states[3][2]
        assert all(value == states[3][1][index] for index, value in states[0][1].items() if index != 7)
        assert int(states[3][0]['wood']) - int(states[0][0]['wood']) == 8
        assert states[3][0]['instance'] == states[0][0]['instance']
        assert float(states[0][0]['wear_remaining']) - float(states[3][0]['wear_remaining']) == 30
        t.send_key('w', 1, pid)
        states.append(save(pid, saved))
        capture(folder, 'tree-passed', pid)
        assert position(states[4][0])[0] < 38.78, states[4][2]
    (folder / 'tree-states.txt').write_text('\n'.join(s[2] for s in states))
    print('Tree: blocked, six swings, eight wood, cleared trunk crossed: PASS', flush=True)

    old_hash = hashlib.sha256(legacy.read_bytes()).hexdigest()
    assert struct.unpack_from('<I', legacy.read_bytes(), 4)[0] == 4
    with session(folder, 'migration', legacy) as (pid, saved, log):
        capture(folder, 'migration-notice', pid)
        migrated = save(pid, saved)
        assert struct.unpack_from('<I', saved.read_bytes(), 4)[0] == 5
        assert abs(position(migrated[0])[0] - 41.430023) < .02
        assert migrated[0]['wood'] == '200' and migrated[0]['wear_remaining'] == '3553.000'
    assert old_hash == hashlib.sha256(legacy.read_bytes()).hexdigest()
    with session(folder, 'migration-reload', saved) as (pid, resaved, log):
        reloaded = save(pid, resaved)
        assert position(reloaded[0]) == position(migrated[0])
        assert migrated[1] == reloaded[1]
    print('Migration: v4 input unchanged, safe v5 save/reload: PASS', flush=True)

    with session(folder, 'zoom', dig) as (pid, saved, log):
        capture(folder, 'zoom-default', pid)
        wheel(pid)
        time.sleep(2.5)
        capture(folder, 'zoom-closer', pid)
        t.send_key('c', .18, pid)
        wheel(pid)
        t.send_key('c', .18, pid)
        time.sleep(.5)
        capture(folder, 'zoom-after-panel', pid)
    match = re.search(r'Sandbox camera: desired ([\d.]+)m, distance ([\d.]+)m', log.read_text())
    assert match and abs(float(match[1]) - 3.6) < .01 and abs(float(match[2]) - 3.6) < .01, log.read_text()
    print('Zoom: smoothed to 3.6 m, character panel consumed wheel: PASS', flush=True)
    (folder / 'result.json').write_text(json.dumps({'tree_states': [s[0] for s in states],
        'migration': migrated[0], 'legacy_sha256': old_hash,
        'zoom_desired': float(match[1]), 'zoom_distance': float(match[2]), 'passed': True}, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'rebuild/build/sandbox-tree-playtest')
    args = parser.parse_args()
    folder = args.output_dir.expanduser().resolve()
    folder.mkdir(parents=True, exist_ok=True)
    run(folder)
