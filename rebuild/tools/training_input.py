#!/usr/bin/env python3
"""Bounded keyboard, pointer and capture driver for the local C training window.

Uses reference_input's input transport, with a separate exact-title and exact-
executable selector. The original reference driver's selector is unchanged.
Commands: --pid PID layout, shot FILE.png, key CHORD, click X Y, drag X Y X2 Y2.
"""
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
        if (client.get('title') != 'Planet Explorers | Training Room'
                or type(candidate) is not int or candidate <= 0
                or (pid is not None and candidate != pid)):
            continue
        if Path(f'/proc/{candidate}/exe').resolve() == EXECUTABLE.resolve():
            matches.append(client)
    if len(matches) != 1:
        raise ValueError('Expected one local C training window; specify --pid.')
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


def main():
    transport.window = window
    transport.__doc__ = __doc__
    return transport.main()


if __name__ == '__main__':
    raise SystemExit(main())
