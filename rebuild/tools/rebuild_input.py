#!/usr/bin/env python3
"""Bounded keyboard, pointer and capture driver for the local C application window.

Uses reference_input's input transport, with a separate allowlisted-title and exact-
executable selector. The original reference driver's selector is unchanged.
Commands: --pid PID layout, shot FILE.png, key CHORD, click X Y, drag X Y X2 Y2.
For captured gameplay: --pid PID hold left|right SECONDS (at most10s).
"""
import argparse
import ctypes
import math
import sys
import time
import json
from pathlib import Path
import re
import reference_input as transport

EXECUTABLE = Path(__file__).resolve().parents[1] / 'build/planet_explorers'


def window(pid=None):
    clients = json.loads(transport.run('hyprctl', 'clients', '-j'))
    matches = []
    for client in clients:
        candidate = client.get('pid')
        if (client.get('title') not in {'Planet Explorers | Training Room', 'Planet Explorers | C rebuild', 'Planet Explorers | Main Menu'}
                or type(candidate) is not int or candidate <= 0
                or (pid is not None and candidate != pid)):
            continue
        if Path(f'/proc/{candidate}/exe').resolve() == EXECUTABLE.resolve():
            matches.append(client)
    if len(matches) != 1:
        raise ValueError('Expected one local C application window; specify --pid.')
    client = matches[0]
    if not re.fullmatch(r'0x[0-9a-fA-F]+', client.get('address', '')):
        raise ValueError('Invalid training window address.')
    for name in ('at', 'size'):
        value = client.get(name)
        if not isinstance(value, list) or len(value) != 2 or any(type(v) is not int for v in value):
            raise ValueError('Invalid training window rectangle.')
    if any(v < 8 or v > 16384 for v in client['size']):
        raise ValueError('Invalid training window dimensions.')
    return client


def hold(button, duration, pid):
    if button not in {"left", "right"} or not math.isfinite(duration) or not 0 < duration <= 10:
        raise ValueError("Hold requires left/right and 0 < duration <=10 seconds.")
    # This selector cannot target Steam or another arbitrary application.
    transport.window = window
    transport.focus(pid)
    code = 1 if button == "left" else 3
    with transport.xdisplay() as (x, xt, display):
        xt.XTestFakeButtonEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
        try:
            if not xt.XTestFakeButtonEvent(display, code, 1, 0):
                raise RuntimeError("XTest rejected button press.")
            x.XFlush(display)
            time.sleep(duration)
        finally:
            xt.XTestFakeButtonEvent(display, code, 0, 0)
            x.XFlush(display)


def main():
    if "hold" in sys.argv[1:]:
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument("--pid", type=int, required=True)
        parser.add_argument("action", choices=["hold"])
        parser.add_argument("button", choices=["left", "right"])
        parser.add_argument("seconds", type=float)
        args = parser.parse_args()
        try:
            hold(args.button, args.seconds, args.pid)
            return 0
        except (ValueError, RuntimeError, OSError) as error:
            print(f"Rebuild hold failed: {error}", file=sys.stderr)
            return 1
    transport.window = window
    transport.__doc__ = __doc__
    return transport.main()


if __name__ == '__main__':
    raise SystemExit(main())
