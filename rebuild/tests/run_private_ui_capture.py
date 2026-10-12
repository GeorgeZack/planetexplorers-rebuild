"""Staged rendering on an explicitly selected, owned high X display only."""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import os
import signal
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'rebuild/tools'))
import native_private_input as private


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--shared',action='store_true',help='compare tutorial and sandbox shared UI')
    parser.add_argument('--settings',action='store_true',help='render and exercise shared main-menu settings')
    parser.add_argument('--loot',action='store_true',help='render staged hunt loot icons')
    parser.add_argument('--materials',action='store_true',help='verify actor alpha materials and capture portraits')
    parser.add_argument('--reference-materials',action='store_true',help='verify static model materials and capture buildings')
    parser.add_argument('--reactions',action='store_true',help='render all staged reaction leaves with attachments and defence')
    parser.add_argument('--knife-reactions',action='store_true',help='use staged Knife33 attachments and blade geometry in the reaction fixture')
    parser.add_argument('--output',type=Path,help='fresh output directory for reaction captures')
    args = parser.parse_args()
    if args.knife_reactions and not args.reactions: parser.error('--knife-reactions requires --reactions')
    if args.output and not args.reactions: parser.error('--output requires --reactions')
    if sum((args.shared,args.settings,args.loot,args.materials,args.reference_materials,args.reactions))>1: parser.error('select one capture type')
    out = root/('rebuild/build/ui-shared-captures' if args.shared else 'rebuild/build/ui-details-captures')
    binary = 'render_shared_ui' if args.shared else 'render_shop_details_ui'
    if args.settings:
        out=root/'rebuild/build/settings-captures'
        binary='render_app_settings'
    if args.loot:
        out=root/'rebuild/build/hunt-loot-captures'
        binary='render_hunt_loot'
    if args.materials:
        out=root/'rebuild/build/actor-material-captures'
        binary='render_actor_materials'
    if args.reference_materials:
        out=root/'rebuild/build/reference-material-captures'
        binary='render_reference_materials'
    if args.reactions:
        out=args.output or root/'rebuild/build/reaction-captures-01'
        out=out.resolve()
        if not out.is_relative_to(root/'rebuild/build') or out.exists():
            parser.error('reaction output must be a fresh directory under rebuild/build')
        stage=root/'rebuild/build/starter-pair-reaction-01'
        baseline=root/'rebuild/build/reference-assets'
        mappings={'player-reaction':'player','player-reaction-wood-sword-drawn':'player-wood-sword-drawn',
                  'player-reaction-wood-sword-stowed':'player-wood-sword-stowed','player-reaction-wood-shield':'player-wood-shield',
                  'player-reaction-defence':'player-defence'}
        if not baseline.is_dir() or any(not (stage/folder).is_dir() for folder in mappings.values()):
            parser.error('installed baseline and complete staged reaction group required')
        out.mkdir()
        assets=out/'assets';assets.mkdir()
        for source in baseline.iterdir(): (assets/source.name).symlink_to(source,target_is_directory=source.is_dir())
        for name,folder in mappings.items():
            if (assets/name).exists() or (assets/name).is_symlink():
                (assets/name).unlink()
            (assets/name).symlink_to(stage/folder,target_is_directory=True)
        knife_sources={}
        if args.knife_reactions:
            for attachment in ('drawn','stowed'):
                source=root/f'rebuild/build/knife-reaction-01-{attachment}'
                if not (source/'recording.peas').is_file(): parser.error('staged Knife33 reaction scenes required')
                (assets/f'player-reaction-knife-{attachment}').symlink_to(source,target_is_directory=True)
                knife_sources.update({f'knife-{attachment}/{p.relative_to(source)}':p for p in source.rglob('*') if p.is_file()})
        binary='render_reaction_group'
        stage_hashes={str(p.relative_to(stage)):hashlib.sha256(p.read_bytes()).hexdigest()
                      for p in stage.rglob('*') if p.is_file()}
        stage_hashes.update({key:hashlib.sha256(p.read_bytes()).hexdigest() for key,p in knife_sources.items()})
    else:
        out.mkdir(exist_ok=True)
    server = None
    try:
        with (out/'xvfb.log').open('w') as log:
            server,display = private.start_private_server(log)
            env = os.environ.copy()
            env.pop('WAYLAND_DISPLAY',None)
            env.update(DISPLAY=display,PE_UI_PRIVATE_DISPLAY=display,
                       LIBGL_ALWAYS_SOFTWARE='1',GALLIUM_DRIVER='llvmpipe')
            print(f'Private Xvfb display {display}',flush=True)
            with (out/'render.log').open('w') as render_log:
                command=[str(root/'rebuild/build'/binary),str(out)]
                if args.reactions: command.append(str(assets))
                if args.knife_reactions: command.append('knife')
                result = subprocess.run(command,
                    env=env,cwd=root,stdout=render_log,stderr=subprocess.STDOUT,timeout=180)
            if args.reactions and result.returncode==0:
                after={str(p.relative_to(stage)):hashlib.sha256(p.read_bytes()).hexdigest()
                       for p in stage.rglob('*') if p.is_file()}
                after.update({key:hashlib.sha256(p.read_bytes()).hexdigest() for key,p in knife_sources.items()})
                if stage_hashes!=after: raise private.ToolError('staged input changed during render')
                with (out/'poses.csv').open() as stream: rows=list(csv.DictReader(stream))
                names=list(dict.fromkeys(row['clip'] for row in rows))
                if len(rows)!=100 or len(names)!=25: raise private.ToolError('incomplete reaction pose report')
                images=[out/f'{i:02d}-{name}.png' for i,name in enumerate(names)]
                if any(not p.is_file() for p in images): raise private.ToolError('missing reaction image')
                report={'schema':1,'kind':'native-isolated-reaction-leaf-fixture','display':display,
                        'clips':names,'panels':200,'stage_sha256':stage_hashes,
                        'images_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in images},
                        'poses_sha256':hashlib.sha256((out/'poses.csv').read_bytes()).hexdigest(),
                        'recovery_images_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in out.glob('knife-recovery-*.png')},
                        'renderer_sha256':hashlib.sha256((root/'rebuild/build'/binary).read_bytes()).hexdigest(),
                        'stage_unchanged':True,'live_admission':False,'attachment':'knife33' if args.knife_reactions else 'wood-sword-shield'}
                with (out/'result.json').open('x') as stream: json.dump(report,stream,indent=2);stream.write('\n')
            print((out/'render.log').read_text())
            return result.returncode
    finally:
        private.end_owned_children(server)


if __name__ == '__main__':
    private.install_signal_handlers()
    try:
        private.set_parent_death(signal.SIGHUP,os.getppid())
        sys.exit(main())
    except (private.ToolError,OSError,ValueError,subprocess.SubprocessError) as error:
        print(f'private-ui-capture: {error}',file=sys.stderr)
        sys.exit(1)
