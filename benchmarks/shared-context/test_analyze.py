"""Completeness and accounting checks for the public benchmark analyzer."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest import mock


def load_analyzer():
    path = Path(__file__).with_name('analyze.py')
    spec = importlib.util.spec_from_file_location('benchmark_analyze', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


analyzer = load_analyzer()


def invoke(root):
    with mock.patch.object(sys, 'argv', [analyzer.__file__, str(root)]):
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return analyzer.main()


def varint(value):
    output = bytearray()
    while value > 127:
        output.append((value & 127) | 128)
        value >>= 7
    output.append(value)
    return bytes(output)


def integer_field(tag, value):
    return varint(tag << 3) + varint(value)


def event(probe, call_id):
    return integer_field(3, probe) + integer_field(15, call_id)


class AnalyzerChecks(unittest.TestCase):
    def trial(self, root, mode='small', *, batches=None, bursts=1):
        directory = root / 'steady' / 'r1-before-t1'
        directory.mkdir(parents=True)
        result = [4 * bursts, 1, 4000 * bursts, 2000 * bursts,
                  58 * bursts, 1, 4096, 0, 0, 0]
        stats = dict(revision='before', mode=mode, workers=1,
                     result=list(map(str, result)), bursts=bursts,
                     burst_calls=4, batch_samples=bursts,
                     before={'server': {'cpu_ticks': 0}},
                     after={'target': {'rss_kib': 4096},
                            'server': {'rss_kib': 4096, 'cpu_ticks': 1}},
                     event_count=0, event_bytes=0)
        if mode in ('ordinary', 'stack'):
            stats['watch_rc'] = 0
        (directory / 'stats.json').write_text(json.dumps(stats))
        (directory / 'commands.jsonl').write_text('')
        if mode != 'off':
            (directory / 'detach.txt').write_text(
                'probes {\n id: 1\n symbol: "bench_root"\n original_bytes_restored: true\n}\n'
                'probes {\n id: 2\n symbol: "bench_leaf"\n original_bytes_restored: true\n}\n')
        if mode in analyzer.EMITTING_MODES:
            (directory / 'events.pb').write_bytes(b'')
        if batches is not None:
            stats['batch_samples'] = len(batches)
            (directory / 'batches.json').write_text(json.dumps(batches))
            (directory / 'stats.json').write_text(json.dumps(stats))
        return directory

    def analysis(self, root):
        return json.loads((root / 'analysis' / 'analysis.json').read_text())

    def test_empty_root_fails(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.assertEqual(invoke(root), 1)
            self.assertTrue(any('no completed trials' in error
                                for error in self.analysis(root)['validation_errors']))

    def test_started_trial_without_stats_fails(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            partial = root / 'steady' / 'r1-before-small-t1'
            partial.mkdir(parents=True)
            (partial / 'runtime-paths.json').write_text('{}')
            self.assertEqual(invoke(root), 1)
            self.assertTrue(any('no stats.json' in error
                                for error in self.analysis(root)['validation_errors']))

    def test_missing_snapshot_event_file_fails(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            directory = self.trial(root, mode='small')
            (directory / 'events.pb').unlink()
            self.assertEqual(invoke(root), 1)
            self.assertTrue(any('missing events.pb' in error
                                for error in self.analysis(root)['validation_errors']))

    def test_empty_snapshot_file_is_valid_best_effort_overload(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.trial(root, mode='small')
            self.assertEqual(invoke(root), 0)
            trial = self.analysis(root)['trials'][0]
            self.assertEqual(trial['measured_snapshots'], 0)
            self.assertEqual(trial['capture_fraction'], 0)

    def test_traced_trial_requires_detach_restoration_evidence(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            directory = self.trial(root, mode='shared')
            (directory / 'detach.txt').unlink()
            self.assertEqual(invoke(root), 1)
            self.assertTrue(any('missing detach.txt' in error
                                for error in self.analysis(root)['validation_errors']))

    def test_every_detached_probe_must_confirm_restoration(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            directory = self.trial(root, mode='shared')
            (directory / 'detach.txt').write_text(
                'probes {\n id: 1\n symbol: "bench_root"\n original_bytes_restored: true\n}\n'
                'probes {\n id: 2\n symbol: "bench_leaf"\n original_bytes_restored: false\n}\n')
            self.assertEqual(invoke(root), 1)
            self.assertTrue(any('each detached probe' in error
                                for error in self.analysis(root)['validation_errors']))

    def test_reinstalled_probe_history_is_allowed_when_all_restored(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            directory = self.trial(root, mode='shared')
            detach = directory / 'detach.txt'
            detach.write_text(detach.read_text() +
                'probes {\n id: 3\n symbol: "bench_leaf"\n original_bytes_restored: true\n}\n')
            self.assertEqual(invoke(root), 0)

    def test_ordinary_shortfall_and_nonzero_watcher_fail(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            directory = self.trial(root, mode='ordinary')
            # Ten events are expected: four leaves and one root, each PRE/POST.
            payloads = [event(1, 2), event(1, 2)]
            payloads.extend(event(2, call_id) for call_id in (33, 33, 34, 34, 35, 35, 36))
            (directory / 'events.pb').write_bytes(b''.join(
                struct.pack('<I', len(payload)) + payload for payload in payloads))
            stats = json.loads((directory / 'stats.json').read_text())
            stats['event_count'] = len(payloads)
            stats['event_bytes'] = sum(map(len, payloads))
            stats['watch_rc'] = 1
            (directory / 'stats.json').write_text(json.dumps(stats))
            self.assertEqual(invoke(root), 1)
            errors = self.analysis(root)['validation_errors']
            self.assertTrue(any('expected ordinary events' in error for error in errors))
            self.assertTrue(any('watcher exited' in error for error in errors))

    def test_complete_ordinary_pre_post_stream_passes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            directory = self.trial(root, mode='ordinary')
            payloads = [event(1, 2), event(1, 2)]
            for call_id in (33, 34, 35, 36):
                payloads.extend((event(2, call_id), event(2, call_id)))
            (directory / 'events.pb').write_bytes(b''.join(
                struct.pack('<I', len(payload)) + payload for payload in payloads))
            stats = json.loads((directory / 'stats.json').read_text())
            stats['event_count'] = len(payloads)
            stats['event_bytes'] = sum(map(len, payloads))
            (directory / 'stats.json').write_text(json.dumps(stats))
            self.assertEqual(invoke(root), 0)

    def test_repeated_latency_rows_feed_trial_and_pooled_distributions(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            directory = self.trial(root, mode='small')
            (directory / 'latencies.csv').write_text('SAMPLES,10,20\nSAMPLES,30\n')
            self.assertEqual(invoke(root), 0)
            data = self.analysis(root)
            trial = data['trials'][0]
            group = data['groups'][0]
            self.assertEqual(trial['latency_ns']['n'], 3)
            self.assertEqual(group['pooled_latency_ns']['n'], 3)

    def test_missing_legacy_batch_details_remain_supported(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            directory = self.trial(root, mode='shared', bursts=100)
            stats = json.loads((directory / 'stats.json').read_text())
            stats.pop('batch_samples')
            (directory / 'stats.json').write_text(json.dumps(stats))
            self.assertEqual(invoke(root), 0)
            group = self.analysis(root)['groups'][0]
            self.assertEqual(group['timed_batches'], 100)
            self.assertEqual(group['recorded_batch_samples'], 0)
            self.assertEqual(group['first_batch_wall_ns_per_call'], {})

    def test_batch_details_must_match_aggregate(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            directory = self.trial(root, mode='shared', batches=[
                [4, 1, 1000, 500, 58, 1, 4096, 0, 0, 0]])
            self.assertEqual(invoke(root), 1)
            self.assertTrue(any('aggregate disagrees' in error
                                for error in self.analysis(root)['validation_errors']))

    def test_truncated_event_frame_is_rejected_without_assertions(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'events.pb'
            path.write_bytes(struct.pack('<I', 20) + b'bad')
            with self.assertRaisesRegex(ValueError, 'incomplete event frame'):
                analyzer.frames(path)


if __name__ == '__main__':
    unittest.main()
