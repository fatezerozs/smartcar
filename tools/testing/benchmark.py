"""Dataset/adapter boundary and independent quality, computation and system gates."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import re
import statistics
import sys
import uuid

from common import ROOT, inside, read_json, sha256, validate_schema, write_json


def validate_benchmark(spec):
    validate_schema(spec,'benchmark.schema.json')
    identifiers = [m['id'] for m in spec['metrics']]
    if len(set(identifiers)) != len(identifiers):
        raise ValueError('Duplicate metric ID')
    for metric in spec['metrics']:
        if metric['gate']=='absolute' and 'absoluteLimit' not in metric:
            raise ValueError('Absolute gate requires absoluteLimit')
        if metric['gate']=='regression' and 'maxRegressionFraction' not in metric:
            raise ValueError('Regression gate requires maxRegressionFraction')
        if metric['safetyCritical'] and metric['gate'] not in ('absolute','no-regression'):
            raise ValueError('Safety-critical metrics require an independent hard/no-regression gate')
        if metric['dimension']=='quality' and spec['dataset']['groundTruthSha256'] is None and metric['gate']!='observation':
            raise ValueError('Quality gates require versioned ground truth')
    if spec['status']=='ready':
        dataset = inside(ROOT,spec['dataset']['path'])
        if not dataset.is_file() or sha256(dataset)!=spec['dataset']['sha256']:
            raise ValueError('Dataset hash mismatch')
        dataset_manifest = read_json(dataset)
        if dataset_manifest.get('id')!=spec['dataset']['id'] or dataset_manifest.get('version')!=spec['dataset']['version']:
            raise ValueError('Dataset ID/version does not match its content manifest')
        samples = dataset_manifest.get('samples',[])
        ids = [sample['id'] for sample in samples]
        if not ids or len(set(ids))!=len(ids):
            raise ValueError('Dataset requires nonempty unique sample IDs')
        for sample in samples:
            if not sample.get('source') or not sample.get('scenario'):
                raise ValueError('Dataset samples require source and scenario provenance')
            if sha256(inside(ROOT,sample['path']))!=sample['sha256']:
                raise ValueError('Dataset sample hash mismatch: '+sample['id'])
        truth = spec['dataset']['groundTruthPath']
        if bool(truth)!=bool(spec['dataset']['groundTruthSha256']):
            raise ValueError('Ground truth path and content hash must be provided together')
        if truth and sha256(inside(ROOT,truth))!=spec['dataset']['groundTruthSha256']:
            raise ValueError('Ground truth hash mismatch')
        if spec['adapter']:
            inside(ROOT,spec['adapter'])
        if spec['approvedBaseline']:
            inside(ROOT,spec['approvedBaseline'])


def statistic(values, method):
    if not values or any(type(v) not in (int,float) or not math.isfinite(v) for v in values):
        raise ValueError('Metric samples must be nonempty and finite')
    ordered = sorted(values)
    if method=='mean': return statistics.mean(values)
    if method=='median': return statistics.median(values)
    if method=='min': return ordered[0]
    if method=='max': return ordered[-1]
    quantile = {'p95':.95,'p99':.99}[method]
    # Linear interpolation over the sorted samples, zero-based position (n-1)*p.
    position = (len(ordered)-1)*quantile
    low, high = math.floor(position), math.ceil(position)
    return ordered[low]+(ordered[high]-ordered[low])*(position-low)


def summarize(spec, measurement):
    for key in ('gitSha','dirty','inputTreeSha256','compilerId','environmentId','mode','dataset','parameters','warmup','repetitions','samples'):
        if key not in measurement:
            raise ValueError('Missing measurement metadata: '+key)
    if not re.fullmatch('[a-f0-9]{40}',measurement['gitSha']):
        raise ValueError('Measurement Git SHA must be a full commit ID')
    if type(measurement['dirty']) is not bool or not re.fullmatch('[a-f0-9]{64}',measurement['inputTreeSha256']):
        raise ValueError('Measurements require dirty state and a SHA-256 source/input snapshot')
    if any(not isinstance(measurement[k],str) or not measurement[k].strip() for k in ('compilerId','environmentId','mode')):
        raise ValueError('Compiler, environment and mode identities must be nonempty')
    if measurement['dataset'] != spec['dataset'] or measurement['parameters'] != spec['parameters']:
        raise ValueError('Measurements do not match the dataset/labels/parameters contract')
    if measurement['warmup'] != spec['warmup'] or measurement['repetitions'] != spec['repetitions']:
        raise ValueError('Warmup/repetition count does not match the benchmark contract')
    values = {m['id']:[] for m in spec['metrics']}
    repetitions = set()
    seen = set()
    for sample in measurement['samples']:
        if not isinstance(sample['sampleId'],str) or not sample['sampleId'] or type(sample['repetition']) is not int:
            raise ValueError('Samples require a nonempty ID and integer repetition')
        key = (sample['sampleId'],sample['repetition'])
        if key in seen:
            raise ValueError('Duplicate sample ID/repetition')
        seen.add(key)
        if type(sample['valid']) is not bool:
            raise ValueError('Sample validity must be explicit boolean')
        if not 0<=sample['repetition']<spec['repetitions']:
            raise ValueError('Sample repetition is out of range; warmup samples must be excluded')
        if sample['valid']:
            repetitions.add(sample['repetition'])
            if set(sample['metrics']) != set(values):
                raise ValueError('Each valid sample must contain exactly the declared metrics')
            for mid,value in sample['metrics'].items():
                if type(value) not in (int,float) or not math.isfinite(value):
                    raise ValueError('Non-finite metric sample: '+mid)
                values[mid].append(value)
    if repetitions != set(range(spec['repetitions'])):
        raise ValueError('Each measured repetition must have valid samples')
    return {m['id']:{'value':statistic(values[m['id']],m['statistic']),'validSamples':len(values[m['id']])}
            for m in spec['metrics']}


def compare(spec, current, baseline=None):
    metrics = summarize(spec,current)
    old = None
    if baseline:
        approval = baseline.get('approval',{})
        if not approval.get('reviewer') or not approval.get('approvedAt') or approval.get('gitSha')!=baseline.get('gitSha'):
            raise ValueError('Baseline must have explicit reviewer/time/commit approval')
        datetime.fromisoformat(approval['approvedAt'].replace('Z','+00:00'))
        if baseline.get('dirty') is not False:
            raise ValueError('An approved baseline must identify a clean reproducible commit')
        for key in ('dataset','parameters','compilerId','environmentId','mode','warmup','repetitions'):
            if current[key]!=baseline.get(key):
                raise ValueError('Baseline conditions do not match: '+key)
        old = summarize(spec,baseline)
        current_samples = {(s['sampleId'],s['repetition'],s['valid']) for s in current['samples']}
        baseline_samples = {(s['sampleId'],s['repetition'],s['valid']) for s in baseline['samples']}
        if current_samples!=baseline_samples:
            raise ValueError('Baseline and current run must measure the same samples and validity set')
    rows=[]
    for metric in spec['metrics']:
        mid = metric['id']
        value = metrics[mid]['value']
        prior = old[mid]['value'] if old else None
        decision,reason='OBSERVATION','Recorded without a gate'
        direction = 1 if metric['direction']=='lower' else -1
        if metric['gate']=='absolute':
            decision='FAIL' if direction*(value-metric['absoluteLimit'])>0 else 'PASS'
            reason='Independent absolute limit'
        elif metric['gate'] in ('regression','no-regression'):
            if prior is None:
                decision,reason='INCONCLUSIVE','Approved comparable baseline is missing'
            else:
                delta=direction*(value-prior)
                scale=abs(prior)
                noise=metric['noiseFraction']*scale
                tolerance=metric.get('maxRegressionFraction',0)*scale if metric['gate']=='regression' else 0
                if delta==0:
                    decision,reason='PASS','Identical statistic'
                elif abs(delta)<=noise or abs(delta-tolerance)<=noise:
                    decision,reason='INCONCLUSIVE','Difference intersects the declared noise interval; repeat measurement'
                elif delta>tolerance:
                    decision,reason='FAIL','Regression exceeds its independent tolerance'
                else:
                    decision,reason='PASS','Within the independent regression gate'
        rows.append(dict(metric,id=mid,value=value,baseline=prior,decision=decision,reason=reason,
                         validSamples=metrics[mid]['validSamples']))
    decisions = {row['decision'] for row in rows}
    status = 'FAIL' if 'FAIL' in decisions else 'INCONCLUSIVE' if 'INCONCLUSIVE' in decisions else 'PASS'
    return {'schemaVersion':'1.0','benchmarkId':spec['id'],'status':status,
            'gitSha':current['gitSha'],'dirty':current['dirty'],'inputTreeSha256':current['inputTreeSha256'],
            'baselineSha':baseline['gitSha'] if baseline else None,
            'evidenceClass':spec['evidenceClass'],'dataset':spec['dataset'],'parameters':spec['parameters'],
            'conditions':{k:current[k] for k in ('compilerId','environmentId','mode','warmup','repetitions')},'metrics':rows}


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest',default='tests/benchmarks/manifests/vision-host.json')
    parser.add_argument('--measurements')
    parser.add_argument('--baseline')
    args=parser.parse_args(argv)
    try:
        spec=read_json(inside(ROOT,args.manifest))
        validate_benchmark(spec)
        if spec['status']=='pending':
            print('SKIPPED: '+spec['reason'])
            return 0
        if not args.measurements:
            raise ValueError('Ready benchmarks require explicit offline measurements')
        raw=read_json(inside(ROOT,args.measurements))
        baseline_path = args.baseline or spec['approvedBaseline']
        if args.baseline and args.baseline!=spec['approvedBaseline']:
            raise ValueError('The supplied baseline must match the manifest approvedBaseline; no automatic baseline promotion')
        baseline=read_json(inside(ROOT,baseline_path)) if baseline_path else None
        result=compare(spec,raw,baseline)
        output=ROOT/'.test-results/benchmarks'/('comparison-'+uuid.uuid4().hex[:10])
        output.mkdir(parents=True)
        write_json(output/'metrics.json',result)
        write_json(output/'measurement.json',raw)
        write_json(output/'manifest.json',spec)
        if baseline:
            write_json(output/'approved-baseline.json',baseline)
        with (output/'raw-results.jsonl').open('w',encoding='utf-8') as stream:
            for sample in raw['samples']:
                stream.write(json.dumps(sample,allow_nan=False)+'\n')
        lines=['# Benchmark comparison','',f"Status: **{result['status']}**",'',
               '| Metric | Dimension | Current | Baseline | Unit | Decision |',
               '| --- | --- | --- | --- | --- | --- |']
        lines += [f"| {m['id']} | {m['dimension']} | {m['value']} | {m['baseline']} | {m['unit']} | {m['decision']} |" for m in result['metrics']]
        lines += ['', 'Metrics gate independently; quality failures cannot be offset by faster computation.',
                  'Host measurements do not imply MCU cycles, stack usage, WCET or physical response.', '']
        (output/'comparison.md').write_text('\n'.join(lines),encoding='utf-8')
        print(str(output/'comparison.md'))
        return 0 if result['status']=='PASS' else 1
    except (ValueError,KeyError,TypeError,OSError) as error:
        print('BENCHMARK ERROR: '+str(error),file=sys.stderr)
        return 2


if __name__=='__main__':
    sys.exit(main())
