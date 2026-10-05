#include "pe_hare.h"
#include <math.h>
#include <string.h>

#define PI 3.14159265358979323846f
#define SKIN .003f
#define FLOOR_Y .65f
/* Source: PrototypeMonster13, Plot352, BTPatrol(BTIdle.cs:410),
 * BTMoveToTarget(BTTarget.cs:599). No compiled MLepusHare topology is claimed. */
static PeVec3 add(PeVec3 a,PeVec3 b){return(PeVec3){a.x+b.x,a.y+b.y,a.z+b.z};}
static PeVec3 mul(PeVec3 a,float b){return(PeVec3){a.x*b,a.y*b,a.z*b};}
static PeVec3 sub(PeVec3 a,PeVec3 b){return add(a,mul(b,-1));}
static float dot(PeVec3 a,PeVec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static float norm(PeVec3 a){return sqrtf(dot(a,a));}
static bool vector_valid(PeVec3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z)&&fabsf(p.x)<=100000&&fabsf(p.y)<=100000&&fabsf(p.z)<=100000;}
static float clamp(float x,float a,float b){return fminf(b,fmaxf(a,x));}
static float random01(PeHare *h){
    uint64_t x=h->rng;x^=x>>12;x^=x<<25;x^=x>>27;h->rng=x;
    return(float)((x*UINT64_C(2685821657736338717))>>40)*(1.0f/16777216.0f);
}
static bool valid(const PeHare *h){
    return h&&h->ready&&h->rng&&vector_valid(h->position)&&vector_valid(h->spawn)&&vector_valid(h->velocity)&&vector_valid(h->patrol_goal)&&
        isfinite(h->scale)&&h->scale>=.8f&&h->scale<=1.3f&&h->radius==.6f*h->scale&&h->height==1.5f*h->scale&&
        isfinite(h->yaw_degrees)&&fabsf(h->yaw_degrees)<=360&&isfinite(h->mode_age)&&h->mode_age>=0&&h->mode_age<=100000&&
        isfinite(h->mode_duration)&&h->mode_duration>=0&&h->mode_duration<=100000&&isfinite(h->stuck_seconds)&&h->stuck_seconds>=0&&h->stuck_seconds<=100000&&isfinite(h->retarget_age)&&h->retarget_age>=0&&h->retarget_age<=100000&&
        isfinite(h->max_hp)&&h->max_hp>0&&isfinite(h->attack)&&h->attack>0&&isfinite(h->defense)&&h->defense>0&&h->mode>=PE_HARE_IDLE&&h->mode<=PE_HARE_HOLD;
}
static bool target_valid(const PeHareTarget *t){
    return !t||!t->accepted||(vector_valid(t->position)&&vector_valid(t->velocity)&&isfinite(t->radius)&&t->radius>0&&t->radius<=100&&
        isfinite(t->height)&&t->height>=2*t->radius&&t->height<=200&&isfinite(t->yaw_degrees)&&fabsf(t->yaw_degrees)<=360&&
        (t->approach==PE_HARE_MELEE||t->approach==PE_HARE_POUNCE));
}
static PeSceneHit sweep(const PeHare *h,const PeSceneCollision *s,PeVec3 p,PeVec3 d){return pe_scene_capsule_sweep(s,p,h->radius,h->height,d);}
static PeVec3 contact(PeVec3 p,PeVec3 d,PeSceneHit hit){return add(add(p,mul(d,hit.fraction)),mul(hit.normal,SKIN));}
bool pe_hare_init(PeHare *h,const PeSceneCollision *s,uint64_t seed,float scale,bool easy){
    if(!h||!s||!isfinite(scale)||(scale!=0&&(scale<.8f||scale>1.3f)))return false;
    PeHare next={0};next.rng=seed?seed:UINT64_C(0x9e3779b97f4a7c15);
    next.scale=scale?scale:.8f+.5f*random01(&next);next.radius=.6f*next.scale;next.height=1.5f*next.scale;
    next.spawn=next.position=(PeVec3){15,2,12};
    next.max_hp=212*next.scale*(easy?.5f:1);next.attack=64*next.scale*(easy?.5f:1);next.defense=29*next.scale*(easy?.5f:1);
    if(pe_scene_capsule_overlap(s,next.position,next.radius,next.height))return false;
    PeVec3 down={0,-5,0};PeSceneHit ground=sweep(&next,s,next.position,down);
    if(!ground.hit||ground.normal.y<FLOOR_Y)return false;
    next.position=contact(next.position,down,ground);
    if(pe_scene_capsule_overlap(s,next.position,next.radius,next.height))return false;
    next.ready=next.grounded=true;next.mode=PE_HARE_IDLE;next.mode_duration=6+2*random01(&next);
    *h=next;return true;
}
static PeVec3 closest(PeVec3 p,float radius,float height,float yaw,PeVec3 other){
    float a=yaw*PI/180,c=cosf(a),s=sinf(a);PeVec3 d=sub(other,p);
    float x=clamp(c*d.x-s*d.z,-radius,radius),z=clamp(s*d.x+c*d.z,-radius,radius);
    return add(p,(PeVec3){c*x+s*z,clamp(d.y,0,height),-s*x+c*z});
}
PeHareEngagement pe_hare_engagement(const PeHare *h,const PeSceneCollision *scene,const PeHareTarget *t){
    PeHareEngagement result={0};if(!valid(h)||!scene||!t||!t->accepted||!target_valid(t))return result;
    result.present=true;
    PeVec3 a=closest(h->position,h->radius,h->height,h->yaw_degrees,t->position);
    PeVec3 b=closest(t->position,t->radius,t->height,t->yaw_degrees,h->position);
    PeVec3 gap=sub(b,a),direction=sub(t->position,h->position);
    bool in_height=(h->position.y>=t->position.y-.5f&&h->position.y<=t->position.y+t->height+.5f)||
        (h->position.y+h->height>=t->position.y-.5f&&h->position.y+h->height<=t->position.y+t->height+.5f);
    if(in_height)gap.y=0;
    if(in_height&&gap.x*direction.x+gap.z*direction.z<0)gap=(PeVec3){0};
    result.gap=norm(gap);
    float heading=atan2f(direction.x,direction.z)*180/PI;
    float angle=fabsf(remainderf(heading-h->yaw_degrees,360));
    if(hypotf(direction.x,direction.z)<.00001f)angle=0;
    result.facing_degrees=angle;
    PeVec3 eye=add(h->position,(PeVec3){0,h->height*.5f,0});
    PeVec3 to=sub(add(t->position,(PeVec3){0,t->height*.5f,0}),eye);float distance=norm(to);
    PeSceneHit hit=pe_scene_raycast(scene,eye,to,distance);
    result.unobstructed=!hit.hit||hit.distance>=distance-SKIN;
    result.melee_geometry=result.unobstructed&&result.gap<=.5f&&angle<=10;
    result.pounce_geometry=result.unobstructed&&result.gap>=2&&result.gap<=5&&angle<=15;
    return result;
}
/* Ground projection parallels GetCorrectHeight's roof-then-down query, without
 * Unity water/layer/polar-shield systems. Invalid destinations are retried. */
static bool ground_goal(const PeHare *h,const PeSceneCollision *scene,PeVec3 *p){
    PeSceneHit roof=pe_scene_raycast(scene,*p,(PeVec3){0,1,0},128);
    PeVec3 top=roof.hit?add(roof.position,(PeVec3){0,-.1f,0}):add(*p,(PeVec3){0,128,0});
    PeSceneHit floor=pe_scene_raycast(scene,top,(PeVec3){0,-1,0},256);
    if(!floor.hit||floor.normal.y<FLOOR_Y)return false;
    *p=add(floor.position,(PeVec3){0,SKIN,0});
    return !pe_scene_capsule_overlap(scene,*p,h->radius,h->height);
}
static void choose_goal(PeHare *h,const PeSceneCollision *scene){
    /* BTPatrol tries5 calls*5 samples forward, then5*5 reverse; final raw
     * fallback preserves the authored radius even when outside a small room.
     * The controller will stop at walls/edges, not teleport to this goal. */
    h->goal_valid=false;
    for(int attempt=0;attempt<51;attempt++){
        float offset=(attempt<25||attempt==50)?-75+150*random01(h):-135+270*random01(h);
        float a=(h->yaw_degrees+offset)*PI/180;
        PeVec3 p=add(h->spawn,(PeVec3){30*sinf(a),0,30*cosf(a)});
        if(attempt==50||ground_goal(h,scene,&p)){h->patrol_goal=p;h->goal_valid=true;break;}
    }
}
static void leisure(PeHare *h,const PeSceneCollision *scene){
    if(h->mode!=PE_HARE_IDLE&&h->mode!=PE_HARE_PATROL){h->mode=PE_HARE_IDLE;h->mode_age=0;h->mode_duration=6+2*random01(h);h->goal_valid=false;}
    if(h->mode_age>h->mode_duration||(h->mode==PE_HARE_PATROL&&h->stuck_seconds>5)){
        h->mode_age=0;h->stuck_seconds=0;h->decisions++;
        if(random01(h)<=.6f){h->mode=PE_HARE_PATROL;h->mode_duration=10+10*random01(h);choose_goal(h,scene);}
        else{h->mode=PE_HARE_IDLE;h->mode_duration=6+2*random01(h);h->goal_valid=false;}
    }
    if(h->mode==PE_HARE_PATROL&&(!h->goal_valid||hypotf(h->position.x-h->patrol_goal.x,h->position.z-h->patrol_goal.z)<2||(h->stuck_seconds>2&&h->retarget_age>2))){
        choose_goal(h,scene);h->retarget_age=0;
    }
}
static PeVec3 slide(PeHare *h,const PeSceneCollision *s,PeVec3 delta){
    PeVec3 p=h->position;
    for(int k=0;k<5&&norm(delta)>.00001f;k++){
        PeSceneHit hit=sweep(h,s,p,delta);if(!hit.hit){p=add(p,delta);break;}
        p=contact(p,delta,hit);delta=mul(delta,1-hit.fraction);
        float into=dot(delta,hit.normal);if(into<0)delta=sub(delta,mul(hit.normal,into));
        into=dot(h->velocity,hit.normal);if(into<0)h->velocity=sub(h->velocity,mul(hit.normal,into));
    }
    return p;
}
bool pe_hare_step(PeHare *h,const PeSceneCollision *s,const PeHareTarget *target){
    if(!valid(h)||!s||!target_valid(target))return false;
    PeHare next=*h;next.mode_age=fminf(next.mode_age+PE_HARE_DT,99999);next.retarget_age=fminf(next.retarget_age+PE_HARE_DT,99999);
    PeVec3 desired={0};float speed=0,max_travel=100000;bool turn=false;
    if(target&&target->accepted){
        PeHareEngagement e=pe_hare_engagement(&next,s,target);
        float minimum=target->approach==PE_HARE_MELEE?0:2,maximum=target->approach==PE_HARE_MELEE?.5f:5;
        desired=sub(target->position,next.position);desired.y=0;turn=true;
        if(e.gap>maximum||!e.unobstructed){
            next.mode=PE_HARE_APPROACH;speed=6.5f;
            /* Source predicts one meter along normalized target velocity. */
            float v=norm(target->velocity);if(v>.00001f)desired=add(desired,mul(target->velocity,1/v));desired.y=0;
            if(e.unobstructed)max_travel=fmaxf(0,e.gap-maximum);
        }else if(e.gap<minimum){next.mode=PE_HARE_RETREAT;speed=-2;max_travel=minimum-e.gap;}
        else next.mode=PE_HARE_HOLD;
        next.goal_valid=false;next.mode_age=0;next.stuck_seconds=0;
    }else{
        leisure(&next,s);
        if(next.mode==PE_HARE_PATROL&&next.goal_valid){desired=sub(next.patrol_goal,next.position);desired.y=0;speed=2.5f;turn=true;}
    }
    float distance=hypotf(desired.x,desired.z);
    if(turn&&distance>.00001f){
        float yaw=atan2f(desired.x,desired.z)*180/PI;
        float difference=remainderf(yaw-next.yaw_degrees,360);
        next.yaw_degrees=remainderf(next.yaw_degrees+clamp(difference,-150*PE_HARE_DT,150*PE_HARE_DT),360);
    }
    float actual_speed=fminf(fabsf(speed),max_travel/PE_HARE_DT)*(speed<0?-1:1);
    next.velocity.x=distance>.00001f?desired.x/distance*actual_speed:0;
    next.velocity.z=distance>.00001f?desired.z/distance*actual_speed:0;
    PeSceneHit below=sweep(&next,s,next.position,(PeVec3){0,-.06f,0});
    next.grounded=next.velocity.y<=0&&below.hit&&below.normal.y>=FLOOR_Y;
    if(next.grounded)next.velocity.y=0;
    next.velocity.y-=10*PE_HARE_DT; /* Serialized hare motor gravity. */
    PeVec3 before=next.position;
    PeVec3 proposed=slide(&next,s,mul(next.velocity,PE_HARE_DT));
    /* No navigation mesh is available: conservatively refuse grounded motion
     * off a ledge rather than falling forever toward an invalid patrol goal. */
    if(next.grounded){
        PeSceneHit support=sweep(&next,s,proposed,(PeVec3){0,-.15f,0});
        PeSceneHit under_center=pe_scene_raycast(s,add(proposed,(PeVec3){0,.1f,0}),(PeVec3){0,-1,0},.25f);
        if(!support.hit||support.normal.y<FLOOR_Y||!under_center.hit||under_center.normal.y<FLOOR_Y){proposed=before;next.velocity=(PeVec3){0};}
    }
    if(!pe_scene_capsule_overlap(s,proposed,next.radius,next.height))next.position=proposed;
    else next.velocity=(PeVec3){0};
    if(next.velocity.y<=0){
        PeVec3 down={0,-.08f,0};PeSceneHit floor=sweep(&next,s,next.position,down);
        if(floor.hit&&floor.normal.y>=FLOOR_Y){
            PeVec3 p=contact(next.position,down,floor);
            if(!pe_scene_capsule_overlap(s,p,next.radius,next.height)){next.position=p;next.velocity.y=0;next.grounded=true;}
        }else next.grounded=false;
    }
    if(next.mode==PE_HARE_PATROL){
        float moved=hypotf(next.position.x-before.x,next.position.z-before.z);
        next.stuck_seconds=moved<.001f?next.stuck_seconds+PE_HARE_DT:0;
    }
    if(!valid(&next))return false;
    *h=next;return true;
}
