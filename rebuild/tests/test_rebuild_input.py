"""Headless C-window wheel driver checks; no compositor or real input calls."""
from contextlib import contextmanager
import json
from pathlib import Path
from types import SimpleNamespace
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import rebuild_input as tool


class RebuildScrollTests(unittest.TestCase):
    def setUp(self):
        original = tool.transport.window
        self.addCleanup(setattr, tool.transport, 'window', original)

    def test_bounds_reject_before_focus(self):
        with patch.object(tool.transport, 'focus') as focus:
            for value in (0, 101, -101, 1.5, True, float('nan')):
                with self.assertRaises(ValueError):
                    tool.scroll(value, 123)
            for pid in (None, 0, -1, True):
                with self.assertRaises(ValueError):
                    tool.scroll(1, pid)
            focus.assert_not_called()

    def test_failed_focus_never_injects(self):
        with patch.object(tool.transport, 'focus', side_effect=ValueError('No C window')), \
             patch.object(tool.transport, 'xdisplay') as display:
            with self.assertRaises(ValueError):
                tool.scroll(-2, 123)
            display.assert_not_called()

    def test_selector_excludes_original_and_requires_exact_pid_and_executable(self):
        clients = [dict(title='Planet Explorers', pid=456, address='0x456', at=[0,0], size=[1280,720]),
                   dict(title='Planet Explorers | C rebuild', pid=123, address='0x123', at=[0,0], size=[1280,720])]
        def resolve(path):
            return tool.EXECUTABLE if str(path) == '/proc/123/exe' else path
        with patch.object(tool.transport, 'run', return_value=json.dumps(clients)), patch.object(Path, 'resolve', resolve):
            self.assertEqual(tool.window(123)['pid'], 123)
            with self.assertRaises(ValueError):
                tool.window(456)
        with patch.object(tool.transport, 'run', return_value=json.dumps(clients)), patch.object(Path, 'resolve', lambda path:path):
            with self.assertRaises(ValueError):
                tool.window(123)

    def fake_display(self, events, reject=False):
        x = SimpleNamespace(XFlush=lambda display: None)
        xt = SimpleNamespace(XTestFakeButtonEvent=lambda display, code, down, delay: events.append((code, down)) or int(not reject))
        @contextmanager
        def display():
            yield x, xt, 123
        return display

    def test_signed_steps_emit_matching_press_release_pairs(self):
        for steps,code in ((2,4),(-3,5),(100,4)):
            events=[]
            with patch.object(tool.transport, 'focus') as focus, \
                 patch.object(tool.transport, 'xdisplay', self.fake_display(events)), patch.object(tool.time, 'sleep'):
                tool.scroll(steps, 123)
                focus.assert_called_once_with(123)
                self.assertIs(tool.transport.window, tool.window)
            self.assertEqual(events, [(code,1),(code,0)]*abs(steps))

    def test_interrupt_and_rejected_press_release_the_button(self):
        for reject in (False,True):
            events=[]
            with patch.object(tool.transport, 'focus'), \
                 patch.object(tool.transport, 'xdisplay', self.fake_display(events,reject)), \
                 patch.object(tool.time, 'sleep', side_effect=KeyboardInterrupt):
                with self.assertRaises(RuntimeError if reject else KeyboardInterrupt):
                    tool.scroll(-2, 123)
            self.assertEqual(events, [(5,1),(5,0)])

    def test_cli_keeps_negative_direction(self):
        with patch.object(tool, 'scroll') as scroll:
            self.assertEqual(tool.main(['--pid','123','scroll','-4']),0)
            scroll.assert_called_once_with(-4,123)


if __name__ == '__main__':
    unittest.main()
