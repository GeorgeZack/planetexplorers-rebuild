#!/usr/bin/env python3
"""Stage ninth hare pounce clip without changing any installed runtime assets."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from export_content import ROOT
from export_reference_actor import BUNDLE,recover
from export_hare_pounce import BUNDLE_SHA,source_contract

def validate_prefix(old,new):
    if len(old)<24 or len(new)<=len(old) or old[:20]!=new[:20] or struct.unpack_from('<I',old,20)[0]!=8 or struct.unpack_from('<I',new,20)[0]!=9 or old[24:]!=new[24:len(old)]:raise ValueError('Existing actor mesh/skeleton/eight clips differ')
    offset=len(old);name,fps,frames,duration,loop=struct.unpack_from('<32sfIfI',new,offset)
    if name.rstrip(b'\0')!=b'attack5' or fps!=30 or frames!=31 or duration!=0.9666671752929688 or loop:raise ValueError('Staged pounce clip header changed')
    joints=struct.unpack_from('<I',new,16)[0]
    if len(new)!=offset+48+frames*joints*48:raise ValueError('Pounce clip palette size changed')
    values=struct.unpack_from('<'+str(frames*joints*12)+'f',new,offset+48)
    import math
    if not all(map(math.isfinite,values)):raise ValueError('Nonfinite pounce palette')
    return dict(old_actor_sha256=hashlib.sha256(old).hexdigest(),new_actor_sha256=hashlib.sha256(new).hexdigest(),unchanged_prefix_bytes=len(old)-24,joints=joints,frames=frames,palettes=frames*joints)

def validate_report(report,actor_bytes):
    from export_hare_pounce import CLIP_ID, STATE_ID, f32, SKILL
    if report['source_sha256'] != BUNDLE_SHA or len(report['clips']) != 9:
        raise ValueError('Staged pounce provenance changed')
    clip = report['clips'][-1]
    expected = {'name':'attack5','source_name':'attack5_pouncefront','path_id':CLIP_ID,
                'fps':30,'frame_count':31,'duration':0.9666671752929688,'loop':False,'dense_channels':302}
    if any(clip.get(k) != value for k,value in expected.items()):
        raise ValueError('Staged pounce clip metadata changed')
    combat = clip['combat']
    if combat['state_full_path_id'] != STATE_ID or combat['state_speed'] != 1 or combat['prepare_effect'] is not None or any(combat['skill'].get(k) != value for k,value in SKILL.items()):
        raise ValueError('Staged pounce action/skill binding changed')
    windows = combat['attack_windows']
    if len(windows) != 1 or windows[0]['start_normalized'] != f32(.575) or windows[0]['end_normalized'] != f32(.802) or windows[0]['bones'] != ['Bip01 L Forearm','Bip01 R Forearm'] or not windows[0]['start_inclusive'] or not windows[0]['end_inclusive']:
        raise ValueError('Staged pounce collision metadata changed')
    actual = hashlib.sha256(actor_bytes).hexdigest()
    if report['files']['hare.peac']['sha256'] != actual or report['files']['hare.peac']['bytes'] != len(actor_bytes):
        raise ValueError('Staged pounce actor hash changed')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game-dir',type=Path,default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    p.add_argument('--baseline',type=Path,default=ROOT/'rebuild/build/reference-assets/hare.peac')
    p.add_argument('--output',type=Path,default=ROOT/'rebuild/build/reference-assets/hare-pounce-staged-20261007')
    p.add_argument('--check',action='store_true');a=p.parse_args();out=a.output.resolve();game=a.game_dir.resolve();source_contract()
    if out==game or game in out.parents:raise ValueError('Output must be outside the original game')
    if a.check:
        report=json.loads((out/'hare-manifest.json').read_text())
        validate_report(report,(out/'hare.peac').read_bytes())
    else:
        if out.exists():raise ValueError('Require an absent staging directory; preserve prior output')
        out.mkdir(parents=True);report=recover(game/BUNDLE,out,include_pounce=True)
        validate_report(report,(out/'hare.peac').read_bytes())
    evidence=validate_prefix(a.baseline.read_bytes(),(out/'hare.peac').read_bytes())
    if a.check:
        if json.loads((out/'pounce-prefix-proof.json').read_text())!=evidence:raise ValueError('Staged prefix proof changed')
    else:(out/'pounce-prefix-proof.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print(('Verified' if a.check else 'Staged')+' pounce, unchanged first8 clips and skeleton; '+str(evidence['palettes'])+' finite palettes')
if __name__=='__main__':main()
