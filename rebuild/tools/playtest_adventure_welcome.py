#!/usr/bin/env python3
"""Physical welcome/resume check using only an isolated C rebuild window/save."""
import argparse
import json
from pathlib import Path
import subprocess
import time
import rebuild_input as driver
from playtest_adventure_supplier import HELPER, inspect
from playtest_sandbox_actions import ROOT, fresh_save
from playtest_sandbox_trees import session, capture


def run(folder):
    fixture=folder/'fixture.pesave'
    subprocess.run([str(HELPER),'--welcome',str(fixture)],cwd=ROOT,check=True,timeout=60)
    states=[]
    initial,initial_text=inspect(fixture)
    inventory=initial_text.split('  inventory:',1)[1]
    assert initial['welcome_page']==initial['mission']==initial['supplies']=='0'
    t=driver.transport
    with session(folder,'welcome',fixture) as (pid,saved,log):
        def key(name):
            t.send_key(name,.15,pid);time.sleep(.25)
        def checkpoint(stage,page):
            fresh_save(pid,saved)
            fields,text=inspect(saved)
            assert fields['welcome_page']==str(page),(stage,fields)
            assert fields['supplies']=='0'
            if page<9:assert fields['mission']==fields['arrival']=='0'
            assert text.split('  inventory:',1)[1]==inventory
            states.append(dict(stage=stage,fields=fields))
        time.sleep(.5);capture(folder,'page-1949',pid)
        # Opening is automatic; attempt unrelated gameplay/panel input while modal.
        key('b');key('1');key('F9')
        assert not saved.exists()
        for _ in range(3):key('Return')
        capture(folder,'page-4083',pid)
        key('Escape');checkpoint('cancelled',3)
        key('F10');time.sleep(.5)
        capture(folder,'resumed-4083',pid)
        for page in range(3,8):
            assert not t.main(['--pid',str(pid),'click','700','525','--button','left'])
            time.sleep(.25)
            if page==3:capture(folder,'page-4084',pid)
        capture(folder,'page-4088',pid)
        key('Escape');checkpoint('before-final-ack',8)
        key('F10');time.sleep(.5);key('Return')
        checkpoint('accepted',9)
        capture(folder,'accepted',pid)
        # Completed welcome does not reopen after loading; NPC offers only close.
        key('F10');time.sleep(.5);key('e')
        capture(folder,'completed-menu',pid)
        key('Return');checkpoint('reopened',9)
        key('F10');time.sleep(.5)
        checkpoint('reloaded',9)
    assert 'Sandbox tools: 0 swings, 0 resource changes' in log.read_text()
    (folder/'result.json').write_text(json.dumps(dict(passed=True,states=states,log=str(log.relative_to(ROOT))),indent=2)+'\n')
    print('Welcome physical input: auto-open, modal input, cancel/resume, final ACK and reload PASS')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir',type=Path,default=ROOT/'rebuild/build/welcome-playtest')
    folder=parser.parse_args().output_dir.resolve();folder.mkdir(parents=True,exist_ok=True)
    run(folder)

if __name__=='__main__':main()
