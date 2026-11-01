#!/usr/bin/env python3
import argparse
from collections import defaultdict
import csv
import json
from pathlib import Path
import re
import statistics as st
import struct

def varint(data,pos):
    value=shift=0
    for index in range(10):
        if pos>=len(data):raise ValueError('truncated protobuf varint')
        b=data[pos];pos+=1
        if index==9 and b>1:raise ValueError('protobuf varint exceeds 64 bits')
        value|=(b&127)<<shift
        if b<128:return value,pos
        shift+=7
    raise ValueError('protobuf varint exceeds 10 bytes')

def fields(data):
    result=defaultdict(list);pos=0
    while pos<len(data):
        key,pos=varint(data,pos); tag,wire=key>>3,key&7
        if tag==0:raise ValueError('protobuf field number is zero')
        if wire==0:value,pos=varint(data,pos)
        elif wire==2:
            size,pos=varint(data,pos)
            if size>len(data)-pos:raise ValueError('truncated protobuf field')
            value=data[pos:pos+size];pos+=size
        elif wire in (1,5):
            size=8 if wire==1 else 4
            if size>len(data)-pos:raise ValueError('truncated protobuf fixed-width field')
            value=data[pos:pos+size];pos+=size
        else:raise ValueError(wire)
        result[tag].append(value)
    return result

def frames(path):
    data=path.read_bytes() if path.exists() else b'';pos=0;rows=[]
    while pos<len(data):
        if len(data)-pos<4:raise ValueError(f'{path}: truncated frame length')
        size=struct.unpack_from('<I',data,pos)[0];pos+=4
        if size>len(data)-pos:raise ValueError(f'{path}: incomplete event frame')
        rows.append(fields(data[pos:pos+size]));pos+=size
    return rows

def latency_samples(path):
    if not path.exists():return []
    samples=[]
    for number,line in enumerate(path.read_text().splitlines(),1):
        values=line.strip().split(',')
        if not values or values[0]!='SAMPLES':
            raise ValueError(f'{path}: invalid latency row {number}')
        try:samples.extend(map(int,values[1:]))
        except ValueError as error:raise ValueError(f'{path}: invalid latency value in row {number}') from error
    return samples

EMITTING_MODES={'ordinary','stack','small','nested','durable','shared-stack','blocked'}
TRACED_MODES=EMITTING_MODES|{'shared','scoped','fresh'}
SUM_BATCH_FIELDS={0,2,3,4,7,8,9}

def discover_trials(root,errors):
    trials=[]
    for path in sorted(root.glob('*/*')):
        if not path.is_dir() or path.parent.name.startswith('pilot'):continue
        stats_path=path/'stats.json'
        if stats_path.is_file():
            trials.append(stats_path)
        elif any((path/name).exists() for name in
                 ('runtime-paths.json','pid','commands.jsonl','detach.txt')):
            errors.append(f'{path}: started trial has no stats.json')
    if not trials and not errors:errors.append(f'{root}: no completed trials found')
    return trials

def message_blocks(text,name):
    blocks=[]
    for match in re.finditer(r'\b'+re.escape(name)+r'\s*\{',text):
        start=match.end();depth=1;pos=start
        while pos<len(text) and depth:
            if text[pos]=='{':depth+=1
            elif text[pos]=='}':depth-=1
            pos+=1
        if depth:return blocks,False
        blocks.append(text[start:pos-1])
    return blocks,True

def validate_batches(stats,result,directory,errors):
    bursts=stats.get('bursts')
    calls_per_batch=stats.get('burst_calls')
    if bursts is not None and calls_per_batch is not None:
        try:
            if result[0]!=int(bursts)*int(calls_per_batch):
                errors.append(f'{directory}: aggregate call count disagrees with bursts')
        except (TypeError,ValueError):
            errors.append(f'{directory}: invalid burst metadata')
    path=directory/'batches.json'
    if not path.exists():return
    try:
        batches=json.loads(path.read_text())
        if not isinstance(batches,list) or not batches:
            raise ValueError('batch list must be nonempty')
        for index,batch in enumerate(batches):
            if not isinstance(batch,list) or len(batch)!=10:
                raise ValueError(f'batch {index} must contain ten result fields')
            batch[:]=list(map(int,batch))
            if batch[0]<=0 or batch[2]<=0 or batch[3]<0 or batch[5]!=1:
                errors.append(f'{directory}: invalid target result in batch {index}')
            if calls_per_batch is not None and batch[0]!=int(calls_per_batch):
                errors.append(f'{directory}: batch {index} call count disagrees with burst_calls')
            if batch[1]!=int(stats['workers']):
                errors.append(f'{directory}: batch {index} worker count disagrees with stats')
        expected_batches=stats.get('batch_samples',bursts)
        if expected_batches is not None and len(batches)!=int(expected_batches):
            errors.append(f'{directory}: batches.json count disagrees with stats')
        aggregate=[sum(batch[i] for batch in batches) if i in SUM_BATCH_FIELDS
                   else max(batch[i] for batch in batches) for i in range(10)]
        if aggregate!=result:
            errors.append(f'{directory}: batches.json aggregate disagrees with stats result')
    except (ValueError,TypeError,KeyError,json.JSONDecodeError) as error:
        errors.append(f'{directory}: invalid batches.json: {error}')

def validate_trial_files(stats,mode,directory,events,errors):
    event_path=directory/'events.pb'
    if mode in EMITTING_MODES and not event_path.is_file():
        errors.append(f'{directory}: missing events.pb for emitting mode {mode}')
    detach_path=directory/'detach.txt'
    if mode in TRACED_MODES:
        if not detach_path.is_file():
            errors.append(f'{directory}: missing detach.txt for traced mode {mode}')
        else:
            detached=detach_path.read_text()
            probe_blocks,complete=message_blocks(detached,'probes')
            expected_probes=1 if mode=='fresh' else 2
            if not complete:errors.append(f'{directory}: malformed detached probe block')
            if len(probe_blocks)<expected_probes:
                errors.append(f'{directory}: expected at least {expected_probes} detached probes, found {len(probe_blocks)}')
            if any('original_bytes_restored: true' not in block for block in probe_blocks):
                errors.append(f'{directory}: each detached probe must confirm byte restoration')
            callback_errors=list(map(int,re.findall(r'callback_errors:\s*(\d+)',detached)))
            if any(callback_errors):errors.append(f'{directory}: detach evidence reports callback errors')
    if event_path.is_file():
        if 'event_count' in stats and int(stats['event_count'])!=len(events):
            errors.append(f'{directory}: event_count disagrees with events.pb')
        if 'event_bytes' in stats:
            actual_bytes=event_path.stat().st_size-4*len(events)
            if int(stats['event_bytes'])!=actual_bytes:
                errors.append(f'{directory}: event_bytes disagrees with events.pb')

def quantile(values,q):
    values=sorted(values)
    return values[max(0,min(len(values)-1,int((len(values)-1)*q)))] if values else None

def distribution(values):
    if not values:return {}
    mean=st.mean(values)
    return dict(n=len(values),median=st.median(values),min=min(values),max=max(values),
                p95=quantile(values,.95),p99=quantile(values,.99),
                cv_percent=st.stdev(values)/mean*100 if len(values)>1 and mean else 0)

def main():
    p=argparse.ArgumentParser();p.add_argument('root',type=Path)
    p.add_argument('--output',type=Path);a=p.parse_args()
    output=a.output or a.root/'analysis'
    if output.resolve()==a.root.resolve():
        p.error('analysis output must differ from the input directory')
    if any((output/name).exists() for name in ('analysis.json','samples.csv')):
        p.error('analysis output already exists; choose a fresh output directory')
    output.mkdir(parents=True,exist_ok=True)
    rows=[];groups=defaultdict(list);cold=defaultdict(list);pooled_latency=defaultdict(list);errors=[]
    for path in discover_trials(a.root,errors):
        campaign=path.parent.parent.name
        if campaign.startswith('pilot'):continue
        s=json.loads(path.read_text());d=path.parent
        rev,mode,workers=s['revision'],s['mode'],s['workers'];v=list(map(int,s['result']))
        if len(v)!=10:
            errors.append(f'{d}: result must contain ten fields')
            continue
        n,_,wall,cpu,checksum,valid,rss,faults,vol,ivol=v
        if n<=0 or wall<=0 or valid!=1:
            errors.append(f'{d}: invalid aggregate target result')
        validate_batches(s,v,d,errors)
        events_path=d/'events.pb'
        events=frames(events_path)
        validate_trial_files(s,mode,d,events,errors)
        snapshots=[f for f in events if f.get(20)]
        detached=(d/'detach.txt').read_text() if (d/'detach.txt').exists() else ''
        probe_map={int(i):symbol for i,symbol in re.findall(r'probes \{\s+id: (\d+).*?symbol: "([^"]+)"',detached,re.S)}
        def measured(f):
            probe=f[3][0]; call=f.get(15,[0])[0]
            return call>(1 if 'root' in probe_map.get(probe,'') else 32)
        measured_events=[f for f in events if measured(f)]
        measured_snapshots=[f for f in snapshots if measured(f)]
        times=[f.get(8,[0])[0] for f in measured_snapshots]
        gaps=[(b-a)/1e6 for a,b in zip(times,times[1:])]
        root_calls=[]
        for f in measured_snapshots:
            if 'root' not in probe_map.get(f[3][0],''):continue
            envelope=fields(f[20][0]);event=fields(envelope[4][0])
            for state_data in event.get(7,[]):
                state=fields(state_data);payload=fields(state[3][0]);root_calls.extend(payload.get(1,[]))
        expected_root_calls=2*(s.get('burst_calls',n)+1)
        if root_calls and root_calls[-1]!=expected_root_calls:
            errors.append(f'{d}: final shared calls {root_calls[-1]} expected {expected_root_calls}')
        ordinary=mode in ('ordinary','stack')
        roots=s.get('bursts',1)
        expected=2*(n+roots) if ordinary else (n+roots if mode in ('small','nested','durable','shared-stack','blocked') else 0)
        if ordinary:
            if s.get('watch_rc') is not None and int(s['watch_rc'])!=0:
                errors.append(f'{d}: ordinary event watcher exited with {s["watch_rc"]}')
            if len(measured_events)!=expected:
                errors.append(f'{d}: received {len(measured_events)} of {expected} expected ordinary events')
            if max([f.get(12,[0])[0] for f in events]+[0])>0:
                errors.append(f'{d}: ordinary events report agent-side drops')
        lat=latency_samples(d/'latencies.csv')
        pooled_latency[(campaign,rev,mode,workers)].extend(lat)
        commands=[json.loads(line) for line in (d/'commands.jsonl').read_text().splitlines()]
        batch_details_saved=(d/'batches.json').exists()
        batches=json.loads((d/'batches.json').read_text()) if batch_details_saved else [v]
        known_batch_count=len(batches) if batch_details_saved else int(s.get('batch_samples',s.get('bursts',1)))
        trace=[r['elapsed_ns']/1e9 for r in commands if 'trace' in r['argv']]
        for i,seconds in enumerate(trace):cold[(campaign,rev,mode,workers,i)].append(seconds)
        item=dict(campaign=campaign,trial=d.name,revision=rev,mode=mode,workers=workers,calls=n,
                  wall_ns_per_call=wall/n,cpu_ns_per_call=cpu/n,throughput_calls_s=n*1e9/wall,
                  wall_ns=wall,cpu_ns=cpu,minor_faults=faults,
                  measurement_window_ns=s.get('measurement_window_ns'),
                  target_rss_kib=s['after']['target']['rss_kib'],peak_rss_kib=rss,
                  server_rss_kib=s['after']['server']['rss_kib'],
                  server_cpu_delta_ticks=s['after']['server']['cpu_ticks']-s['before']['server']['cpu_ticks'],
                  received=len(events),measured_received=len(measured_events),expected_measured=expected,
                  measured_snapshots=len(measured_snapshots),final_state_calls=root_calls,
                  capture_fraction=len(measured_snapshots)/expected if expected and not ordinary else None,
                  gap_ms=distribution(gaps),latency_ns=distribution(lat),trace_seconds=trace,
                  max_agent_drop=max([f.get(12,[0])[0] for f in events]+[0]),
                  max_server_drop=max([f.get(13,[0])[0] for f in events]+[0]))
        item['timed_batches']=known_batch_count
        item['batch_wall_ns_per_call']=[batch[2]/batch[0] for batch in batches] if batch_details_saved or known_batch_count==1 else []
        item['batch_cpu_ns_per_call']=[batch[3]/batch[0] for batch in batches] if batch_details_saved or known_batch_count==1 else []
        # Wire field 13 counts retained-history evictions, not live-stream loss.
        item['max_server_history_evictions']=item['max_server_drop']
        rows.append(item);groups[(campaign,rev,mode,workers)].append(item)
    summary=[];measurement_warnings=[]
    for key,items in sorted(groups.items()):
        row=dict(zip(['campaign','revision','mode','workers'],key));row['trials']=len(items)
        for metric in ['wall_ns_per_call','cpu_ns_per_call','throughput_calls_s','target_rss_kib','peak_rss_kib',
                       'server_rss_kib','received','measured_received','measured_snapshots','capture_fraction']:
            row[metric]=distribution([v[metric] for v in items if v[metric] is not None])
        row['pooled_latency_ns']=distribution(pooled_latency[key])
        row['independent_processes']=len(items)
        row['timed_batches']=sum(item['timed_batches'] for item in items)
        row['recorded_batch_samples']=sum(len(item['batch_wall_ns_per_call']) for item in items)
        row['batch_wall_ns_per_call']=distribution([value for item in items for value in item['batch_wall_ns_per_call']])
        row['first_batch_wall_ns_per_call']=distribution([item['batch_wall_ns_per_call'][0] for item in items if item['batch_wall_ns_per_call']])
        if (key[0] in ('steady','spawn','requests') and key[2] in ('ordinary','shared') and
                all(item['timed_batches']==1 for item in items)):
            measurement_warnings.append(dict(campaign=key[0],revision=key[1],mode=key[2],workers=key[3],
                reason='Single timed batch per process: measure within-process variability and first-request latency separately before attributing small differences to a version.'))
        summary.append(row)
    cold_rows=[dict(campaign=k[0],revision=k[1],mode=k[2],workers=k[3],install_index=k[4],seconds=distribution(v)) for k,v in sorted(cold.items())]
    excluded=[]
    for path in sorted(a.root.glob('*/*/detach.txt')):
        if path.parent.parent.name.startswith('pilot') or (path.parent/'stats.json').exists():continue
        counts=list(map(int,re.findall(r'callback_errors:\s*(\d+)',path.read_text())))
        if counts:excluded.append(dict(trial=str(path.parent),reason='callback failure gate',callback_errors=counts))
    (output/'analysis.json').write_text(json.dumps(dict(groups=summary,trials=rows,activation=cold_rows,validation_errors=errors,excluded_trials=excluded,measurement_warnings=measurement_warnings),indent=2)+'\n')
    with (output/'samples.csv').open('w') as f:
        cols=['campaign','trial','revision','mode','workers','calls','wall_ns_per_call','cpu_ns_per_call','throughput_calls_s','target_rss_kib','server_rss_kib','received','measured_received','expected_measured','measured_snapshots','capture_fraction']
        writer=csv.DictWriter(f,fieldnames=cols,extrasaction='ignore');writer.writeheader();writer.writerows(rows)
    for row in summary:
        print(row['campaign'],row['revision'],row['mode'],row['workers'],row['trials'],
              'wall_us',round(row['wall_ns_per_call']['median']/1000,3),
              'cpu_us',round(row['cpu_ns_per_call']['median']/1000,3),
              'capture',row['capture_fraction'].get('median'),
              'RSS_MiB',round(row['target_rss_kib']['median']/1024,1))
    print('validation_errors',errors)
    print('excluded_trials',excluded)
    print('measurement_warnings',measurement_warnings)
    return 1 if errors or excluded else 0

if __name__=='__main__':raise SystemExit(main())
