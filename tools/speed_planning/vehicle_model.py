"""C1 provisional geometry for offline design; no device or firmware access."""
import argparse
import json
import math
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
DEFAULT_PROFILE=ROOT/'profiles/c1-host-geometry-v0.1.json'


class Uncalibrated(ValueError):
    pass


def _positive(value,name):
    if isinstance(value,bool) or not isinstance(value,(int,float)) or not math.isfinite(value) or value<=0:
        raise ValueError('Invalid '+name)
    return value


def load_model(path=DEFAULT_PROFILE):
    def unique(pairs):
        obj={}
        for key,value in pairs:
            if key in obj:raise ValueError('Duplicate key: '+key)
            obj[key]=value
        return obj
    model=json.loads(Path(path).read_text(encoding='utf-8'),object_pairs_hook=unique,
                     parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)))
    if model['schemaVersion']!='smartcar.host-vehicle-model/1' or model['productionApproved'] is not False:
        raise ValueError('Only unapproved Host model profiles supported')
    for name,item in model['geometry'].items():
        _positive(item['value'],name)
        if item['status'] not in ('user-supplied-manufacturer-nominal','user-supplied-image-estimate'):
            raise ValueError('Unsupported geometry provenance')
    return model


def nominal_circumference(model):
    return math.pi*_positive(model['geometry']['tireDiameterM']['value'],'nominal diameter')


def turn_geometry(model,speed_mps,curvature_per_m):
    """Ideal, no-slip bicycle/Ackermann model using provisional dimensions.

    Positive curvature turns right. Output angle is a required effective angle,
    never a measurement or a conversion from the actual servo pulse.
    """
    if isinstance(speed_mps,bool) or not math.isfinite(speed_mps) or speed_mps<0:
        raise ValueError('Nonnegative speed required')
    if isinstance(curvature_per_m,bool) or not math.isfinite(curvature_per_m):
        raise ValueError('Finite curvature required')
    geo=model['geometry'];length=_positive(geo['wheelbaseM']['value'],'wheelbase')
    rear=_positive(geo['rearTrackM']['value'],'rear track');front=_positive(geo['frontTrackM']['value'],'front track')
    k=curvature_per_m
    if abs(k)*max(rear,front)/2>=1:raise ValueError('Turn crosses inner wheel / unsupported forward model')
    return {'rearLeftMps':speed_mps*(1+k*rear/2),'rearRightMps':speed_mps*(1-k*rear/2),
            'requiredEffectiveSteeringRad':math.atan(length*k),
            'requiredFrontLeftRad':math.atan(length*k/(1+k*front/2)),
            'requiredFrontRightRad':math.atan(length*k/(1-k*front/2)),
            'provenance':'ideal-kinematics-with-provisional-geometry','productionApproved':False,
            'servoPulseFromAngleUs':None}


def legacy_servo_command_us(model,normalized_command):
    """Reproduce existing command shaping; this does NOT produce wheel angle."""
    if isinstance(normalized_command,bool) or not math.isfinite(normalized_command) or abs(normalized_command)>1:
        raise ValueError('Normalized command must be in [-1,1]')
    s=model['servo'];u=normalized_command*(2/3+normalized_command**2/3)
    # Positive body command is right; source profile has lower pulse to right.
    endpoint=s['pulseRightUs'] if u>=0 else s['pulseLeftUs']
    return s['pulseCenterUs']+abs(u)*(endpoint-s['pulseCenterUs'])


def encoder_metres_per_count(model,wheel):
    if wheel not in ('left','right'):raise ValueError('Expected left or right wheel')
    cpr=model['encoder'][wheel]['countsPerWheelRev']
    circumference=model['measuredRollingCircumference'+wheel.capitalize()+'M']
    if cpr is None or circumference is None:
        raise Uncalibrated(wheel+': wheel counts/rev and measured rolling circumference required; nominal 1024/diameter are insufficient')
    return _positive(circumference,'measured circumference')/_positive(cpr,'wheel counts/rev')


def counts_to_speed(model,wheel,counts,interval_s):
    if isinstance(counts,bool) or not isinstance(counts,(int,float)) or not math.isfinite(counts):
        raise ValueError('Finite signed count required')
    scale=encoder_metres_per_count(model,wheel)
    return counts*model['encoder'][wheel]['forwardRawSign']*scale/_positive(interval_s,'sample interval')


def angle_from_pulse(model,pulse_us):
    if model['servo']['effectiveFrontAngleMap'] is None:
        raise Uncalibrated('No measured pulse -> effective front wheel angle map')
    raise Uncalibrated('A measured map requires a separate reviewed import contract')


def report(model):
    availability={}
    for wheel in ('left','right'):
        try:availability[wheel]={'available':True,'metresPerCount':encoder_metres_per_count(model,wheel)}
        except Uncalibrated as error:availability[wheel]={'available':False,'reason':str(error)}
    missing=[name for name,value in model['dynamicCalibration'].items() if value is None]
    return {'modelId':model['modelId'],'purpose':'Host design only','productionApproved':False,
            'nominalTireRadiusM':model['geometry']['tireDiameterM']['value']/2,
            'computedNominalCircumferenceM':nominal_circumference(model),
            'rollingCircumferenceMeasured':False,'encoderConversion':availability,
            'servoAngleMapAvailable':model['servo']['effectiveFrontAngleMap'] is not None,
            'dynamicCalibrationMissing':missing,
            'exampleRightTurn':turn_geometry(model,1,1)}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile',type=Path,default=DEFAULT_PROFILE)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args();result=report(load_model(args.profile))
    text=json.dumps(result,ensure_ascii=False,indent=2,allow_nan=False)+'\n'
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(text,encoding='utf-8')
    print(text,end='')


if __name__=='__main__':main()
