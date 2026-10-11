#!/usr/bin/env python3
"""Create/repair the local UnityPy environment with relocatable entry points."""
import argparse
from pathlib import Path
import re
import venv

ROOT = Path(__file__).resolve().parents[2]
ENVIRONMENT = ROOT/'rebuild/build/unity-tools'
CONSOLE_HEADER = "#!/bin/sh\n'''exec' \"$(dirname -- \"$0\")/python\" \"$0\" \"$@\"\n' '''\n"
ACTIVATION_PREFIX = '''# Locate this sourced file in Bash or Zsh, rather than remembering its install path.
if [ -n "${ZSH_VERSION:-}" ]; then
    eval '_PE_UNITY_SCRIPT=${(%):-%x}'
elif [ -n "${BASH_VERSION:-}" ]; then
    _PE_UNITY_SCRIPT=${BASH_SOURCE[0]}
else
    printf '%s\\n' 'Use bin/python directly, or source activate in Bash/Zsh.' >&2
    return 1
fi
_PE_UNITY_ENV=$(CDPATH= cd -- "$(dirname -- "$_PE_UNITY_SCRIPT")/.." && pwd)
'''


def repair(environment):
    """Only generated environment launchers/config, never recovered content."""
    directory = environment/'bin'
    if not (directory/'python').is_file():
        raise ValueError('Python environment is missing; run make unity-tools')
    for path in directory.iterdir():
        if path.is_symlink() or not path.is_file():
            continue
        # Ignore any binary console tools an installed package may provide.
        with path.open('rb') as stream:
            first_bytes = stream.readline(4096)
        if not first_bytes.startswith(b'#!'):
            continue
        data = path.read_text()
        first, _, rest = data.partition('\n')
        if first.startswith('#!') and re.search(r'/python(?:[0-9.]*)$', first):
            path.write_text(CONSOLE_HEADER+rest)
    config = environment/'pyvenv.cfg'
    if config.exists():
        config.write_text(''.join(line for line in config.read_text().splitlines(True)
                                  if not line.startswith('command = ')))
    activation = directory/'activate'
    if activation.exists():
        data = activation.read_text()
        if not data.startswith('# Locate this sourced file'):
            data = re.sub(r'VIRTUAL_ENV=\$\(cygpath [^\n]+\)', 'VIRTUAL_ENV=$(cygpath "$_PE_UNITY_ENV")', data)
            data = re.sub(r'export VIRTUAL_ENV=[^\n]+', 'export VIRTUAL_ENV="$_PE_UNITY_ENV"', data)
            activation.write_text(ACTIVATION_PREFIX+data+'\nunset _PE_UNITY_ENV _PE_UNITY_SCRIPT\n')
    fish = directory/'activate.fish'
    if fish.exists():
        data = re.sub(r'set -gx VIRTUAL_ENV [^\n]+',
                      'set -gx VIRTUAL_ENV (path resolve (dirname (status filename))/..)', fish.read_text())
        fish.write_text(data)
    csh = directory/'activate.csh'
    if csh.exists():
        data = csh.read_text()
        if not data.startswith('# Pass the environment directory'):
            data = re.sub(r'setenv VIRTUAL_ENV [^\n]+', 'setenv VIRTUAL_ENV "$argv[1]"', data)
            # Csh does not expose a sourced file's directory. Accept it explicitly.
            csh.write_text('# Pass the environment directory as the argument when sourcing.\n'
                           'if ( $#argv == 0 ) then\n'
                           '    echo "Use bin/python directly, or source activate.csh ENVIRONMENT_DIRECTORY."\n'
                           'else\n'+data+'\nendif\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repair', action='store_true', help='Preserve installed packages; repair launchers only')
    args = parser.parse_args()
    if not args.repair:
        venv.EnvBuilder(with_pip=True, symlinks=True).create(ENVIRONMENT)
    repair(ENVIRONMENT)
    print('Unity tools entry points are relative to their environment.')


if __name__ == '__main__':
    main()
