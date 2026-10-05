#!/usr/bin/env python3
"""Bounded physical-input playtest of the local C sandbox (never the Steam client).

Requires Hyprland/Xwayland, recovered assets, build/planet_explorers and
build/sandbox_action_fixture. Creates isolated fixture/result saves and captures
under --output-dir. No normal save is loaded or overwritten. The fixed action
holds last eight seconds, and releases/owned-window cleanup run on failure.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import threading
import time
import rebuild_input as driver

ROOT = Path(__file__).resolve().parents[2]
HELPER = ROOT / 'rebuild/build/sandbox_action_fixture'


def inspect(path):
    result = subprocess.run([str(HELPER), '--inspect', str(path)], cwd=ROOT,
                            check=True, capture_output=True, text=True, timeout=15)
    fields = dict(re.findall(r'(\w+)=([^\s]+)', result.stdout.splitlines()[0]))
    trees = {}
    for line in result.stdout.splitlines()[1:]:
        match = re.match(r'  tree(\d+) remaining=(\d+) health=([\d.]+)', line)
        if match:
            trees[int(match[1])] = (int(match[2]), float(match[3]))
    return fields, trees, result.stdout


def fresh_save(pid, path):
    """Require this F9 press to create its owned output; never inspect an old save."""
    path.unlink(missing_ok=True)
    driver.transport.send_key('F9', .18, pid)
    deadline = time.monotonic() + 3
    while not path.is_file():
        if time.monotonic() >= deadline:
            raise RuntimeError(f'F9 did not create a fresh save: {path}')
        time.sleep(.05)


def cleanup_process(process):
    """Close only this Popen child, even if its allowlisted window vanished."""
    if process.poll() is None:
        try:
            client = driver.window(process.pid)
            driver.transport.dispatch('hl.dsp.window.close({window="address:' + client['address'] + '"})')
        except Exception:
            # Window lookup/close can fail during a crash or compositor reset.
            # Direct fallbacks below target only the process we launched.
            pass
    try:
        process.wait(timeout=10)
        return
    except subprocess.TimeoutExpired:
        pass
    try:
        process.terminate()
    except ProcessLookupError:
        pass
    try:
        process.wait(timeout=2)
        return
    except subprocess.TimeoutExpired:
        pass
    try:
        process.kill()
    except ProcessLookupError:
        pass
    process.wait(timeout=2)


def run_action(kind, folder):
    fixture, saved = folder / f'{kind}-fixture.pesave', folder / f'{kind}-result.pesave'
    saved.unlink(missing_ok=True)
    before, old_trees, before_text = inspect(fixture)
    log = folder / f'{kind}.log'
    driver.transport.window = driver.window
    t = driver.transport
    with log.open('w') as stream:
        process = subprocess.Popen([str(driver.EXECUTABLE), '--load', str(fixture),
                                    '--save', str(saved)], cwd=ROOT, stdout=stream,
                                   stderr=subprocess.STDOUT)
        try:
            until = time.monotonic() + 30
            while 'Sandbox ready:' not in log.read_text():
                if process.poll() is not None or time.monotonic() > until:
                    raise RuntimeError(f'{kind}: sandbox did not become ready: {log.read_text()}')
                time.sleep(.1)
            if t.main(['--pid', str(process.pid), 'layout']):
                raise RuntimeError('Unable to arrange owned game window')
            time.sleep(.3)
            def capture(name):
                if t.main(['--pid', str(process.pid), 'shot', str(folder / f'{kind}-{name}.png')]):
                    raise RuntimeError('Unable to capture owned game window')
            capture('before')
            errors = []
            def hold():
                try:
                    driver.hold('left', 8, process.pid)
                except Exception as error:
                    errors.append(error)
            thread = threading.Thread(target=hold)
            thread.start()
            try:
                time.sleep(1.25)
                capture('action')
            finally:
                thread.join(timeout=12)
            if thread.is_alive():
                raise RuntimeError('Bounded input hold did not terminate')
            if errors:
                raise errors[0]
            time.sleep(2)
            fresh_save(process.pid, saved)
            capture('after')
        finally:
            cleanup_process(process)
    if process.returncode:
        raise RuntimeError(log.read_text())
    after, new_trees, after_text = inspect(saved)
    (folder / f'{kind}-state.txt').write_text(before_text + '\n' + after_text)
    match = re.search(r'Sandbox tools: (\d+) swings, (\d+) resource changes', log.read_text())
    if not match:
        raise RuntimeError('Missing action counters')
    swings, changes = map(int, match.groups())
    assert swings > 0 and changes > 0
    assert float(before['wear_remaining']) - float(after['wear_remaining']) == 5 * swings
    assert before['tool'] == after['tool'] and before['instance'] == after['instance']
    assert before['juice'] == after['juice'] and after['cells'] == '0'
    if kind in ('dig', 'mine'):
        assert int(after['dirt']) > int(before['dirt']) and int(after['gathered']) > 0
        assert old_trees == new_trees and before['wood'] == after['wood']
    else:
        removed = [index for index in old_trees if new_trees[index] != old_trees[index]]
        assert len(removed) == 1 and new_trees[removed[0]] == (0, 0)
        assert int(after['wood']) - int(before['wood']) == old_trees[removed[0]][0]
        assert after['gathered'] == '1' and before['dirt'] == after['dirt']
    print(f'{kind}: PASS, {swings} authored swings, {changes} resource changes', flush=True)
    return {'kind': kind, 'swings': swings, 'changes': changes, 'before': before,
            'after': after, 'log': str(log), 'passed': True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'rebuild/build/sandbox-action-playtest')
    parser.add_argument('--action', choices=('dig', 'mine', 'fell'), action='append',
                        help='Select one or more checks; default runs all three')
    args = parser.parse_args()
    folder = args.output_dir.expanduser().resolve()
    folder.mkdir(parents=True, exist_ok=True)
    (folder / 'result.json').unlink(missing_ok=True)
    with (folder / 'fixtures.log').open('w') as log:
        subprocess.run([str(HELPER), str(folder / 'dig-fixture.pesave'), str(folder / 'fell-fixture.pesave')],
                       cwd=ROOT, check=True, stdout=log, timeout=30)
        subprocess.run([str(HELPER), '--mine', str(folder / 'mine-fixture.pesave')],
                       cwd=ROOT, check=True, stdout=log, timeout=30)
    results = [run_action(kind, folder) for kind in dict.fromkeys(args.action or ('dig', 'mine', 'fell'))]
    (folder / 'result.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
