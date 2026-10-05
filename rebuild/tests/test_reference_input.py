"""Headless contract tests: no compositor calls, focus, input, or screenshots."""
from contextlib import contextmanager
import importlib.util
import json
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch

TOOL = Path(__file__).resolve().parents[1] / 'tools/reference_input.py'
spec = importlib.util.spec_from_file_location('reference_input', TOOL)
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


def client(**kwargs):
    result = dict(title='Planet Explorers', pid=123, address='0xabc', at=[100, 50], size=[1280, 720])
    result.update(kwargs)
    return result


class ReferenceInputTests(unittest.TestCase):
    def test_only_unique_verified_original(self):
        clients = [client(), client(title='Planet Explorers C', pid=999)]
        with patch.object(tool, 'run', return_value=json.dumps(clients)), patch.object(tool, 'process_is_original', return_value=True):
            self.assertEqual(tool.window()['pid'], 123)
        with patch.object(tool, 'run', return_value=json.dumps([client()])), patch.object(tool, 'process_is_original', return_value=False):
            with self.assertRaises(ValueError):
                tool.window()
        with patch.object(tool, 'run', return_value=json.dumps([client(), client(pid=456)])), patch.object(tool, 'process_is_original', return_value=True):
            with self.assertRaises(ValueError):
                tool.window()
            self.assertEqual(tool.window(456)['pid'], 456)

    def test_reject_address_injection_and_invalid_rectangle(self):
        for invalid in [client(address='0xabc"})'), client(size=[0, 720]), client(at=['100', 0])]:
            with patch.object(tool, 'run', return_value=json.dumps([invalid])), patch.object(tool, 'process_is_original', return_value=True):
                with self.assertRaises(ValueError):
                    tool.window()

    def test_failed_focus_never_opens_xdisplay(self):
        with patch.object(tool, 'window', return_value=client()), patch.object(tool, 'dispatch'), \
             patch.object(tool.time, 'sleep'), patch.object(tool, 'run', return_value='{"address":"0xdef"}'), \
             patch.object(tool, 'xdisplay') as display:
            with self.assertRaises(ValueError):
                tool.send_key('F5', 0.1, None)
            display.assert_not_called()

    def test_stable_chords_and_bounds(self):
        self.assertEqual(tool.key_codes('Shift_L+w'), [50, 25])
        self.assertEqual(tool.key_codes('F5'), [71])
        self.assertEqual(tool.key_codes('Escape'), [9])
        for bad in ['Shift_L+Shift_L', 'F13', '', 'w++a']:
            with self.assertRaises(ValueError):
                tool.key_codes(bad)
        with patch.object(tool, 'focus') as focus:
            for bad in [0, -1, 5.01, float('nan'), float('inf')]:
                with self.assertRaises(ValueError):
                    tool.send_key('w', bad, None)
            with self.assertRaises(ValueError):
                tool.look(2001, 0, None)
            focus.assert_not_called()

    def test_keyboard_releases_reverse_order_on_interrupt(self):
        events = []
        x = SimpleNamespace(XFlush=lambda display: None)
        xt = SimpleNamespace(XTestFakeKeyEvent=lambda display, code, down, delay: events.append((code, down)) or 1)
        @contextmanager
        def display():
            yield x, xt, 123
        with patch.object(tool, 'focus'), patch.object(tool, 'xdisplay', display), patch.object(tool.time, 'sleep', side_effect=KeyboardInterrupt):
            with self.assertRaises(KeyboardInterrupt):
                tool.send_key('Shift_L+w', 0.1, None)
        self.assertEqual(events, [(50, 1), (25, 1), (25, 0), (50, 0)])

    def test_look_preserves_total_and_limits_each_step(self):
        deltas = []
        x = SimpleNamespace(XFlush=lambda display: None)
        xt = SimpleNamespace(XTestFakeRelativeMotionEvent=lambda display, dx, dy, delay: deltas.append((dx, dy)) or 1)
        @contextmanager
        def display():
            yield x, xt, 123
        with patch.object(tool, 'focus'), patch.object(tool, 'xdisplay', display), patch.object(tool.time, 'sleep'):
            tool.look(-57, 23, None)
        self.assertEqual(tuple(map(sum, zip(*deltas))), (-57, 23))
        self.assertTrue(all(max(abs(x), abs(y)) <= 20 for x, y in deltas))

    def test_monitor_scaling_rotation_and_negative_origin(self):
        monitors = [dict(x=-1920, y=0, width=1920, height=1080, scale=1),
                    dict(x=0, y=0, width=2160, height=3840, scale=2, transform=1),
                    dict(disabled=True)]
        self.assertEqual(tool.desktop_bounds(monitors), (-1920, 0, 3840, 1080))
        self.assertEqual(tool.click_coordinates(client(), 0, 0, (-1920, 0, 3840, 1080)),
                         (2020, 50, 3840, 1080, 2024))
        self.assertEqual(tool.click_coordinates(client(), 1279, 719, (-1920, 0, 3840, 1080))[-1], 3295)
        for x, y in [(-1, 0), (1280, 0), (0, -1), (0, 720)]:
            with self.assertRaises(ValueError):
                tool.click_coordinates(client(), x, y, (0, 0, 1920, 1080))
        with self.assertRaises(ValueError):
            tool.desktop_bounds([])

    def test_click_cli_passes_validated_jitter_and_button(self):
        monitors = [dict(x=0, y=0, width=1920, height=1080, scale=1)]
        with patch.object(tool, 'window', return_value=client()), patch.object(tool, 'focus', return_value=client()), \
             patch.object(tool, 'run', return_value=json.dumps(monitors)), \
             patch.object(tool, 'build_pointer', return_value=Path('/fake/helper')), patch.object(tool.subprocess, 'run') as execute:
            self.assertEqual(tool.main(['click', '40', '30', '--button', 'middle']), 0)
            execute.assert_called_once_with(['/fake/helper', '140', '80', '1920', '1080', '274', '136', '80'], check=True, timeout=6)

    def test_drag_cli_uses_both_client_local_endpoints_and_default_left_button(self):
        monitors = [dict(x=-1920, y=0, width=3840, height=1080, scale=1)]
        # A requery after focus accounts for a moved window before any input.
        moved = client(at=[200, 100])
        with patch.object(tool, 'window', return_value=client()), patch.object(tool, 'focus', return_value=moved), \
             patch.object(tool, 'run', return_value=json.dumps(monitors)), \
             patch.object(tool, 'build_pointer', return_value=Path('/fake/helper')), patch.object(tool.subprocess, 'run') as execute:
            self.assertEqual(tool.main(['drag', '806', '256', '360', '670']), 0)
            execute.assert_called_once_with(['/fake/helper', '2926', '356', '3840', '1080', '272',
                                            '2922', '356', '2480', '770'], check=True, timeout=6)

    def test_drag_rejects_either_endpoint_before_build_or_focus(self):
        monitors = [dict(x=0, y=0, width=1920, height=1080, scale=1)]
        with patch.object(tool, 'window', return_value=client()), patch.object(tool, 'focus') as focus, \
             patch.object(tool, 'run', return_value=json.dumps(monitors)), \
             patch.object(tool, 'build_pointer') as build, patch.object(tool.subprocess, 'run') as execute:
            for coordinates in [('0', '0', '1280', '0'), ('-1', '0', '10', '10'),
                                ('0', '0', '0', '720')]:
                self.assertEqual(tool.main(['drag', *coordinates]), 1)
            focus.assert_not_called()
            build.assert_not_called()
            execute.assert_not_called()

    def test_drag_revalidates_destination_after_focus_resize(self):
        monitors = [dict(x=0, y=0, width=1920, height=1080, scale=1)]
        with patch.object(tool, 'window', return_value=client()), \
             patch.object(tool, 'focus', return_value=client(size=[640, 480])), \
             patch.object(tool, 'run', return_value=json.dumps(monitors)), \
             patch.object(tool, 'build_pointer', return_value=Path('/fake/helper')), patch.object(tool.subprocess, 'run') as execute:
            self.assertEqual(tool.main(['drag', '10', '10', '360', '670', '--button', 'right']), 1)
            execute.assert_not_called()


if __name__ == '__main__':
    unittest.main()
