"""B2 offline continuous-frame audit built on the existing SCRAW archive.

No serial, hardware, draft protocol replacement or production-control access.
"""
from __future__ import annotations
import csv
import statistics
from pathlib import Path
from .archive import load_session, verify_session
from .common import data_root, read, require, sha, write, file_ref
from .sampling import make_view
from .annotations import make_tasks


def _range(first, last, kind):
    return {'first': first, 'last': last, 'count': last - first + 1, 'evidence': kind}


def build_report(root, session_id, output, every=10):
    require(type(every) is int and every > 0, 'Positive sample interval required')
    root=data_root(root)
    output=Path(output).resolve()
    data_root(output.parent)
    require(not output.exists(), 'Report output already exists')
    base, manifest, frames=load_session(root,session_id)
    require(not output.is_relative_to(base) and not base.is_relative_to(output), 'Report/archive overlap')
    verified=verify_session(root,session_id)
    integrity=read(base/'integrity.json')
    meta=manifest['metadata']
    ending=integrity['segments'][-1].get('ending')
    gaps=[]
    previous=None
    deltas=[]
    for frame in frames:
        if previous:
            if frame['source_sequence']>previous['source_sequence']+1:
                gaps.append(_range(previous['source_sequence']+1,frame['source_sequence']-1,'validated_record_sequence'))
            deltas.append(frame['timestamp_us']-previous['timestamp_us'])
        previous=frame
    if ending and meta['format']=='SCRAW-1' and frames:
        first,last=frames[0]['source_sequence'],frames[-1]['source_sequence']
        if ending['base_source_sequence']+1<first:
            gaps.insert(0,_range(ending['base_source_sequence']+1,first-1,'END_base_vs_first_record'))
        if last<ending['last_source_sequence']:
            gaps.append(_range(last+1,ending['last_source_sequence'],'END_last_vs_last_record'))
    unknown_loss=ending is None or bool(ending.get('unknown_source_loss'))
    complete=verified['integrity']=='complete' and not gaps and not unknown_loss
    intervals=sorted(deltas)
    duration=(frames[-1]['timestamp_us']-frames[0]['timestamp_us']) if len(frames)>1 else 0
    resolution=meta.get('timestamp_resolution_us')
    quality={
        'schema_version':'smartcar.continuous-session-audit/1',
        'session_id':session_id,'format':meta['format'],'synthetic':meta['synthetic'],
        'status':('complete' if complete else 'invalid' if not frames else 'incomplete'),
        'source_archive_integrity':verified['integrity'],'frames':len(frames),
        'first_sequence':frames[0]['source_sequence'] if frames else None,
        'last_sequence':frames[-1]['source_sequence'] if frames else None,
        'missing_frame_ranges':gaps,'known_missing_frames':sum(g['count'] for g in gaps),
        'unknown_source_loss':unknown_loss,
        'timestamp_unit':'us','timestamp_semantics':meta['timestamp_semantics'],
        'timestamp_resolution_us':resolution,'timestamp_strictly_increasing':all(x>0 for x in deltas),
        'observed_valid_record_fps':((len(frames)-1)*1000000/duration) if duration>0 else None,
        'interval_us':{'minimum':min(intervals) if intervals else None,
                       'median':statistics.median(intervals) if intervals else None,
                       'p95':intervals[(len(intervals)*95+99)//100-1] if intervals else None,
                       'maximum':max(intervals) if intervals else None},
        'geometry':{k:meta[k] for k in ('width','height','stride','pixel_format')},
        'valid_region':[0,0,meta['width'],meta['height']],
        'pixel_byte_order':'single_byte_GRAY8','image_transform':'visible_rows_no_resize',
        'configured_sensor_fps':meta.get('configured_sensor_fps'),
        'actual_exposure_readback':meta.get('actual_exposure_readback'),
        'telemetry_available':False,'telemetry_status':'SCRAW-1 producer has no per-frame control sidecar',
        'capture_time_is_exposure_start':False,
        'b2_hardware_accepted':False,'hardware_acceptance_reason':'offline file validation does not establish CSI/SD concurrency or actual sensor configuration',
        'b1_status':'skipped_by_user_not_passed','production_ready':False,
        'source_manifest_sha256':sha(base/'session_manifest.json'),
        'source_files':manifest['source_files'],
        'recorder_gap_records':[g for segment in integrity['segments'] for g in segment.get('gaps',[])],
        'source_issues':integrity['issues'],
    }
    output.mkdir()
    columns=['frame_id','source_sequence','timestamp_us','width','height','stride',
             'source_segment','payload_offset','payload_size','payload_crc32','payload_sha256','png_path']
    with (output/'frame_timeline.csv').open('x',newline='',encoding='utf-8') as f:
        writer=csv.DictWriter(f,fieldnames=columns,extrasaction='ignore');writer.writeheader();writer.writerows(frames)
    write(output/'missing_frames.json',{'ranges':gaps,'unknown_source_loss':unknown_loss,'never_zero_filled':True})
    view_id='b2-'+quality['source_manifest_sha256'][:20]+'-every-'+str(every)
    make_view(root,view_id,[session_id],every=every,context=0)
    tasks=make_tasks(root,view_id,output/'label_studio')
    quality['sampling']={'every':every,'view_id':view_id,'frames':tasks['frames'],'raw_sequence_preserved':True}
    quality['timeline']=file_ref(output/'frame_timeline.csv',output)
    quality['label_studio_tasks']=file_ref(output/'label_studio/tasks.json',output)
    write(output/'continuous_report.json',quality)
    return quality
