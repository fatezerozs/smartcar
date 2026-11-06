"""Offline full SCRAW import, CRC/PNG audit, continuous timeline and pixel checks."""
import argparse,json,hashlib,statistics
from pathlib import Path
from vision_data.archive import import_session,load_session
from vision_data.continuous import build_report

def verify(source,output):
 source=Path(source).resolve();output=Path(output).resolve()
 if output.exists():raise ValueError('Output must be new')
 meta=json.loads((source/'session.json').read_text(encoding='utf-8'))
 expected={'session.json',*meta['segments']}
 actual={p.name for p in source.iterdir() if p.is_file()}
 if actual!=expected:raise ValueError('Directory/manifest entries differ')
 output.mkdir()
 imported=import_session(source,output/'archive')
 (output/'import-result.json').write_text(json.dumps(imported,indent=2))
 sid=meta['session_id'];quality=build_report(output/'archive',sid,output/'continuous',every=10)
 base,manifest,frames=load_session(output/'archive',sid)
 # Independent source-byte pixel inspection, separately from PNG round-trip verification.
 samples=[];previous=None;identical=0
 for f in frames:
  p=source/Path(f['source_segment']).name
  with p.open('rb') as stream:stream.seek(f['payload_offset']);b=stream.read(f['payload_size'])
  if len(b)!=f['payload_size'] or hashlib.sha256(b).hexdigest()!=f['payload_sha256']:raise ValueError('Source pixel identity mismatch')
  if previous==b:identical+=1
  previous=b
  mean=statistics.fmean(b)
  samples.append({'frame_id':f['frame_id'],'minimum':min(b),'maximum':max(b),'mean':mean,'stddev':statistics.pstdev(b),'unique_values':len(set(b))})
 report={'synthetic':meta['synthetic'],'frames':len(frames),'all_source_pixels_match_archive':True,'identical_adjacent_pairs':identical,'pixel_statistics':samples,'source_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in source.iterdir() if p.is_file()},'complete_observed_window':quality['status']=='complete','b2_full_acceptance':False,'remaining':'actual camera configuration and CSI/SD resource evidence must be reviewed separately'}
 (output/'pixel-audit.json').write_text(json.dumps(report,indent=2))
 print(json.dumps({'source':str(source),'session_id':sid,'status':quality['status'],'frames':len(frames),'fps':quality['observed_valid_record_fps'],'pixel_audit':str(output/'pixel-audit.json')},indent=2))
 return quality['status']=='complete' and not meta['synthetic']
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',required=True);p.add_argument('--output',required=True);a=p.parse_args();raise SystemExit(0 if verify(a.source,a.output) else 1)
