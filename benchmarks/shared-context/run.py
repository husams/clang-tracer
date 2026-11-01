#!/usr/bin/env python3
"""Isolated native benchmark. Uses public CLI; never installs runtime artifacts."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import select
import signal
import struct
import subprocess as sp
import tempfile
import time

def control_socket_directory():
    """Keep the private Unix socket independent of the results directory length."""
    return tempfile.TemporaryDirectory(prefix='ct-reg-', dir='/tmp')

def verify_identity(args):
    if args.identity_manifest is None:
        return
    for line in args.identity_manifest.read_text().splitlines():
        expected, name = line.split(maxsplit=1)
        path = Path(name)
        if not path.is_absolute():
            path = args.root / path
        digest = hashlib.sha256()
        with path.open('rb') as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b''):
                digest.update(block)
        if digest.hexdigest() != expected:
            raise RuntimeError('benchmark input changed: '+str(path))

def proc_stats(pid):
    text = Path(f'/proc/{pid}/stat').read_text().split(') ', 1)[1].split()
    status = Path(f'/proc/{pid}/status').read_text()
    maps = Path(f'/proc/{pid}/maps').read_text()
    return dict(cpu_ticks=int(text[11])+int(text[12]),
                rss_kib=int(re.search(r'VmRSS:\s+(\d+)', status)[1]),
                hwm_kib=int(re.search(r'VmHWM:\s+(\d+)', status)[1]),
                threads=int(re.search(r'Threads:\s+(\d+)', status)[1]),
                mapping_count=len(maps.splitlines()),mapping_bytes=len(maps))

def frames(path):
    data = path.read_bytes() if path.exists() else b''
    values = []
    pos = 0
    while pos + 4 <= len(data):
        size = struct.unpack_from('<I', data, pos)[0]
        pos += 4
        if pos + size > len(data): raise RuntimeError('incomplete event frame')
        values.append(data[pos:pos+size]); pos += size
    if pos != len(data): raise RuntimeError('trailing event bytes')
    return values

def trial(args, directory, revision, mode, workers):
    verify_identity(args)
    directory.mkdir(parents=True)
    build = args.root / revision / 'build'
    if revision == 'installed':
        client = args.installed_prefix / 'bin/clang-tracer'
        server_binary = args.installed_prefix / 'bin/clang-tracer-server'
        agent = args.installed_prefix / args.installed_libdir / 'libclang_tracer_agent.so'
        proto = args.installed_prefix / 'share/clang-tracer/proto/tracer.proto'
    else:
        client, server_binary = build/'clang-tracer', build/'clang-tracer-server'
        agent, proto = build/'libclang_tracer_agent.so', args.root/revision/'proto/tracer.proto'
    (directory/'runtime-paths.json').write_text(json.dumps(
        {key: str(value.resolve()) for key, value in
         dict(client=client, server=server_binary, agent=agent, proto=proto).items()}, indent=2)+'\n')
    # Experiment directories can exceed Linux's Unix-domain socket path limit.
    socket_directory = control_socket_directory()
    endpoint = 'unix://' + str(Path(socket_directory.name) / 'control.sock')
    (directory/'endpoint.txt').write_text(endpoint+'\n')
    server = target = watcher = profiler = None
    attached = False
    command_log = (directory / 'commands.jsonl').open('w')
    logs = []
    def cli(*parts, check=True, timeout=90):
        command = [str(client), '--endpoint', endpoint, *map(str, parts)]
        begin = time.monotonic_ns()
        result = sp.run(command, capture_output=True, timeout=timeout)
        command_log.write(json.dumps(dict(argv=command, elapsed_ns=time.monotonic_ns()-begin,
                                         rc=result.returncode, stdout=result.stdout.decode(errors='replace'),
                                         stderr=result.stderr.decode(errors='replace')))+'\n')
        command_log.flush()
        if check and result.returncode:
            raise RuntimeError(str(command)+': '+result.stderr.decode(errors='replace'))
        return result
    def batch(count, stride=0):
        target.stdin.write(f'{count} {workers} {stride} {args.pace_us}\n'); target.stdin.flush()
        if not select.select([target.stdout], [], [], 120)[0]: raise RuntimeError('target timed out')
        row = target.stdout.readline().strip()
        (directory / 'target.log').open('a').write(row+'\n')
        values = row.split(',')
        if len(values) != 11 or values[0] != 'RESULT' or values[6] != '1':
            raise RuntimeError('invalid target result '+row)
        if stride:
            samples = target.stdout.readline().strip()
            if not samples.startswith('SAMPLES,'): raise RuntimeError(samples)
            (directory / 'latencies.csv').write_text(samples+'\n')
        return values[1:]
    try:
        target_log = (directory / 'target.err').open('w'); logs.append(target_log)
        target = sp.Popen(['taskset', '-c', '2' if workers == 1 else '2-5', str(args.root/'fixture/target')],
                          stdin=sp.PIPE, stdout=sp.PIPE, stderr=target_log, text=True, bufsize=1)
        ready = target.stdout.readline().strip()
        if ready != f'READY {target.pid}': raise RuntimeError(ready)
        (directory / 'pid').write_text(str(target.pid))
        baseline_memory = proc_stats(target.pid)
        server_log = (directory / 'server.log').open('w'); logs.append(server_log)
        command = ['taskset', '-c', '6,7', str(server_binary), '--agent',
                   str(agent), '--listen', endpoint, '--timeout', '60']
        if mode == 'durable': command += ['--context-durable-dir', str(directory/'journal')]
        server = sp.Popen(command, stdout=server_log, stderr=sp.STDOUT)
        for _ in range(100):
            if cli('list', check=False, timeout=3).returncode == 0: break
            if server.poll() is not None: raise RuntimeError('server exited')
            time.sleep(.05)
        else: raise RuntimeError('server not ready')
        callback_name = {'off':None, 'ordinary':'ordinary', 'stack':'ordinary', 'shared':'shared',
                         'scoped':'scoped', 'small':'small', 'nested':'nested',
                         'durable':'small', 'shared-stack':'small', 'blocked':'small',
                         'fresh':'shared'}.get(mode)
        probes = []
        if mode != 'off':
            cli('attach', target.pid); attached = True
            callback = args.root/'fixture'/f'{callback_name}.cpp'
            declarations = ['bench_root(int, int)', 'bench_leaf(int)']
            if mode == 'fresh': declarations = ['bench_leaf(int)']
            for declaration in declarations:
                extras = ['--stack', '16', '--stack-application-only', 'true'] if mode in ('stack','shared-stack') else []
                result = cli('trace', target.pid, declaration, '--callback', callback, *extras)
                probes.append(int(re.search(rb'probe=(\d+)', result.stdout)[1]))
            if revision == 'installed':
                mapped_paths = {line.split()[-1] for line in Path(f'/proc/{target.pid}/maps').read_text().splitlines()
                                if 'libclang_tracer_agent.so' in line}
                if mapped_paths != {str(agent.resolve())}:
                    raise RuntimeError('installed agent mapping mismatch: '+repr(mapped_paths))
                (directory/'mapped-agent.txt').write_text('\n'.join(sorted(mapped_paths))+'\n')
        batch(32)
        if mode in ('small', 'nested', 'durable', 'shared-stack'):
            time.sleep(.3)
            result = cli('events', target.pid, '--count', 1, '--timeout', 2, check=False)
            (directory/'readable-check.txt').write_bytes(result.stdout+result.stderr)
        time.sleep(.15)
        before = dict(target=proc_stats(target.pid), server=proc_stats(server.pid))
        count = args.count if mode != 'off' else args.baseline_count
        if args.latency and workers == 1: stride = max(1, count//512)
        else: stride = 0
        # Watch before the timed batch so retained-history bounds don't hide losses.
        if mode in ('ordinary','stack','small','nested','durable','shared-stack'):
            expected = (count*args.bursts+32+1+args.bursts)* (1 if mode in ('small','nested','durable','shared-stack') else 2)
            watch_log = (directory/'watch.err').open('w'); logs.append(watch_log)
            watcher = sp.Popen([str(client), '--endpoint', endpoint, '--format', 'protobuf',
                                '--output', str(directory/'events.pb'), 'events', str(target.pid),
                                '--count', str(expected), '--timeout', str(args.watch_timeout)], stdout=sp.DEVNULL, stderr=watch_log)
            time.sleep(.15)
        if mode == 'blocked': server.send_signal(signal.SIGSTOP)
        if args.profile_count:
            profiler = sp.Popen(['python3', str(args.root/'fixture/profile.py'), str(target.pid),
                                 str(directory), str(args.profile_count)])
        window_begin = time.monotonic_ns()
        batches = []
        for burst in range(args.bursts):
            batches.append(list(map(int,batch(count, stride))))
            if burst+1 < args.bursts: time.sleep(args.burst_interval_us/1e6)
        measurement_window_ns = time.monotonic_ns()-window_begin
        (directory/'batches.json').write_text(json.dumps(batches)+'\n')
        row = [str(sum(b[i] for b in batches)) if i in (0,2,3,4,7,8,9)
               else str(max(b[i] for b in batches)) for i in range(10)]
        if mode == 'blocked': server.send_signal(signal.SIGCONT)
        if profiler: profiler.wait(timeout=120)
        time.sleep(.25)
        after = dict(target=proc_stats(target.pid), server=proc_stats(server.pid))
        if watcher:
            watcher.wait(timeout=12)
        if mode == 'blocked':
            cli('--format', 'protobuf', '--output', directory/'events.pb', 'events', target.pid,
                '--count', '100000', '--timeout', '1', check=False)
        # Explicit repeated installation measures already-retained owner reuse.
        if mode == 'shared' and workers == 1 and args.reuse:
            cli('remove', target.pid, probes[-1])
            cli('trace', target.pid, 'bench_leaf(int)', '--callback', args.root/'fixture/shared.cpp')
        if attached:
            cli('--format', 'protobuf', '--output', directory/'detach.pb', 'detach', target.pid)
            attached = False
            data = frames(directory/'detach.pb')
            decoded = sp.run(['protoc', '-I'+str(proto.parent),
                              '--decode=clangtracer.v1.Session', str(proto)],
                             input=data[0], capture_output=True, check=True).stdout
            (directory/'detach.txt').write_bytes(decoded)
            if b'original_bytes_restored: true' not in decoded or b'callback_errors:' in decoded:
                raise RuntimeError('detach/callback correctness failure')
        batch(32)
        events = frames(directory/'events.pb')
        if events:
            (directory/'event-first.pb').write_bytes(events[0])
        verify_identity(args)
        stats = dict(revision=revision, mode=mode, workers=workers, result=row,
                     batch_samples=len(batches),
                     bursts=args.bursts,burst_calls=count,measurement_window_ns=measurement_window_ns,
                     baseline_memory=baseline_memory, before=before, after=after,
                     event_count=len(events), event_bytes=sum(map(len,events)),
                     watch_rc=watcher.returncode if watcher else None)
        (directory/'stats.json').write_text(json.dumps(stats,indent=2)+'\n')
        return stats
    finally:
        if server and server.poll() is None: server.send_signal(signal.SIGCONT)
        if attached:
            try: cli('detach', target.pid)
            except Exception: pass
        if watcher and watcher.poll() is None: watcher.terminate(); watcher.wait()
        if profiler and profiler.poll() is None: profiler.terminate(); profiler.wait()
        if target and target.poll() is None:
            target.stdin.write('0 1 0 0\n'); target.stdin.flush()
            try: target.wait(timeout=5)
            except sp.TimeoutExpired: target.terminate(); target.wait()
        if server and server.poll() is None: server.terminate(); server.wait(timeout=10)
        for log in logs: log.close()
        command_log.close()
        socket_directory.cleanup()

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--root', type=Path, required=True)
    p.add_argument('--installed-prefix', type=Path, default=Path('/usr/local'))
    p.add_argument('--installed-libdir', default='lib64')
    p.add_argument('--identity-manifest', type=Path)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--rounds', type=int, default=5)
    p.add_argument('--count', type=int, default=2048)
    p.add_argument('--baseline-count', type=int, default=5000000)
    p.add_argument('--cases', required=True)
    p.add_argument('--latency', action='store_true')
    p.add_argument('--reuse', action='store_true')
    p.add_argument('--pace-us', type=int, default=0)
    p.add_argument('--profile-count', type=int, default=0)
    p.add_argument('--bursts', type=int, default=1)
    p.add_argument('--burst-interval-us', type=int, default=20000)
    p.add_argument('--watch-timeout', type=int, default=8)
    args=p.parse_args(); args.output=args.output.resolve()
    if args.rounds<1 or args.bursts<1 or args.watch_timeout<1:
        p.error('rounds, bursts and watch timeout must be positive')
    if not (1<=args.count<=100000000 and 1<=args.baseline_count<=100000000):
        p.error('call counts must be between 1 and 100000000')
    if min(args.pace_us,args.profile_count,args.burst_interval_us)<0:
        p.error('pacing, profile count and burst interval must be nonnegative')
    try:
        cases=[(a,b,int(c)) for a,b,c in (s.split(':') for s in args.cases.split(','))]
    except ValueError:
        p.error('cases must be source-directory:mode:workers')
    modes={'off','ordinary','stack','shared','scoped','small','nested','durable','shared-stack','blocked','fresh'}
    if any(not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', revision)
           or mode not in modes or not 1<=workers<=8 for revision,mode,workers in cases):
        p.error('cases need a simple source-directory name, known mode and 1–8 workers')
    if len(set(cases))!=len(cases):
        p.error('duplicate cases would reuse the same trial directory')
    for round in range(args.rounds):
        for revision,mode,workers in cases:
            directory=args.output/f'r{round+1}-{revision}-{mode}-t{workers}'
            if directory.exists():
                p.error('trial already exists; choose a fresh output directory: '+str(directory))
    args.output.mkdir(parents=True,exist_ok=True)
    for round in range(args.rounds):
        order=cases[round%len(cases):]+cases[:round%len(cases)]
        if round%2: order=list(reversed(order))
        for revision,mode,workers in order:
            label=f'r{round+1}-{revision}-{mode}-t{workers}'
            directory=args.output/label
            stats=trial(args,directory,revision,mode,workers)
            print(label, json.dumps(stats),flush=True)

if __name__=='__main__': main()
