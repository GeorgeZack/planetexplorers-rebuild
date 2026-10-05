#!/usr/bin/env python3
"""Bounded tutorial/sandbox panel-drag and tracker capture; owns only C windows."""
import json
from pathlib import Path
import subprocess
import time
import rebuild_input as driver
from playtest_sandbox_actions import ROOT, cleanup_process
from playtest_sandbox_trees import session, capture


def controls(folder,pid,prefix):
    t=driver.transport
    def key(name):t.send_key(name,.15,pid);time.sleep(.25)
    def command(*args):
        assert not t.main(['--pid',str(pid),*map(str,args)])
        time.sleep(.25)
    capture(folder,prefix+'-tracker-1280',pid)
    command('layout','--width',1000,'--height',700)
    capture(folder,prefix+'-tracker-1000',pid)
    command('layout','--width',1280,'--height',720)
    key('c');key('i')
    capture(folder,prefix+'-panels-before',pid)
    # Prime the newly released Xwayland cursor with a harmless modal-gap click.
    command('click',700,400)
    # Inventory root752,110 ->850,75. Character root5,84 ->270,61.
    command('drag',832,134,930,99)
    command('drag',85,108,350,85)
    capture(folder,prefix+'-panels-dragged',pid)
    # Recovered inventory close moves from1101,126 ->1199,91.
    command('click',1208,100)
    capture(folder,prefix+'-moved-close',pid)
    key('i')
    command('layout','--width',1000,'--height',700)
    capture(folder,prefix+'-panels-resized',pid)
    key('i');key('c');key('q')
    # Mission panel uses centered layout at this aspect. Move its header up.
    command('drag',250,160,250,105)
    capture(folder,prefix+'-mission-dragged',pid)
    key('q')


def main():
    folder=ROOT/'rebuild/build/ui-windows-playtest';folder.mkdir(parents=True,exist_ok=True)
    driver.transport.window=driver.window
    log=folder/'tutorial.log'
    with log.open('w') as stream:
        process=subprocess.Popen([str(driver.EXECUTABLE),'--training-room',str(ROOT/'rebuild/build/reference-assets')],cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT)
        try:
            deadline=time.monotonic()+45
            while True:
                try:driver.window(process.pid);break
                except ValueError:
                    if process.poll() is not None or time.monotonic()>deadline:raise RuntimeError(log.read_text())
                    time.sleep(.2)
            time.sleep(2)
            assert not driver.transport.main(['--pid',str(process.pid),'layout'])
            for _ in range(2):driver.transport.send_key('Return',.15,process.pid);time.sleep(.3)
            controls(folder,process.pid,'tutorial')
        finally:cleanup_process(process)
    assert process.returncode==0,log.read_text()
    fixture=folder/'welcome-fixture.pesave'
    subprocess.run([str(ROOT/'rebuild/build/adventure_supplier_fixture'),'--welcome',str(fixture)],cwd=ROOT,check=True,timeout=60)
    with session(folder,'sandbox',fixture) as (pid,saved,log):
        time.sleep(.5)
        for _ in range(9):driver.transport.send_key('Return',.15,pid);time.sleep(.25)
        controls(folder,pid,'sandbox')
    assert 'Sandbox tools: 0 swings, 0 resource changes' in log.read_text()
    (folder/'result.json').write_text(json.dumps(dict(completed=True,sizes=[[1280,720],[1000,700]],checks=['tracker clearance','title drag','moved close','resize clamp','mission panel drag'],note='Screenshots require visual review; deterministic geometry/input tests separately verify state.'),indent=2)+'\n')
    print('Tutorial and sandbox window drag/capture sequences completed; owned windows closed.')

if __name__=='__main__':main()
