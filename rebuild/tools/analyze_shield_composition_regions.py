#!/usr/bin/env python3
"""Compare local/model-space body regions after strict clone replay readback."""
import argparse,json,math
from pathlib import Path
from analyze_shield_composition import multiply, analyze, finite, matrix
from inventory_shield_layers import sha
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def norm(a):return math.sqrt(dot(a,a))
def unit(a):
 for x in a:finite(x)
 length=norm(a)
 if length<=1e-12:raise ValueError('Degenerate matrix basis')
 return [x/length for x in a]
def cross(a,b):return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
def rotation(m):
 m=matrix(m)
 c=[[m[r*4+k] for r in range(3)] for k in range(3)]
 x=unit(c[0]);y=unit([a-dot(c[1],x)*b for a,b in zip(c[1],x)]);z=cross(x,y)
 determinant=dot(z,c[2])
 if abs(determinant)<=1e-12:raise ValueError('Degenerate matrix basis')
 sign=1 if determinant>0 else -1
 return (x,y,[v*sign for v in z],sign)
def angle(a,b):
 aa,bb=rotation(a),rotation(b)
 if aa[3]!=bb[3]:raise ValueError('Mismatching reflected basis')
 return math.degrees(math.acos(max(-1,min(1,(sum(dot(x,y) for x,y in zip(aa[:3],bb[:3]))-1)/2))))
def distance(a,b):return math.sqrt(sum((a[i]-b[i])**2 for i in (3,7,11)))
def review(capture,plan_path):
 checked=analyze(capture,plan_path)
 if not checked['readback_matches']:raise ValueError('Regional review requires matching state/clock/weight readback')
 plan=json.loads(plan_path.read_text())
 frames=[json.loads(l) for l in capture.read_text().splitlines() if json.loads(l)['type']=='frame']
 out=[]
 for f,s in zip(frames,plan['frames']):
  sb={b['path']:b for b in s['bones']};cb={b['path']:b for b in f['bones']};values=[]
  for name,path in s['human_paths'].items():
   a=multiply(s['root_inverse_world'],sb[path]['world']);b=cb[path]['model'];al=sb[path]['local'];bl=cb[path]['local']
   values.append({'bone':name,'path':path,'model_position_m':distance(a,b),'local_position_m':distance(al,bl),
                  'model_rotation_degrees':angle(a,b),'local_rotation_degrees':angle(al,bl),
                  'model_matrix_max_abs':max(abs(x-y) for x,y in zip(a,b)),
                  'local_matrix_max_abs':max(abs(x-y) for x,y in zip(al,bl)),
                  'local_scale_error':max(abs(norm([al[r*4+k] for r in range(3)])-norm([bl[r*4+k] for r in range(3)])) for k in range(3))})
  groups={
   'shield_arm':[v for v in values if v['bone'] in ('LeftShoulder','LeftUpperArm','LeftLowerArm','LeftHand')],
   'shield_fingers':[v for v in values if v['bone'].startswith(('LeftThumb','LeftIndex','LeftRing'))],
   'lower_body':[v for v in values if any(part in v['bone'] for part in ('Leg','Foot','Toes'))],
   'head_neck':[v for v in values if v['bone'] in ('Neck','Head','LeftEye','Jaw')],
   'trunk':[v for v in values if v['bone'] in ('Hips','Spine','Chest')],
   'weapon_arm':[v for v in values if v['bone'] in ('RightShoulder','RightUpperArm','RightLowerArm','RightHand')]}
  summary={k:{'bones':len(v),'max_model_position_m':max(x['model_position_m'] for x in v),
              'max_local_position_m':max(x['local_position_m'] for x in v),
              'max_model_rotation_degrees':max(x['model_rotation_degrees'] for x in v),
              'max_local_rotation_degrees':max(x['local_rotation_degrees'] for x in v)} for k,v in groups.items()}
  out.append({'source_sample':s['sample'],'groups':summary,'bones':values})
 return {'schema':'pe-shield-composition-regions-1','capture_sha256':sha(capture),'plan_sha256':sha(plan_path),
         'source_live_capture_sha256':plan['capture_sha256'],'bundle_sha256':plan['bundle_sha256'],'frames':out,
         'rotation_method':'Orthogonalize matrix basis columns with Gram-Schmidt, require matching handedness and retain common reflected basis; compute acos((trace(Ra^T Rb)-1)/2). Local and model rotations are separate.',
         'limits':['No live IK target/solver state or same-pose clone bind snapshot was observed.',
                   'Raw local translation/scale differences are diagnostics, not proof of character proportion differences.',
                   'No tolerance relaxed or runtime asset installed.']}


def main(argv=None):
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture',type=Path);p.add_argument('--plan',type=Path,required=True);p.add_argument('--report',type=Path,required=True)
 a=p.parse_args(argv)
 try:
  if a.report.exists() or a.report.resolve() in (a.capture.resolve(),a.plan.resolve()):raise ValueError('Fresh report required')
  result=review(a.capture,a.plan);a.report.parent.mkdir(parents=True,exist_ok=True);a.report.write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
 except (OSError,ValueError,KeyError,TypeError) as e:p.error(str(e))
 print(json.dumps({'frames':len(result['frames']),'runtime_pose_export_ready':False}))


if __name__=='__main__':main()
