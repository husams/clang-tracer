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
    while True:
        b=data[pos];pos+=1;value|=(b&127)<<shift
        if b<128:return value,pos
        shift+=7

def fields(data):
    result=defaultdict(list);pos=0
    while pos<len(data):
        key,pos=varint(data,pos); tag,wire=key>>3,key&7
        if wire==0:value,pos=varint(data,pos)
        elif wire==2:
            size,pos=varint(data,pos);value=data[pos:pos+size];pos+=size
        elif wire in (1,5):
            size=8 if wire==1 else 4;value=data[pos:pos+size];pos+=size
        else:raise ValueError(wire)
        result[tag].append(value)
    return result

def frames(path):
    data=path.read_bytes() if path.exists() else b'';pos=0;rows=[]
    while pos<len(data):
        size=struct.unpack_from('<I',data,pos)[0];pos+=4
        assert pos+size<=len(data)
        rows.append(fields(data[pos:pos+size]));pos+=size
    return rows

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
    rows=[];groups=defaultdict(list);cold=defaultdict(list);errors=[]
    for path in sorted(a.root.glob('*/*/stats.json')):
        campaign=path.parent.parent.name
        if campaign.startswith('pilot'):continue
        s=json.loads(path.read_text());d=path.parent
        rev,mode,workers=s['revision'],s['mode'],s['workers'];v=list(map(int,s['result']))
        n,_,wall,cpu,checksum,valid,rss,faults,vol,ivol=v
        assert valid==1
        events=frames(d/'events.pb')
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
        lat=[]
        if (d/'latencies.csv').exists():
            lat=list(map(int,(d/'latencies.csv').read_text().strip().split(',')[1:]))
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
        row['pooled_latency_ns']=distribution([x for item in items for x in (list(map(int,(a.root/item['campaign']/item['trial']/'latencies.csv').read_text().strip().split(',')[1:])) if (a.root/item['campaign']/item['trial']/'latencies.csv').exists() else [])])
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
