#!/usr/bin/env python3
"""Physical supplier interaction check against the C rebuild, never Steam.

Requires recovered assets and an unlocked Hyprland/Xwayland desktop. One
operator only. Uses generated fixture/output saves under --output-dir and
closes its owned window even on failure. No normal player save is accessed.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import time

import rebuild_input as driver
from playtest_sandbox_actions import ROOT, fresh_save
from playtest_sandbox_trees import session, capture

HELPER = ROOT / 'rebuild/build/adventure_supplier_fixture'


def inspect(path):
    text = subprocess.run([str(HELPER), '--inspect', str(path)], cwd=ROOT,
                          check=True, capture_output=True, text=True, timeout=30).stdout
    return dict(re.findall(r'(\w+)=([^\s]+)', text.splitlines()[0])), text


def run(folder):
    fixture = folder / 'fixture.pesave'
    subprocess.run([str(HELPER), str(fixture)], cwd=ROOT, check=True, timeout=60)
    states = []
    def record(path, name):
        fields, text = inspect(path)
        states.append(dict(stage=name, fields=fields, text=text))
        return fields
    initial = record(fixture, 'initial')
    assert initial['arrival'] == initial['supplies'] == '0'
    t = driver.transport
    with session(folder, 'supplier', fixture) as (pid, saved, log):
        def key(name):
            t.send_key(name, .15, pid)
            time.sleep(.2)
        def click(x, y, button='left'):
            assert not t.main(['--pid', str(pid), 'click', str(x), str(y), '--button', button])
            time.sleep(.2)
        def checkpoint(name):
            fresh_save(pid, saved)
            return record(saved, name)
        time.sleep(.5)
        state = checkpoint('arrival-only')
        assert state['arrival'] == '1' and state['supplies'] == '0'
        capture(folder, 'arrival', pid)
        # Open with E, select the real menu row, cancel after the promise page.
        key('e')
        capture(folder, 'menu', pid)
        click(220, 234)
        key('Return'); key('Return')
        capture(folder, 'promise', pid)
        key('Escape')
        state = checkpoint('cancelled')
        assert state['supplies'] == state['pickaxes'] == state['shovels'] == '0'
        # Free cursor right-click follows the source NPC picking path.
        key('Tab')
        click(640, 340, 'right')
        capture(folder, 'mouse-menu', pid)
        click(220, 234)
        for page in range(4090, 4095):
            capture(folder, f'page-{page}', pid)
            # Click the conversation background, not just the next arrow.
            click(700, 525)
        state = checkpoint('accepted')
        for name in ('supplies', 'shovels', 'pickaxes', 'axes', 'script406'):
            assert state[name] == '1', (name, state)
        assert state['known'] == '0' and state['wood'] == '200'
        inventory = states[-1]['text'].split('  inventory:', 1)[1]
        capture(folder, 'accepted', pid)
        click(640, 340, 'right')
        capture(folder, 'claimed-menu', pid)
        click(220, 234)  # Only "That's all." remains.
        state = checkpoint('reopened')
        assert states[-1]['text'].split('  inventory:', 1)[1] == inventory
        key('F10'); time.sleep(.3)
        click(640, 340, 'right')
        capture(folder, 'reloaded-menu', pid)
        key('Escape')
        state = checkpoint('reloaded')
        assert state['supplies'] == '1'
        assert states[-1]['text'].split('  inventory:', 1)[1] == inventory
    assert 'Sandbox tools: 0 swings, 0 resource changes' in log.read_text()
    result = dict(passed=True, states=states, log=str(log.relative_to(ROOT)))
    (folder / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print('Supplier physical input: arrival, cancel, mouse dialogue, final grant, replay and reload PASS')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'rebuild/build/supplier-playtest')
    args = parser.parse_args()
    folder = args.output_dir.resolve()
    folder.mkdir(parents=True, exist_ok=True)
    (folder / 'result.json').unlink(missing_ok=True)
    run(folder)


if __name__ == '__main__':
    main()
