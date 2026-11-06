"""Explicitly provisional engineering references, never production defaults."""
import json
import math
from pathlib import Path
from .vehicle_model import ROOT,load_model,nominal_circumference
from .data_inputs import child,load_encoder,sha256,read_json

DEFAULT_REFERENCE=ROOT/'profiles/c1-host-reference-v0.3.json'


def load_reference(path=DEFAULT_REFERENCE):
    data=read_json(path)
    if data['schemaVersion']!='smartcar.host-reference/1' or data['productionApproved'] is not False:
        raise ValueError('Only unapproved engineering references supported')
    for name,param in data['parameters'].items():
        value=param['value']
        if isinstance(value,bool) or not isinstance(value,(int,float)) or not math.isfinite(value) or value<=0:
            raise ValueError('Invalid reference: '+name)
        if not param['basis'] or param['status'] not in ('engineering-reference','simulation-assumption','conditional-hypothesis','historical-candidate','existing-project-reference'):
            raise ValueError('Missing reference provenance: '+name)
    if ('encoderProfile' in data)==('encoderWheelCprHypothesis' in data['parameters']):
        raise ValueError('Specify exactly one encoder source: adopted record or legacy hypothesis')
    return data


def values(data):
    return {key:item['value'] for key,item in data['parameters'].items()}


def derived(data,model):
    p=values(data);usable=p['nearPreviewM']-p['distanceMarginM']
    if usable<=0:raise ValueError('No usable near distance')
    a=p['planningDecelerationMps2'];t=p['latencyS']
    cap=math.sqrt((a*t)**2+2*a*usable)-a*t
    circumference=nominal_circumference(model)
    if 'encoderProfile' in data:
        path=child(ROOT,data['encoderProfile']);encoder=load_encoder(path)
        scales={wheel:encoder[wheel]['counts_per_wheel_revolution']/circumference for wheel in ('left','right')}
        source={'kind':'adopted-wheel-cpr-with-nominal-circumference',
                'encoderProfile':data['encoderProfile'],'encoderProfileSha256':sha256(path),
                'metrologicalRepeatabilityPassed':encoder['metrological_repeatability_passed']}
    else:
        scales={wheel:p['encoderWheelCprHypothesis']/circumference for wheel in ('left','right')}
        source={'kind':'legacy-conditional-cpr-with-nominal-circumference'}
    return {'nearBrakingCapMps':cap,'nearCapReferenceMps':min(cap,p['maximumSpeedMps']),
            'referenceTicksPerMetreLeft':scales['left'],'referenceTicksPerMetreRight':scales['right'],
            'encoderScaleSource':source,'nominalCircumferenceM':circumference,'measuredMetricScale':False,
            'planningDecelerationDistanceAtMaxM':p['maximumSpeedMps']**2/(2*a),
            'latencyDistanceAtMaxM':p['maximumSpeedMps']*t,'productionApproved':False}


def write_c_header(path,data,model):
    p=values(data);d=derived(data,model)
    names={'MAX_SPEED':'maximumSpeedMps','LAT_ACCEL':'lateralAccelerationMps2','ACCEL':'accelerationMps2',
           'BRAKE':'brakingDecelerationMps2','PLAN_BRAKE':'planningDecelerationMps2','JERK_ACC':'jerkAccelerationMps3','JERK_BRAKE':'jerkBrakingMps3',
           'LATENCY':'latencyS','NEAR_DISTANCE':'nearPreviewM','FULL_DISTANCE':'fullPreviewM','MARGIN':'distanceMarginM',
           'FAR_DISTANCE':'farDistanceM','PLANT_SPEED':'plantFreeSpeedMps','PLANT_TAU':'plantTimeConstantS'}
    result=['/* Generated from unapproved Host reference JSON; NOT production configuration. */',
            '#ifndef SP_REFERENCE_CONFIG_H','#define SP_REFERENCE_CONFIG_H']
    def float_macro(name,value):
        number=format(value,'.9g')
        if '.' not in number and 'e' not in number:number+='.0'
        result.append('#define REF_'+name+' '+number+'f')
    for name,key in names.items():float_macro(name,p[key])
    for name,key in {'FAR_MS':'farBudgetMs','NEAR_MS':'nearWindowMs','NEAR_FRAMES':'nearFrames','FAR_FRAMES':'farFrames',
                     'STOP_MS':'stopTimeoutMs','KP':'piKpQ8','KI':'piKiQ8'}.items():
        if type(p[key]) is not int:raise ValueError('Integer reference required: '+key)
        result.append('#define REF_'+name+' '+str(p[key])+'U')
    float_macro('NEAR_CAP',d['nearCapReferenceMps'])
    float_macro('TICKS_PER_M_LEFT',d['referenceTicksPerMetreLeft'])
    float_macro('TICKS_PER_M_RIGHT',d['referenceTicksPerMetreRight'])
    result.append('#endif')
    Path(path).write_text('\n'.join(result)+'\n',encoding='utf-8')
    return d
