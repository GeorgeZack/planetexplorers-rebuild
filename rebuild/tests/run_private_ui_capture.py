"""Staged rendering on an explicitly selected, owned high X display only."""
from pathlib import Path
import argparse
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
    args = parser.parse_args()
    if sum((args.shared,args.settings,args.loot,args.materials))>1: parser.error('select one capture type')
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
                result = subprocess.run([str(root/'rebuild/build'/binary),str(out)],
                    env=env,cwd=root,stdout=render_log,stderr=subprocess.STDOUT,timeout=180)
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
