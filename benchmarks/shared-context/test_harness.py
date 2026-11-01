"""Portable behavior checks; native tracer integration is exercised separately."""
import contextlib
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import socket
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock


def load_script(name):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


runner = load_script('run')
analyzer = load_script('analyze')


def invoke(module, *args):
    with mock.patch.object(sys, 'argv', [module.__file__, *map(str, args)]):
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return module.main()


def varint(value):
    encoded = bytearray()
    while value > 127:
        encoded.append((value & 127) | 128)
        value >>= 7
    encoded.append(value)
    return bytes(encoded)


def integer_field(tag, value):
    return varint(tag << 3) + varint(value)


def bytes_field(tag, value):
    return varint((tag << 3) | 2) + varint(len(value)) + value


class HarnessChecks(unittest.TestCase):
    def fixture(self, root, batches=None, bursts=2):
        directory = root / 'steady' / 'r1-before-shared-t4'
        directory.mkdir(parents=True)
        result = [4 * bursts, 4, 4000, 6000, 58 * bursts, 1, 4096, 0, 0, 0]
        stats = dict(revision='before', mode='shared', workers=4,
                     result=list(map(str, result)), bursts=bursts, burst_calls=4,
                     before={'server': {'cpu_ticks': 0}},
                     after={'target': {'rss_kib': 4096},
                            'server': {'rss_kib': 4096, 'cpu_ticks': 1}})
        (directory / 'stats.json').write_text(json.dumps(stats))
        (directory / 'commands.jsonl').write_text('')
        if batches is not None:
            (directory / 'batches.json').write_text(json.dumps(batches))
        return directory

    def read_analysis(self, root):
        self.assertEqual(invoke(analyzer, root), 0)
        return json.loads((root / 'analysis' / 'analysis.json').read_text())

    @unittest.skipUnless(hasattr(socket, 'AF_UNIX'), 'requires Unix sockets')
    def test_private_socket_binds_despite_deep_result_path(self):
        with tempfile.TemporaryDirectory() as temporary:
            results = Path(temporary) / ('deep-results-' * 12)
            results.mkdir()
            self.assertGreater(len(os.fsencode(results / 'control.sock')), 108)
            with runner.control_socket_directory() as control:
                control_path = Path(control) / 'control.sock'
                self.assertLess(len(os.fsencode(control_path)), 104)
                self.assertEqual(os.stat(control).st_mode & 0o777, 0o700)
                with socket.socket(socket.AF_UNIX) as server:
                    server.bind(str(control_path))
            self.assertFalse(Path(control).exists())

    def test_private_socket_directory_cleans_up_on_error(self):
        with self.assertRaisesRegex(RuntimeError, 'injected'):
            with runner.control_socket_directory() as control:
                raise RuntimeError('injected failure')
        self.assertFalse(Path(control).exists())

    def test_changed_frozen_input_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            payload = root / 'target'
            payload.write_bytes(b'baseline')
            manifest = root / 'inputs.sha256'
            manifest.write_text(hashlib.sha256(payload.read_bytes()).hexdigest() + '  target\n')
            args = SimpleNamespace(root=root, identity_manifest=manifest)
            runner.verify_identity(args)
            payload.write_bytes(b'replaced')
            with self.assertRaisesRegex(RuntimeError, 'input changed'):
                runner.verify_identity(args)

    def test_existing_trial_is_rejected_before_any_target_launch(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            trial = root / 'results' / 'r2-before-shared-t4'
            trial.mkdir(parents=True)
            (trial / 'stats.json').write_text('{"old_count": 1}')
            with mock.patch.object(runner, 'trial') as launch:
                with self.assertRaises(SystemExit) as stopped:
                    invoke(runner, '--root', root, '--output', root / 'results',
                           '--rounds', 2, '--count', 999, '--cases', 'before:shared:4')
                self.assertEqual(stopped.exception.code, 2)
                launch.assert_not_called()

    def test_revision_paths_are_rejected_before_any_output_is_created(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = root / 'results'
            with mock.patch.object(runner, 'trial') as launch:
                for revision in ('../outside', 'name/../../../outside', '/tmp/outside', '.', '..'):
                    with self.subTest(revision=revision), self.assertRaises(SystemExit) as stopped:
                        invoke(runner, '--root', root, '--output', output,
                               '--cases', revision + ':ordinary:4')
                    self.assertEqual(stopped.exception.code, 2)
                    self.assertFalse(output.exists())
                launch.assert_not_called()

    def test_repeated_batches_do_not_become_independent_processes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            batches = [[4, 4, 1000, 2000, 58, 1, 4096, 0, 0, 0],
                       [4, 4, 3000, 4000, 58, 1, 4096, 0, 0, 0]]
            self.fixture(root, batches=batches)
            data = self.read_analysis(root)
            group = data['groups'][0]
            self.assertEqual(group['independent_processes'], 1)
            self.assertEqual(group['timed_batches'], 2)
            self.assertEqual(group['recorded_batch_samples'], 2)
            self.assertEqual(group['first_batch_wall_ns_per_call']['median'], 250)
            self.assertEqual(group['batch_wall_ns_per_call']['median'], 500)
            self.assertEqual(data['measurement_warnings'], [])

    def test_legacy_aggregate_keeps_count_without_inventing_first_sample(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.fixture(root, bursts=100)
            data = self.read_analysis(root)
            group = data['groups'][0]
            self.assertEqual(group['timed_batches'], 100)
            self.assertEqual(group['recorded_batch_samples'], 0)
            self.assertEqual(group['first_batch_wall_ns_per_call'], {})

    def test_single_batch_cohort_is_explicitly_flagged(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.fixture(root, bursts=1)
            data = self.read_analysis(root)
            self.assertEqual(len(data['measurement_warnings']), 1)

    def test_separate_output_preserves_input_and_rejects_overwrite(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            directory = self.fixture(root)
            original = (directory / 'stats.json').read_bytes()
            output = root / 'separate'
            self.assertEqual(invoke(analyzer, root, '--output', output), 0)
            generated = (output / 'analysis.json').read_bytes()
            with self.assertRaises(SystemExit):
                invoke(analyzer, root, '--output', output)
            self.assertEqual((output / 'analysis.json').read_bytes(), generated)
            self.assertEqual((directory / 'stats.json').read_bytes(), original)
            self.assertFalse((root / 'analysis.json').exists())
            with self.assertRaises(SystemExit):
                invoke(analyzer, root, '--output', root)

    def test_history_evictions_are_labeled_without_becoming_agent_loss(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            directory = self.fixture(root)
            body = integer_field(3, 1) + integer_field(15, 33) + integer_field(13, 57)
            (directory / 'events.pb').write_bytes(struct.pack('<I', len(body)) + body)
            data = self.read_analysis(root)
            self.assertEqual(data['trials'][0]['max_server_history_evictions'], 57)
            self.assertEqual(data['trials'][0]['max_agent_drop'], 0)
            self.assertEqual(data['trials'][0]['received'], 1)

    def test_incorrect_captured_final_state_fails_analysis(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            directory = self.fixture(root)
            stats = json.loads((directory / 'stats.json').read_text())
            stats['mode'] = 'small'
            (directory / 'stats.json').write_text(json.dumps(stats))
            (directory / 'detach.txt').write_text('probes { id: 1 symbol: "bench_root" }')
            payload = integer_field(1, 99)
            state = bytes_field(3, payload)
            event = bytes_field(7, state)
            envelope = bytes_field(4, event)
            body = integer_field(3, 1) + integer_field(15, 2) + bytes_field(20, envelope)
            (directory / 'events.pb').write_bytes(struct.pack('<I', len(body)) + body)
            self.assertEqual(invoke(analyzer, root), 1)
            data = json.loads((root / 'analysis' / 'analysis.json').read_text())
            self.assertEqual(len(data['validation_errors']), 1)


if __name__ == '__main__':
    unittest.main()
