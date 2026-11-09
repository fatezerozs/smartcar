"""Build a source-pinned C26 SHADOW configuration from existing Phase2 data.

No device access, production macro writes, or physical threshold approval.
Uncalibrated dynamic/policy fields retain explicit engineering references.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
BASE = 'docs/changes/phase2-calibration-20261108/'
PROFILE = ROOT/'profiles/phase3-c26-shadow-20261109.json'


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def project(matrix, u, v):
    z = matrix[2][0]*u+matrix[2][1]*v+matrix[2][2]
    if not math.isfinite(z) or abs(z)<1e-9:
        raise ValueError('Singular image point')
    return [(matrix[i][0]*u+matrix[i][1]*v+matrix[i][2])/z for i in range(2)]


def build_profile(root=ROOT):
    root=Path(root);sources={}
    def read(name):
        path=root/name;sources[name]=digest(path)
        return json.loads(path.read_text('utf-8'))
    geometry=read(BASE+'reported-candidates.json')['geometryM']
    scale=read(BASE+'passive-distance-summary.json')
    mapping=read(BASE+'ground-homography-origin-corrected.json')
    fit=read(BASE+'ground-homography-candidate.json')
    near=read(BASE+'ground-centre250-check.json')
    latency=read(BASE+'evidence/latency-static/session01/analysis.json')
    visual=read(BASE+'evidence/visual-continuous/session04/analysis.json')
    stops=read(BASE+'stop-distance-candidate-reanalysis.json')
    coast=read(BASE+'coast-profile-review.json')
    response=[read(BASE+'evidence/'+name+'/review.json') for name in
              ('ground-target40-cap250-1500-01','ground-target40-cap250-2500-01')]
    encoder=read('profiles/encoder-production-20261106.json')
    reference=read('profiles/c1-host-reference-v0.3.json')['parameters']
    for name in ('demo/code/vision_midterm.c','demo/code/vision_speed.h','demo/code/vision_profile.h',
                 BASE+'evidence/visual-supervisor-replay04/visual_policy.h'):
        sources[name]=digest(root/name)
    preview_source=(root/'demo/code/vision_midterm.c').read_text('utf-8')
    if 'source[y * 2 * VISION_WIDTH + x * 2]' not in preview_source:
        raise ValueError('Preview sampling changed; audit homography conversion')
    speed_header=(root/'demo/code/vision_speed.h').read_text('utf-8')
    def speed_contract(name):
        m=re.search(r'^#define '+name+r' (\d+)U',speed_header,re.M)
        if not m:raise ValueError('Missing native timing contract: '+name)
        return int(m[1])
    sample_age=speed_contract('VD_SPEED_SAMPLE_MAX_AGE_MS')
    sample_gap=speed_contract('VD_SPEED_SAMPLE_MAX_INTERVAL_MS')
    native=root/'demo/code/midterm_route_profile.h';sources['demo/code/midterm_route_profile.h']=digest(native)
    match=re.search(r'^#define MT_ROUTE_FAST_TICKS (\d+)U',native.read_text('utf-8'),re.M)
    if not match:raise ValueError('Native count range changed')
    max_ticks=int(match[1])
    if not mapping.get('cameraOriginHorizontalDistanceConfirmed') or fit['imageSize']!=[94,60]:
        raise ValueError('Confirmed camera-ground origin and known preview sampling required')
    h=mapping['homographyImageToRearGroundMm']
    if len(h)!=3 or any(len(row)!=3 or any(not math.isfinite(x) for x in row) for row in h):
        raise ValueError('Finite 3x3 homography required')
    # vision_midterm_preview uses source[y*2][x*2], with zero-based centres.
    # H_raw_m = diag(.001,.001,1) * H_preview_mm * diag(.5,.5,1).
    raw_h=[[h[i][j]*(.5 if j<2 else 1)*(.001 if i<2 else 1) for j in range(3)] for i in range(3)]
    points=mapping['points'];ys=[p['yRearForwardMm']*.001 for p in points]
    xs=[p['xRearRightMm']*.001 for p in points];domain=[min(xs),max(xs),min(ys),max(ys)]
    rows=[]
    # Seven interior sample distances, not new measured points. Their image
    # rows come from x=0 projection inversion; no whole-image extrapolation.
    for j in range(1,8):
        y=(domain[2]+(domain[3]-domain[2])*j/8)*1000
        a,b,c=h[0];d,e,f=h[1];g,k,l=h[2]
        denominator=a*(e-y*k)-b*(d-y*g)
        v=(a*(y*l-f)+c*(d-y*g))/denominator
        rows.append(int(round(v*2)))
    if len(set(rows))!=7 or any(not 0<=r<120 for r in rows) or rows!=sorted(rows,reverse=True):
        raise ValueError('Derived preview rows invalid')
    left,right=scale['descriptiveMeanCountsPerMetreLeftRight']
    if not (0<left<1e6 and 0<right<1e6):raise ValueError('Invalid distance scales')
    newest=response[-1];target=newest['config']['maximumTargetTicks10ms']
    requested=.5*(target*100/left+target*100/right)
    def ref(name):return reference[name]['value']
    # Maximum is a software input range converted from the original count
    # ceiling; it is NOT a measured/approved maximum physical driving speed.
    planning={'maximum_speed':max_ticks*100/min(left,right),'near_cap':requested,
              'lateral_accel':ref('lateralAccelerationMps2'),'accel':ref('accelerationMps2'),
              'brake':ref('brakingDecelerationMps2'),'jerk_accel':ref('jerkAccelerationMps3'),
              'jerk_brake':ref('jerkBrakingMps3'),'latency_s':ref('latencyS'),
              'distance_margin':ref('distanceMarginM'),'curvature_epsilon':.0001,'max_dt_s':sample_gap*.001}
    seed=json.dumps(sources,sort_keys=True,separators=(',',':')).encode()
    revision=int(hashlib.sha256(seed).hexdigest()[:8],16) or 1
    profile={'schemaVersion':'smartcar.phase3-shadow-profile/1','profileId':'C26-SHADOW-'+format(revision,'08X'),
             'revision':revision,'mode':'shadow','productionApproved':False,'activeMetricControlAllowed':False,
             'sourcesSha256':sources,
             'measured':{'geometryM':geometry,'encoderProfile':encoder,
                         'distanceCountsPerMetreLeftRight':{'value':[left,right],'status':'measured-candidate'},
                         'groundMapping':{'previewSize':[94,60],'runtimeSize':[188,120],
                                          'previewToRawPixelScale':2,'runtimeHomographyMetres':raw_h,
                                          'coordinateOrigin':'rear-axle ground projection; x right,y forward',
                                          'sampledGroundRectangleM':domain,
                                          'heldOutNearPointResidualM':near['residualToOperatorReportedPointMm']*.001,
                                          'status':'measured-candidate; rectangle is an evaluation domain, not a certified validity region'},
                         'staticLatencyMs':latency['stagesMs']['capture_to_observe_return'],
                         'visualSession':{'frames':visual['frames'],'controlCounts':visual['controlValidFarCounts']},
                         'groundResponses':[{'config':r['config'],'operator':r['operatorObservation'],
                                             'tail300ms':r['tailWindowsMs']['300'],'stopWheelReview':r['stopWheelReview']} for r in response],
                         'historicalCoastRuns':stops['runs'],'coastProfileInterpretation':coast['interpretation']},
             'runtimeConfig':{'limits':planning,'ticks_per_metre_left':left,'ticks_per_metre_right':right,
                              'rear_track_m':geometry['rearTrack'],'max_ticks_10ms':max_ticks,
                              'frame_max_age_ms':100,'encoder_max_age_ms':sample_age,'candidate_max_age_ms':sample_gap,
                              'odometry_max_age_ms':sample_age,'interpolation_gap_ms':sample_gap,'sign_left':-1,'sign_right':1,
                              'compensate':0,'geometry_calibrated':0,'near_rows':rows[:5],'full_rows':rows,
                              'min_confidence':25,'max_step_m':ref('distanceMarginM'),
                              'requested_speed_m_s':requested,'pwm_cap':newest['config']['requestPermille'],
                              'feedforward_right':0,'feedforward_left':0,
                              'policy':{'frame_timeout_ms':100,'encoder_timeout_ms':30,
                                        'near_window_ms':ref('nearWindowMs'),'far_budget_ms':ref('farBudgetMs'),
                                        'stop_timeout_ms':ref('stopTimeoutMs'),'near_frames':ref('nearFrames'),
                                        'far_frames':ref('farFrames'),'zero_samples':3,
                                        'far_distance_m':ref('farDistanceM'),'zero_speed_m_s':.02}},
             'parameterClasses':{'limitsAndBudgets':'engineering references from c1-host-reference-v0.3; SP_SIMULATION',
                                 'sampleAndCandidateAges':'existing software timing contracts; not measured physical latency',
                                 'maximumSpeed':'converted native count ceiling; not measured v_max',
                                 'requestedSpeed':'target40 counts/10ms converted using candidate scales',
                                 'previewRows':'computed sampling inside the fitted ground-distance range',
                                 'continuityAndZeroSpeed':'shadow experiment settings; not adopted physical thresholds',
                                 'pwmAndFeedforward':'unused by shadow output; original legacy PI/gains/PWM limits unchanged'},
             'unresolved':{'runningEndToEndLatencyS':None,'approvedBrakeMps2':None,'approvedJerkMps3':None,
                           'approvedLateralAccelerationMps2':None,'dynamicNearFarBudgets':None,
                           'groundTurningCurvature':None},
             'remainingWork':['Bundle/adapt existing measurements; do not repeat the entire Phase2 campaign.',
                              'Measure RUNNING/MPC timing, dynamic visual/odometer episodes and ground turn geometry.',
                              'Resolve near-domain errors and review dynamic/stop parameters before active metric driving.']}
    return profile


def validate_profile(profile,root=ROOT):
    # Rebuild rather than accept a relabelled/tampered active profile. This pins
    # original measurements AND the exact construction/units/reference policy.
    if profile!=build_profile(root):raise ValueError('Profile differs from current source-pinned shadow data')
    return profile


def header(profile):
    validate_profile(profile);c=profile['runtimeConfig'];limits=c['limits'];policy=c['policy']
    def f(value):
        s=format(float(value),'.9g');return s+('.0' if '.' not in s and 'e' not in s else '')+'f'
    def members(data,names):return ','.join(f(data[n]) for n in names)
    h=profile['measured']['groundMapping']['runtimeHomographyMetres'];domain=profile['measured']['groundMapping']['sampledGroundRectangleM']
    text='''/* Generated from source-pinned Phase2 records. Shadow only. */
#ifndef PHASE3_SHADOW_PROFILE_H
#define PHASE3_SHADOW_PROFILE_H
#if !VISION_PLANNER_RUNTIME || VISION_PLANNER_ACTIVE
#error "Measured candidate profile supports shadow only."
#endif
'''
    text+='#define PHASE3_SHADOW_PROFILE_ID "'+profile['profileId']+'"\n'
    text+='static const vdr_config_t phase3_shadow_configuration = {\n'
    text+='  .planning = {.planner = {.revision='+str(profile['revision'])+'U,\n'
    text+='    .limits={SP_SIMULATION,'+members(limits,('maximum_speed','near_cap','lateral_accel','accel','brake','jerk_accel','jerk_brake','latency_s','distance_margin','curvature_epsilon','max_dt_s'))+'},\n'
    for n in ('ticks_per_metre_left','ticks_per_metre_right','rear_track_m'):text+='    .'+n+'='+f(c[n])+',\n'
    for n,v in [('max_left_ticks_10ms',c['max_ticks_10ms']),('max_right_ticks_10ms',c['max_ticks_10ms']),('frame_max_age_ms',c['frame_max_age_ms']),('encoder_max_age_ms',c['encoder_max_age_ms']),('candidate_max_age_ms',c['candidate_max_age_ms']),('compensate',0),('geometry_calibrated',0)]:text+='    .'+n+'='+str(v)+'U,\n'
    text+='  },.policy={'+','.join(str(policy[n])+'U' for n in ('frame_timeout_ms','encoder_timeout_ms','near_window_ms','far_budget_ms','stop_timeout_ms','near_frames','far_frames','zero_samples'))+','+members(policy,('far_distance_m','zero_speed_m_s'))+'}},\n'
    text+='  .odometry={'+str(profile['revision'])+'U,'+str(c['odometry_max_age_ms'])+'U,'+str(c['interpolation_gap_ms'])+'U,'+f(c['ticks_per_metre_left'])+','+f(c['ticks_per_metre_right'])+',-1,1},\n'
    text+='  .vision={.homography={'+','.join(f(v) for row in h for v in row)+'},\n'
    for n,v in zip(('min_x_m','max_x_m','min_y_m','max_y_m'),domain):text+='    .'+n+'='+f(v)+',\n'
    text+='    .max_step_m='+f(c['max_step_m'])+',.near_rows={'+','.join(map(str,c['near_rows']))+'},.full_rows={'+','.join(map(str,c['full_rows']))+'},\n'
    text+='    .near_count=5U,.full_count=7U,.min_confidence=25U,.revision='+str(profile['revision'])+'U,.max_age_ms=100U},\n'
    text+='  .requested_speed_m_s='+f(c['requested_speed_m_s'])+',.pwm_cap='+str(c['pwm_cap'])+'U,.feedforward_right=0U,.feedforward_left=0U\n};\n#endif\n'
    return text


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=PROFILE)
    args=parser.parse_args();profile=build_profile();args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(profile,ensure_ascii=False,indent=2)+'\n','utf-8')
    print(profile['profileId']+' -> '+str(args.output))
