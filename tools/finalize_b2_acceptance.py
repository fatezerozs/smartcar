"""Pair board closure/metrics, frozen build and immutable full Session evidence.

Acceptance is restricted to the authorized stationary short window. Offline-only
tools keep their conservative hardware flags; this combined record is separate.
"""
import argparse,csv,hashlib,json
from pathlib import Path
from vision_data.archive import verify_session

def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def finalize(board,download,build,output,provenance=None):
 board,download,build,output=map(lambda p:Path(p).resolve(),(board,download,build,output))
 closed=read(board/'result.json');metrics=read(board/'metrics-review.json')
 acquisition=read(download/'acquisition.json');source=download/acquisition['session']
 meta=read(source/'session.json');q=read(download/'pc/continuous/continuous_report.json')
 pixels=read(download/'pc/pixel-audit.json');inputs=read(build/'inputs.json');proof=read(build/'build-evidence.json')
 archive=download/'pc/archive';base=archive/'sessions'/meta['session_id']
 verified=verify_session(archive,meta['session_id']);integrity=read(base/'integrity.json')
 ending=integrity['segments'][-1]['ending'];n=q['frames']
 source_files=[source/'session.json',*[source/s for s in meta['segments']]]
 data_bytes=sum(p.stat().st_size for p in source_files)
 raw_equal=all(sha(p)==sha(base/'A0'/p.name) for p in source_files)
 write=next(g for g in metrics['io'] if g['io']=='write')
 sync=next(g for g in metrics['io'] if g['io']=='sync')
 capture_dir=metrics['device']['session']
 with (download/'pc/continuous/frame_timeline.csv').open(newline='',encoding='utf-8') as f:timeline=list(csv.DictReader(f))
 span=int(timeline[-1]['timestamp_us'])-int(timeline[0]['timestamp_us']) if timeline else -1
 image_ok=sha(build/'Objects/recorder_b2.axf') in proof['sha256'].values()
 image_provenance=None
 if provenance:
  image_provenance=read(provenance);snap=Path(image_provenance['snapshot'])
  a=(build/'Objects/recorder_b2.bin').read_bytes();b=(snap/'recorder_b2.bin').read_bytes();off=image_provenance['compile_time_offset']
  image_ok=(len(a)==len(b) and a[:off]==b[:off] and a[off+8:]==b[off+8:] and b[off:off+8].decode()==meta['firmware_compiled'].rsplit(' ',1)[-1] and sha(snap/'recorder_b2.axf')==sha(build/'Objects/recorder_b2.axf')==image_provenance['tested_axf_sha256'] and sha(snap/'recorder_b2.bin')==image_provenance['tested_load_image_sha256'])
 sources_unchanged=all(sha(Path(__file__).resolve().parents[1]/p)==h for p,h in inputs['sources'].items())
 checks={
  'board_identity_closed_and_final_sync':closed['identity_verified'] and closed['board_closed'] and closed['written']==closed['synced']==n,
  'actual_metrics_report_complete':metrics['measurements_present_and_valid'] and all(metrics['checks'].values()),
  'board_session_matches_download':capture_dir=='2:/'+source.name and acquisition['session']==source.name,
  'capture_source_fingerprint_matches_build':meta['firmware_input_tree_sha256']==inputs['manifest_digest'],
  'frozen_build_and_map_isolated':proof['zero_errors_warnings'] and proof['no_actuator_encoder_control_symbols'] and proof['fresh_dependencies_match_frozen_manifest'],
  'frozen_source_inputs_still_match':sources_unchanged,
  'tested_image_provenance_verified':image_ok,
  'real_camera_data':not meta['synthetic'] and not pixels['synthetic'],
  'full_raw_source_preserved':raw_equal and pixels['all_source_pixels_match_archive'],
  'crc_png_and_manifest_complete':verified['integrity']=='complete' and q['status']=='complete' and acquisition['pc_verified'],
  'complete_observed_sequence':not q['known_missing_frames'] and not q['unknown_source_loss'] and q['last_sequence']-q['first_sequence']+1==n and ending['accepted']==ending['recorded_frames']==ending['observed_capture']==n,
  'timestamp_monotonic':q['timestamp_strictly_increasing'],
  'reported_io_bytes_match_downloaded_files':write['bytes']==data_bytes,
  'zero_recorded_errors_and_losses':all(ending[k]==0 for k in ('queue_full','pool_full','capture_ambiguous','storage_errors')),
  'bounded_observed_window_normal_stop':ending['stop_reason']=='bounded_static_capture_window' and n>0 and 0<=span<=3000000,
 }
 # Check the actual observed record span; the firmware's stop policy is soft.
 # This does not assert a general frame-count or scheduling deadline.
 accepted=all(checks.values())
 result={'schema':'smartcar.b2-combined-acceptance/1','checks':checks,'b2_stage_accepted':accepted,
  'scope':'approved stationary Camera+SD bounded short window; standalone B2 profile',
  'session_id':meta['session_id'],'card_directory':capture_dir,'frames':n,
  'first_sequence':q['first_sequence'],'last_sequence':q['last_sequence'],
  'observed_timestamp_span_us':span,
  'observed_record_fps':q['observed_valid_record_fps'],'geometry':q['geometry'],
  'sd_clock_hz':metrics['device']['sd_hz'],'sd_bus_bits':metrics['device']['sd_bits'],
  'cpu_nonidle_estimate_percent':metrics['runtime']['nonidle_permille']/10,
  'queue_high_water':ending['queue_high_water'],'capture_copy_max_us':ending['capture_copy_max_us'],
  'write_io_KiBps':write['bytes']*1000000/write['total_us']/1024,
  'write_plus_sync_call_KiBps':write['bytes']*1000000/(write['total_us']+sync['total_us'])/1024,
  'io':metrics['io'],'irq':metrics['runtime'],'tasks':metrics['tasks'],
  'source_sha256':{p.name:sha(p) for p in source_files},
  'capture_axf_sha256':sha(build/'Objects/recorder_b2.axf'),
  'capture_input_fingerprint':inputs['manifest_digest'],
  'tested_image_provenance':image_provenance,
  'board_evidence':str(board),'download_evidence':str(download),'build_evidence':str(build),
  'b1':'SKIPPED_BY_USER_NOT_PASSED','production_ready':False,'control_parallel_validated':False,'long_duration_validated':False,'b3_authorized':False,
  'limitations':metrics['limitations']+['101 vs 100 events is permitted by the documented soft stop; no strict frame-count claim','exposure/gain sensor-register readback unavailable; reported geometry is the captured output contract and verified CSI DMA byte grouping','historical SD-only tests use different bus clocks, I/O sizes and software; no matched-load causal comparison or long-duration guarantee','first/final SD bytes were downloaded using onboard FatFs+USDHC, not a separate PC card reader'],
  'evidence_layers':['structured_parsed','computed','visual_sample_review_required_separately']}
 with output.open('x',encoding='utf-8') as f:json.dump(result,f,indent=2)
 print(json.dumps(result,indent=2));return accepted

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('board','download','build','output'):p.add_argument('--'+name,required=True)
 p.add_argument('--provenance')
 a=p.parse_args();raise SystemExit(0 if finalize(a.board,a.download,a.build,a.output,a.provenance) else 1)
