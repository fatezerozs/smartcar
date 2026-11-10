"""Reproduce four original-row gray-floor/white-road diagnostic examples.

This is an analysis of archived native outputs, not a production segmentation
algorithm or an end-to-end counterfactual. Run from the repository root after
the performance-vision-images suite; no hardware or firmware changes.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from tools.speed_planning.visual_trace import decode_frame
from tools.testing.audit_vision_threshold import inspect

SESSION=ROOT/'docs/changes/phase2-calibration-20261108/evidence/visual-continuous/session04'
INPUTS=('demo/code/vision_midterm.c','demo/code/vision_reference_route.h',
        'tools/speed_planning/vision_adapter.c','demo/code/vision_metric_io.c')

def digest(data):return hashlib.sha256(data).hexdigest()

def vote(row,threshold):
    result=[int(v>threshold) for v in row]
    return [result[0]]+[int(sum(result[x-1:x+2])>=2) for x in range(1,len(row)-1)]+[result[-1]]

def run(folder):
    folder=Path(folder).resolve();saved=json.loads((SESSION/'session.json').read_text('utf-8'))
    original_report=json.loads((folder/'report.json').read_text('utf-8'))
    for name in INPUTS:
        if digest((ROOT/name).read_bytes())!=original_report['sourceSha256'][name]:
            raise ValueError('Current native source differs from archived measurement: '+name)
    with (folder/'rows.csv').open(newline='',encoding='utf-8') as f:rows=list(csv.DictReader(f))
    masks=(folder/'masks.gray').read_bytes()
    if len(masks)!=len(saved['frames'])*188*120:raise ValueError('Incomplete native masks')
    cases=[]
    for i in (0,32,80,99):
        record=saved['frames'][i];blob=(SESSION/record['file']).read_bytes()
        if digest(blob)!=record['sha256']:raise ValueError('Original VQF SHA mismatch')
        decoded=decode_frame(blob,saved['frozen']['gen'],i)
        if any(decoded[k]!=record[k] for k in decoded):raise ValueError('VQF metadata mismatch')
        y=54;gray=list(blob[96+y*188:96+(y+1)*188]);row=rows[i*120+y]
        if int(row['frame'])!=i or int(row['row'])!=y:raise ValueError('Row order mismatch')
        detail=inspect(gray);threshold=detail['predictedFinalThreshold'];native_threshold=int(row['threshold'])
        if threshold!=native_threshold:raise ValueError('Independent oracle disagrees with native threshold')
        actual=vote(gray,native_threshold);stored=masks[i*188*120+y*188:i*188*120+(y+1)*188]
        if actual!=[int(v!=0) for v in stored]:raise ValueError('Native 3-pixel mask mismatch')
        stage=detail['stages'][0];first,second=stage['first'],stage['second']
        middle=[v for v in gray if first<v<=second];white=[v for v in gray if v>second]
        counterfactual=vote(gray,second)
        cases.append({'index':i,'vqfSha256':record['sha256'],'row':y,'firstThreshold':first,
          'candidateThreshold':second,'nativeFinalThreshold':native_threshold,
          'middleCount':len(middle),'middleSum':sum(middle),'whiteCount':len(white),'whiteSum':sum(white),
          'middleMean':sum(middle)/len(middle),'whiteMean':sum(white)/len(white),
          'ratio':sum(middle)*len(white)/(len(middle)*sum(white)),
          'ratioPass':stage['checks']['middleAtLeastThreeQuartersWhite'],
          'allFirstPassGuards':stage['checks'],'nativeWhitePixels':sum(actual),
          'forcedCandidateWhitePixels':sum(counterfactual),'rowFlags':int(row['flags']),
          'rawValid':int(row['valid']),'confidence':int(row['confidence']),
          'junctionRows':int(row['junction_rows']),'junctionAnchors':int(row['junction_anchors']),
          'nearValid':int(row['near_valid']),'fullValid':int(row['full_valid']),
          'probes':[{'x':x,'gray':gray[x],'nativeWhite':actual[x],
                     'forcedCandidateWhite':counterfactual[x]} for x in (40,100,140)]})
    return {'scope':'Four selected original frames, row 54; native replay verified against archived source hashes',
      'counterfactual':'Force first bright-class candidate threshold on this row only; diagnostic pixel/3-vote comparison, no full-frame tracker or driving result',
      'timing':'Archived chronological replay uses uniform synthetic 20ms frame spacing; no board timing inference',
      'sourceSha256':{n:original_report['sourceSha256'][n] for n in INPUTS},'cases':cases}

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifacts',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args();result=run(args.artifacts)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(f"Verified {len(result['cases'])} VQFs, source hashes, native thresholds and 3-vote masks -> {args.output}")
