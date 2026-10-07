from pathlib import Path
import os
import select
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
out = root / 'rebuild/build/ui-details-captures'
out.mkdir(exist_ok=True)
read_fd, write_fd = os.pipe()
with (out / 'xvfb.log').open('w') as log:
    server = subprocess.Popen([str(root / 'rebuild/build/private-xvfb/usr/bin/Xvfb'), '-displayfd', str(write_fd), '-screen', '0', '1280x720x24', '-nolisten', 'tcp', '-noreset', '-ac'], pass_fds=(write_fd,), stdout=log, stderr=log)
    os.close(write_fd)
    try:
        if not select.select([read_fd], [], [], 15)[0]:
            raise RuntimeError('Private Xvfb did not announce its display')
        number = os.read(read_fd, 128).decode().strip()
        if not number.isdecimal():
            raise RuntimeError(f'Invalid private display: {number!r}')
        env = os.environ.copy()
        env.pop('WAYLAND_DISPLAY', None)
        env.update(DISPLAY=':' + number, PE_UI_PRIVATE_DISPLAY=':' + number, LIBGL_ALWAYS_SOFTWARE='1', GALLIUM_DRIVER='llvmpipe')
        print(f'Private Xvfb display :{number}', flush=True)
        with (out / 'render.log').open('w') as render_log:
            result = subprocess.run([str(root / 'rebuild/build/render_shop_details_ui'), str(out)], env=env, cwd=root, stdout=render_log, stderr=subprocess.STDOUT, timeout=180)
        print((out / 'render.log').read_text())
        sys.exit(result.returncode)
    finally:
        os.close(read_fd)
        server.terminate()
        try:
            server.wait(timeout=10)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait()
