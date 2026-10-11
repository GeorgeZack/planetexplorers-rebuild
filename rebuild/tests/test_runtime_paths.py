"""Exercise the actual C path discovery and Python launchers after a move."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import venv

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'rebuild/tools'))
import setup_unity_tools


class RuntimePaths(unittest.TestCase):
    @unittest.skipUnless(shutil.which('cc'), 'C compiler unavailable')
    def test_native_paths_follow_the_relocated_binary_from_an_unrelated_cwd(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            old = base/'old checkout'
            binary_dir = old/'rebuild/build'
            binary_dir.mkdir(parents=True)
            (old/'rebuild/Makefile').write_text('all:\n')
            source = base/'probe.c'
            source.write_text('''#include "pe_paths.h"
int main(void) {
    char assets[4096],root[4096],small[2];
    if(!pe_paths_default_assets(assets,sizeof assets)||!pe_paths_project_root(root,sizeof root))return 1;
    if(pe_paths_default_assets(small,sizeof small)||pe_paths_project_root(small,sizeof small))return 2;
    puts(assets);puts(root);return 0;
}
''')
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                            '-I'+str(ROOT/'rebuild/src'), str(source), '-o', str(binary_dir/'probe')], check=True)
            new = base/'relocated checkout'
            old.rename(new)
            result = subprocess.run([str(new/'rebuild/build/probe')], cwd=base, text=True,
                                    capture_output=True, check=True)
            self.assertEqual(result.stdout.splitlines(), [str(new/'rebuild/build/reference-assets'), str(new)])

    def test_console_launcher_follows_environment_after_move(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            old = base/'old environment'
            venv.EnvBuilder(with_pip=False, symlinks=True).create(old)
            binary_dir = old/'bin'
            console = binary_dir/'example'
            console.write_text('#!/absent/old/environment/bin/python\nimport sys\nprint(sys.prefix)\nprint(sys.argv[1])\n')
            console.chmod(0o755)
            setup_unity_tools.repair(old)
            new = base/'moved environment'
            old.rename(new)
            result = subprocess.run([str(new/'bin/example'), 'exact argument'], cwd=base,
                                    text=True, capture_output=True, check=True)
            self.assertEqual(result.stdout.splitlines(), [str(new), 'exact argument'])
            for shell in ('bash', 'zsh'):
                if not shutil.which(shell):
                    continue
                with self.subTest(shell=shell):
                    result = subprocess.run([shell, '-c',
                                             'source "$1"; python -c \'import sys; print(sys.prefix)\' ',
                                             'activation-test', str(new/'bin/activate')], cwd=base,
                                            text=True, capture_output=True, check=True)
                    self.assertEqual(result.stdout.strip(), str(new))


if __name__ == '__main__':
    unittest.main()
