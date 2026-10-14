#include "pe_grounder.h"
#include "../generated/pe_grounder_source.h"
#include <math.h>
#include <string.h>

typedef PeVec3 V;
typedef PeFbbikQuat Q;
static const V ZERO={0,0,0},UP={0,1,0};
static const Q IDENTITY={0,0,0,1};

static V add(V a,V b){return (V){a.x+b.x,a.y+b.y,a.z+b.z};}
static V sub(V a,V b){return (V){a.x-b.x,a.y-b.y,a.z-b.z};}
static V mul(V a,float s){return (V){a.x*s,a.y*s,a.z*s};}
static float dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static V cross(V a,V b){return (V){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static float mag(V a){return sqrtf(dot(a,a));}
static V unit(V a){float m=mag(a);return m>1e-5f?mul(a,1/m):ZERO;}
static float clampf(float v,float lo,float hi){return v<lo?lo:v>hi?hi:v;}   /* Mathf.Clamp order */
static float lerpf(float a,float b,float t){return a+(b-a)*clampf(t,0,1);}
static V lerp(V a,V b,float t){t=clampf(t,0,1);return add(a,mul(sub(b,a),t));}
static bool finite3(V v){return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z);}
static bool finiteq(Q q){return isfinite(q.x)&&isfinite(q.y)&&isfinite(q.z)&&isfinite(q.w);}
static Q qunit(Q q){
    float m=sqrtf(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    return m>1e-5f?(Q){q.x/m,q.y/m,q.z/m,q.w/m}:IDENTITY;
}
static V rotate_about(V v,V axis,float angle){   /* Rodrigues, unit axis */
    float c=cosf(angle),s=sinf(angle);
    return add(add(mul(v,c),mul(cross(axis,v),s)),mul(axis,dot(axis,v)*(1-c)));
}
/* Conventional adapters for engine natives. */
static Q from_to(V a,V b){
    a=unit(a);b=unit(b);float d=dot(a,b);
    if(d>=1-1e-6f)return IDENTITY;
    if(d<=-1+1e-6f){
        V axis=cross((V){1,0,0},a);if(dot(axis,axis)<1e-6f)axis=cross((V){0,1,0},a);
        axis=unit(axis);return (Q){axis.x,axis.y,axis.z,0};
    }
    V c=cross(a,b);return qunit((Q){c.x,c.y,c.z,1+d});
}
static Q slerp_unclamped(Q a,Q b,float t){
    float d=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
    if(d<0){b=(Q){-b.x,-b.y,-b.z,-b.w};d=-d;}
    if(d>0.9995f)return qunit((Q){a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t});
    float angle=acosf(clampf(d,-1,1)),s=sinf(angle),p=sinf((1-t)*angle)/s,q=sinf(t*angle)/s;
    return qunit((Q){a.x*p+b.x*q,a.y*p+b.y*q,a.z*p+b.z*q,a.w*p+b.w*q});
}
static Q slerp(Q a,Q b,float t){return slerp_unclamped(a,b,clampf(t,0,1));}
static float moved(float from,float to,float limit){
    float delta=to-from;return delta>0?from+fminf(delta,limit):from-fminf(-delta,limit);
}
static V rotate_towards(V from,V to,float max_radians,float max_magnitude){
    float a=mag(from),b=mag(to);
    if(a>1e-5f&&b>1e-5f){
        V fa=mul(from,1/a),tb=mul(to,1/b);float d=dot(fa,tb);
        if(d<=1-1e-6f){
            V axis;float angle;
            if(d<-1+1e-6f){axis=cross((V){1,0,0},fa);if(dot(axis,axis)<1e-6f)axis=cross((V){0,0,1},fa);axis=unit(axis);angle=max_radians;}
            else{axis=unit(cross(fa,tb));angle=fminf(max_radians,acosf(d));}
            return mul(rotate_about(fa,axis,angle),moved(a,b,max_magnitude));
        }
    }
    V delta=sub(to,from);float m=mag(delta);   /* MoveTowards */
    return m<=max_magnitude||m==0?to:add(from,mul(delta,max_magnitude/m));
}
/* Managed bodies, in compiled order. */
static float quat_angle(Q a,Q b){
    float d=fabsf(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w);return acosf(fminf(d,1))*2*57.29578f;
}
static Q quat_rotate_towards(Q from,Q to,float max_degrees){
    float angle=quat_angle(from,to);if(angle==0)return to;
    return slerp_unclamped(from,to,fminf(1,max_degrees/angle));
}
static float lerp_value(float value,float target,float up_speed,float down_speed,float delta_time){   /* Interp.LerpValue */
    if(value==target)return target;
    if(value<target)return clampf(value+delta_time*up_speed,-INFINITY,target);
    return clampf(value-delta_time*down_speed,target,INFINITY);
}
static V line_to_plane(V origin,V direction,V normal,V point){   /* V3Tools.LineToPlane */
    float a=dot(sub(point,origin),normal),b=dot(direction,normal);
    if(b==0)return ZERO;
    return add(origin,mul(unit(direction),a/b));
}

const PeGrounderSettings *pe_grounder_female_settings(void){return &pe_grounder_source_settings;}

bool pe_grounder_foot_rotation_limit(float angle,float *degrees){
    if(!degrees||!isfinite(angle))return false;
    const PeGrounderCurveKey *k=pe_grounder_source_limit_keys,*last=k+PE_GROUNDER_SOURCE_LIMIT_KEYS-1;
    if(angle<=k->time){*degrees=k->value;return true;}
    if(angle>=last->time){*degrees=last->value;return true;}
    while(angle>=k[1].time)k++;
    float h=k[1].time-k->time,x=(angle-k->time)/h,x2=x*x,x3=x2*x;
    float result=(2*x3-3*x2+1)*k->value+(x3-2*x2+x)*k->out_slope*h+(3*x2-2*x3)*k[1].value+(x3-x2)*k[1].in_slope*h;
    if(!isfinite(result))return false;
    *degrees=result;return true;
}
bool pe_grounder_set_live(PeGrounder *g,float forward_ground_angle,bool spine_effect){
    float limit;
    if(!g||!g->initiated||!pe_grounder_foot_rotation_limit(forward_ground_angle,&limit))return false;
    g->settings.max_foot_rotation_angle=limit;
    g->settings.spine_bend=spine_effect?pe_grounder_source_settings.spine_bend:0;
    return true;
}

static bool valid_settings(const PeGrounderSettings *s){
    const float *v=(const float *)s;
    for(size_t i=0;i<sizeof *s/sizeof(float);i++)if(!isfinite(v[i]))return false;
    return s->max_step>0;
}
bool pe_grounder_init(PeGrounder *g,const PeGrounderSettings *settings,PeGrounderStart start){
    if(!g||!settings||!valid_settings(settings)||!isfinite(start.delta_time)||!finite3(start.root_position))return false;
    for(unsigned i=0;i<PE_GROUNDER_LEGS;i++)if(!finite3(start.foot[i]))return false;
    PeGrounder next={.settings=*settings,.initiated=true,.pelvis_last_time=start.delta_time,.pelvis_last_root=start.root_position};
    for(unsigned i=0;i<PE_GROUNDER_LEGS;i++)
        next.leg[i]=(PeGrounderLeg){.last_time=start.delta_time,.last_position=start.foot[i],.ik_position=start.foot[i],.r=IDENTITY,.rotation_offset=IDENTITY};
    *g=next;return true;
}

typedef struct {const PeGrounderInput *in; PeGrounderSettings s; bool root_grounded;} Frame;

/* Leg.GetRaycastHit */
static PeGrounderHit cast(const Frame *f,V foot,V offset){
    V origin=add(foot,offset);
    PeGrounderHit hit={.point=sub(origin,mul(mul(UP,f->s.max_step),2)),.normal=UP};
    f->in->ray(f->in->context,add(origin,mul(UP,f->s.max_step)),mul(UP,-1),f->s.max_step*3,&hit);
    return hit;
}
/* Leg.GetHeightFromGround: the root's height over the point, whatever the foot's own height. */
static float height(const Frame *f,V foot,V point){
    return (foot.y-point.y)-(foot.y-f->in->root_position.y);
}
/* Leg.Process */
static void leg_process(const Frame *f,PeGrounderLeg *leg,V foot){
    const PeGrounderSettings *s=&f->s;
    float dt=f->in->time-leg->last_time;leg->last_time=f->in->time;
    if(dt==0)return;
    V velocity=mul(sub(foot,leg->last_position),1/dt);velocity.y=0;leg->last_position=foot;
    V prediction=mul(velocity,s->prediction);
    PeGrounderHit heel=cast(f,foot,ZERO);
    PeGrounderHit forward=cast(f,foot,add(mul(f->in->root_forward,s->foot_radius),prediction));
    PeGrounderHit side=cast(f,foot,mul(mul(f->in->root_right,s->foot_radius),0.5f));
    V normal=unit(cross(sub(forward.point,heel.point),sub(side.point,heel.point)));
    if(dot(normal,UP)<0)normal=mul(normal,-1);
    /* SetFootToPlane, with RotateNormal's per-frame turn limit */
    normal=rotate_towards(UP,normal,s->max_foot_rotation_angle*0.0174532924f,dt);
    Q to_hit=from_to(UP,normal);
    V on_plane=line_to_plane(add(foot,mul(UP,s->max_step)),mul(UP,-1),normal,heel.point);
    leg->height_from_ground=clampf(height(f,foot,on_plane),-INFINITY,height(f,foot,heel.point));
    leg->grounded=leg->height_from_ground<s->max_step;
    float target=clampf(leg->height_from_ground,-s->max_step,s->max_step);
    if(!f->root_grounded)target=0;
    leg->ik_offset=lerp_value(leg->ik_offset,target,s->foot_speed,s->foot_speed,f->in->delta_time);
    leg->ik_offset=lerpf(leg->ik_offset,target,dt*s->foot_speed);
    float limit=clampf(s->max_step-(foot.y-f->in->root_position.y),0,s->max_step);
    leg->ik_offset=clampf(leg->ik_offset,-limit,leg->ik_offset);
    Q wanted=s->max_foot_rotation_angle<=0?IDENTITY:s->max_foot_rotation_angle>=180?to_hit
            :quat_rotate_towards(IDENTITY,to_hit,s->max_foot_rotation_angle);
    leg->r=slerp(leg->r,wanted,dt*s->foot_rotation_speed);
    leg->ik_position=sub(foot,mul(UP,leg->ik_offset));
    leg->rotation_offset=s->foot_rotation_weight>=1?leg->r:slerp(IDENTITY,leg->r,s->foot_rotation_weight);
}

bool pe_grounder_update(PeGrounder *g,const PeGrounderInput *in,PeGrounderOutput *out){
    if(!g||!in||!out||!g->initiated||!in->ray||!isfinite(in->time)||!isfinite(in->delta_time)||!isfinite(in->weight))return false;
    if(!finite3(in->root_position)||!finite3(in->root_forward)||!finite3(in->root_right)||!finite3(in->root_up))return false;
    for(unsigned i=0;i<PE_GROUNDER_LEGS;i++)if(!finite3(in->foot[i]))return false;
    PeGrounder next=*g;PeGrounderOutput result={0};
    for(unsigned i=0;i<PE_GROUNDER_LEGS;i++)result.foot_rotation[i]=IDENTITY;
    float weight=clampf(in->weight,0,1);   /* GrounderFBBIK.Update */
    if(!in->enabled||weight<=0){*out=result;return true;}
    Frame f={.in=in,.s=next.settings};
    /* Grounding.Update clamps */
    f.s.foot_radius=clampf(f.s.foot_radius,9.999999747378752e-05f,f.s.max_step);
    f.s.pelvis_damper=clampf(f.s.pelvis_damper,0,1);
    f.s.max_foot_rotation_angle=clampf(f.s.max_foot_rotation_angle,0,90);
    if(f.s.prediction<0)f.s.prediction=0;
    if(f.s.foot_speed<0)f.s.foot_speed=0;
    /* GetRootHit(10): a miss keeps the eleven-step default distance */
    PeGrounderHit root={.point=sub(in->root_position,mul(mul(UP,f.s.max_step),10)),.distance=f.s.max_step*11};
    in->ray(in->context,add(in->root_position,mul(UP,f.s.max_step)),mul(UP,-1),f.s.max_step*11,&root);
    f.root_grounded=root.distance<f.s.max_step*2;
    float highest=-INFINITY,lowest=INFINITY;bool grounded=false;
    for(unsigned i=0;i<PE_GROUNDER_LEGS;i++){
        leg_process(&f,&next.leg[i],in->foot[i]);
        if(next.leg[i].ik_offset>highest)highest=next.leg[i].ik_offset;
        if(next.leg[i].ik_offset<lowest)lowest=next.leg[i].ik_offset;
        if(next.leg[i].grounded)grounded=true;
    }
    /* Pelvis.Process(-max*lowerPelvisWeight, -min*liftPelvisWeight, isGrounded) */
    float dt=in->time-next.pelvis_last_time;next.pelvis_last_time=in->time;
    if(dt>0){
        float target=-highest*f.s.lower_pelvis_weight+-lowest*f.s.lift_pelvis_weight;
        if(!f.root_grounded)target=0;
        next.pelvis_height=lerpf(next.pelvis_height,target,dt*f.s.pelvis_speed);
        float rise=in->root_position.y-next.pelvis_last_root.y;next.pelvis_last_root=in->root_position;
        next.pelvis_damper=lerp_value(next.pelvis_damper,grounded?1:0,1,10,in->delta_time);
        next.pelvis_height-=rise*f.s.pelvis_damper*next.pelvis_damper;
    }
    /* GrounderFBBIK.OnSolverUpdate */
    result.ran=true;result.grounded=grounded;result.root_grounded=f.root_grounded;
    result.pelvis_offset=mul(mul(UP,next.pelvis_height),weight);
    V moved_foot[PE_GROUNDER_LEGS];
    for(unsigned i=0;i<PE_GROUNDER_LEGS;i++){
        moved_foot[i]=add(in->foot[i],result.pelvis_offset);
        result.foot_offset[i]=mul(sub(next.leg[i].ik_position,moved_foot[i]),weight);
        result.foot_rotation[i]=slerp(IDENTITY,next.leg[i].rotation_offset,weight);
    }
    if(f.s.spine_bend!=0){
        float speed=f.s.spine_speed<0?0:f.s.spine_speed;
        V target=ZERO;
        for(unsigned i=0;i<PE_GROUNDER_LEGS;i++){   /* GetLegSpineBendVector */
            V tangent=sub(moved_foot[i],in->root_position);tangent.y=0;
            float facing=(dot(in->root_forward,unit(tangent))+1)*0.5f;
            target=add(target,mul(mul(tangent,mag(sub(next.leg[i].ik_position,moved_foot[i]))),facing));
        }
        next.spine_offset=lerp(next.spine_offset,mul(mul(target,weight),f.s.spine_bend),in->delta_time*speed);
        V vertical=mul(in->root_up,mag(next.spine_offset));
        for(unsigned e=0;e<PE_FBBIK_EFFECTORS;e++)
            result.effector_offset[e]=add(mul(next.spine_offset,f.s.spine_horizontal[e]),mul(vertical,f.s.spine_vertical[e]));
    }
    if(!finite3(result.pelvis_offset)||!finite3(next.spine_offset)||!isfinite(next.pelvis_height)||!isfinite(next.pelvis_damper))return false;
    for(unsigned i=0;i<PE_GROUNDER_LEGS;i++)
        if(!finite3(result.foot_offset[i])||!finiteq(result.foot_rotation[i])||!isfinite(next.leg[i].ik_offset)||!finiteq(next.leg[i].r))return false;
    for(unsigned e=0;e<PE_FBBIK_EFFECTORS;e++)if(!finite3(result.effector_offset[e]))return false;
    *g=next;*out=result;return true;
}
