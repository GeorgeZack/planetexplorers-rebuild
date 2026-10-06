"""Headless C-window pointer driver checks; no compositor or real input calls."""
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



class RebuildClickTests(unittest.TestCase):
    def setUp(self):
        original = tool.transport.window
        original_doc = tool.transport.__doc__
        self.addCleanup(setattr, tool.transport, 'window', original)
        self.addCleanup(setattr, tool.transport, '__doc__', original_doc)
        self.client = dict(title='Planet Explorers | C rebuild', pid=123,
                           address='0x123', at=[100, 200], size=[640, 480])
        monitors = json.dumps([dict(x=0, y=0, width=1920, height=1080, scale=1)])
        self.focus = self.enterContext(patch.object(tool.transport, 'focus', return_value=self.client))
        self.selector = self.enterContext(patch.object(tool, 'window', return_value=self.client))
        self.enterContext(patch.object(tool.transport, 'run', return_value=monitors))
        self.build = self.enterContext(patch.object(tool.transport, 'build_pointer', return_value=Path('/fake/pointer')))
        self.execute = self.enterContext(patch.object(tool.transport.subprocess, 'run'))
        self.xdisplay = self.enterContext(patch.object(tool.transport, 'xdisplay'))

    def test_click_uses_compositor_transport_and_native_selector(self):
        self.assertEqual(tool.main(['--pid', '123', 'click', '30', '40', '--button', 'right']), 0)
        self.assertIs(tool.transport.window, tool.window)
        self.selector.assert_called_once_with(123)
        self.focus.assert_called_once_with(123)
        # Logical desktop coordinates plus an in-client jitter, not XTest motion.
        self.execute.assert_called_once_with(
            ['/fake/pointer', '130', '240', '1920', '1080', '273', '126', '240'],
            check=True, timeout=6)
        self.xdisplay.assert_not_called()

    def test_invalid_coordinates_reject_before_focus_or_build(self):
        for xpos, ypos in ((-1, 0), (640, 0), (0, 480)):
            self.assertEqual(tool.main(['--pid', '123', 'click', str(xpos), str(ypos)]), 1)
        self.focus.assert_not_called()
        self.build.assert_not_called()
        self.execute.assert_not_called()
        self.xdisplay.assert_not_called()

    def test_failed_focus_or_resized_client_never_injects(self):
        self.focus.side_effect = ValueError('No focus')
        self.assertEqual(tool.main(['--pid', '123', 'click', '400', '200']), 1)
        self.focus.side_effect = None
        self.focus.return_value = dict(self.client, size=[320, 240])
        self.assertEqual(tool.main(['--pid', '123', 'click', '400', '200']), 1)
        self.execute.assert_not_called()
        self.xdisplay.assert_not_called()

    def test_post_focus_location_is_used(self):
        self.focus.return_value = dict(self.client, at=[300, 400])
        self.assertEqual(tool.main(['--pid', '123', 'click', '30', '40']), 0)
        self.execute.assert_called_once_with(
            ['/fake/pointer', '330', '440', '1920', '1080', '272', '326', '440'],
            check=True, timeout=6)

    def test_duration_override_is_rejected(self):
        with self.assertRaises(SystemExit) as stopped:
            tool.main(['--pid', '123', 'click', '30', '40', '--duration', '.7'])
        self.assertEqual(stopped.exception.code, 2)
        self.focus.assert_not_called()
        self.execute.assert_not_called()


if __name__ == '__main__':
    unittest.main()
