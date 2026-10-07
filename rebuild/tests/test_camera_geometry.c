#include "pe_camera_geometry.h"
#include "../generated/pe_camera_geometry_source.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    PeCameraGeometryQuery calls[128];unsigned count,fail_at,invalid_at;
    float target_threshold;bool eye_overlap,block_lines,block_radials;
    unsigned clear_line;
    bool clamp_hit,rise_hit;float clamp_distance,rise_distance;
    bool plane;float plane_z;
} Fixture;
static bool closef(float a,float b){return fabsf(a-b)<.00002f;}
static bool equal3(PeVec3 a,PeVec3 b){return closef(a.x,b.x)&&closef(a.y,b.y)&&closef(a.z,b.z);}
static PeCameraGeometryInput input(void){return(PeCameraGeometryInput){.position={0,0,-6},.forward={0,0,1},.right={1,0,0},.up={0,1,0},.distance=6,.near_clip=.8f,.fov_degrees=60,.aspect=16.f/9,.activity=50};}
static unsigned count_kind(const Fixture *f,PeCameraGeometryQueryKind kind){unsigned n=0;for(unsigned j=0;j<f->count;j++)n+=f->calls[j].kind==kind;return n;}
static bool query(void *context,const PeCameraGeometryQuery *q,PeCameraGeometryHit *h){
    Fixture *f=context;assert(f->count<128);f->calls[f->count++]=*q;
    assert(q->mask==8468481&&q->ignore_triggers);
    if(f->fail_at==f->count)return false;
    if(f->invalid_at==f->count){h->distance=NAN;return true;}
    *h=(PeCameraGeometryHit){0};
    if(f->plane){
        if(q->kind==PE_CAMERA_GEOMETRY_OVERLAP){h->hit=fabsf(q->origin.z-f->plane_z)<=q->radius;return true;}
        if(q->direction.z==0)return true;
        float radius=q->kind==PE_CAMERA_GEOMETRY_SPHERE_CAST?q->radius:0;
        /* Infinite plane, initial-overlap misses for sphere casts only. This
           is an ANALYTICAL fixture contract, not a Unity backend claim. */
        if(radius>0&&fabsf(q->origin.z-f->plane_z)<=radius)return true;
        float boundary=f->plane_z+(q->origin.z<f->plane_z?-radius:radius);
        float distance=(boundary-q->origin.z)/q->direction.z;
        if(distance>=0&&distance<=q->distance){h->hit=true;h->distance=distance;}return true;
    }
    if(q->kind==PE_CAMERA_GEOMETRY_OVERLAP){
        bool target=fabsf(q->origin.x)<1e-5f&&fabsf(q->origin.y)<1e-5f&&fabsf(q->origin.z)<1e-5f;
        h->hit=target?(f->target_threshold>0&&q->radius>=f->target_threshold):f->eye_overlap;
    }else if(q->kind==PE_CAMERA_GEOMETRY_RAY)h->hit=f->block_radials;
    else if(q->kind==PE_CAMERA_GEOMETRY_LINE){h->hit=f->block_lines;unsigned n=count_kind(f,PE_CAMERA_GEOMETRY_LINE);if(f->clear_line&&n==f->clear_line)h->hit=false;}
    else if(q->direction.y>0){h->hit=f->rise_hit;h->distance=h->hit?f->rise_distance:0;}
    else {h->hit=f->clamp_hit;h->distance=h->hit?f->clamp_distance:0;}
    return true;
}
static void open_and_preview(void){
    PeCameraGeometryInput i=input(),before_i=i;PeCameraGeometryState s={0};PeCameraGeometryResult r;Fixture f={0};
    assert(pe_camera_geometry_evaluate(&s,&i,query,&f,&r));
    assert(!memcmp(&i,&before_i,sizeof i)&&s.rise==0&&r.rise==0&&!r.clamped&&closef(r.distance,6)&&equal3(r.position,i.position));
    float ur=sqrtf(1+i.aspect*i.aspect)*tanf(30*.0174532924f);
    assert(closef(r.near_clip,.8f)&&closef(r.ncr,.8f*ur));
    assert(f.count==5&&f.calls[0].kind==PE_CAMERA_GEOMETRY_OVERLAP&&f.calls[1].kind==PE_CAMERA_GEOMETRY_OVERLAP);
    assert(f.calls[2].kind==PE_CAMERA_GEOMETRY_LINE&&f.calls[3].kind==PE_CAMERA_GEOMETRY_OVERLAP&&f.calls[4].kind==PE_CAMERA_GEOMETRY_SPHERE_CAST);
    assert(closef(f.calls[0].radius,.05f)&&closef(f.calls[1].radius,r.ncr)&&closef(f.calls[4].radius,r.ncr-.02f));
    i.activity=0;f=(Fixture){0};assert(pe_camera_geometry_evaluate(&s,&i,query,&f,&r)&&closef(r.near_clip,.32f));
    i.activity=50;i.aspect=1;f=(Fixture){0};assert(pe_camera_geometry_evaluate(&s,&i,query,&f,&r)&&closef(r.ncr,.8f*sqrtf(2)*tanf(30*.0174532924f)));
    /* Source does not clamp max upward to its nominal minimum endpoint. */
    i.near_clip=.04f;f=(Fixture){0};assert(pe_camera_geometry_evaluate(&s,&i,query,&f,&r)&&r.ncr<.05f&&closef(r.near_clip,.04f));
    f=(Fixture){.target_threshold=.01f};assert(pe_camera_geometry_evaluate(&s,&i,query,&f,&r)&&closef(r.ncr,.05f)&&r.near_clip>i.near_clip);
}
static void search_and_rings(void){
    PeCameraGeometryInput i=input();PeCameraGeometryState s={0};PeCameraGeometryResult r;Fixture f={.target_threshold=.4f};
    assert(pe_camera_geometry_evaluate(&s,&i,query,&f,&r));assert(r.ncr<.4f&&.4f-r.ncr<=.005f);
    assert(f.calls[0].radius==.05f&&f.calls[1].radius>.9f);
    float low=.05f,high=f.calls[1].radius;unsigned n=2;
    while(high-low>.005f){float mid=(low+high)*.5f;assert(closef(f.calls[n++].radius,mid));if(mid>=.4f)high=mid;else low=mid;}
    assert(closef(r.ncr,low));
    f=(Fixture){.target_threshold=.01f};assert(pe_camera_geometry_evaluate(&s,&i,query,&f,&r));assert(closef(r.ncr,.05f));
    assert(f.calls[1].kind==PE_CAMERA_GEOMETRY_LINE); /* occupied minimum skips max */
    f=(Fixture){.block_lines=true};assert(pe_camera_geometry_evaluate(&s,&i,query,&f,&r));
    assert(count_kind(&f,PE_CAMERA_GEOMETRY_LINE)==16&&count_kind(&f,PE_CAMERA_GEOMETRY_RAY)==12&&count_kind(&f,PE_CAMERA_GEOMETRY_OVERLAP)==2&&count_kind(&f,PE_CAMERA_GEOMETRY_SPHERE_CAST)==2&&!r.clamped);
    n=2;for(int ring=0;ring<3;ring++)for(int k=0;k<(ring<2?4:8);k++){
        if(ring){assert(f.calls[n].kind==PE_CAMERA_GEOMETRY_RAY&&closef(f.calls[n].distance,ring*1.5f));n++;}
        assert(f.calls[n].kind==PE_CAMERA_GEOMETRY_LINE);
        float angle=k*(360/(ring<2?4:8))*.0174532924f;
        assert(equal3(f.calls[n].origin,(PeVec3){ring*1.5f*cosf(angle),ring*1.5f*sinf(angle),0}));n++;
    }
    assert(f.calls[n++].kind==PE_CAMERA_GEOMETRY_SPHERE_CAST&&f.calls[n].kind==PE_CAMERA_GEOMETRY_SPHERE_CAST&&n+1==f.count);
    f=(Fixture){.block_lines=true,.clear_line=5};assert(pe_camera_geometry_evaluate(&s,&i,query,&f,&r));
    assert(count_kind(&f,PE_CAMERA_GEOMETRY_LINE)==5&&count_kind(&f,PE_CAMERA_GEOMETRY_RAY)==1&&count_kind(&f,PE_CAMERA_GEOMETRY_SPHERE_CAST)==1);
    f=(Fixture){.block_lines=true,.block_radials=true};assert(pe_camera_geometry_evaluate(&s,&i,query,&f,&r));
    assert(count_kind(&f,PE_CAMERA_GEOMETRY_LINE)==4&&count_kind(&f,PE_CAMERA_GEOMETRY_RAY)==12);
}
static void clamp_and_rise(void){
    PeCameraGeometryInput i=input();PeCameraGeometryState s={0};PeCameraGeometryResult a,b;Fixture f={.eye_overlap=true,.clamp_hit=true,.clamp_distance=0};
    assert(pe_camera_geometry_evaluate(&s,&i,query,&f,&a)&&a.clamped&&s.rise==0);
    assert(closef(a.distance,.8f)&&closef(a.rise,a.ncr*.1f));
    assert(closef(a.position.y,a.rise)&&closef(a.position.z,-.8f+.3f*a.rise));
    f=(Fixture){.eye_overlap=true,.clamp_hit=true,.clamp_distance=0};assert(pe_camera_geometry_evaluate(&s,&i,query,&f,&b));assert(!memcmp(&a,&b,sizeof a)&&s.rise==0);
    f=(Fixture){.eye_overlap=true,.clamp_hit=true,.clamp_distance=0};assert(pe_camera_geometry_step(&s,&i,query,&f,&b)&&closef(s.rise,a.rise));
    f=(Fixture){.eye_overlap=true,.clamp_hit=true,.clamp_distance=0};assert(pe_camera_geometry_step(&s,&i,query,&f,&b)&&closef(b.rise,a.ncr*.19f));
    assert(closef(b.distance,a.distance)); /* never include rise in saved Dist */
    i.shoot_mode=true;float before=s.rise;f=(Fixture){0};assert(pe_camera_geometry_step(&s,&i,query,&f,&b)&&closef(s.rise,before*.9f)&&!b.clamped);
    /* A far rise hit REPLACES, rather than clamps to, the default NCR cap. */
    s.rise=0;i.shoot_mode=false;f=(Fixture){.eye_overlap=true,.clamp_hit=true,.clamp_distance=0,.rise_hit=true,.rise_distance=2};
    assert(pe_camera_geometry_evaluate(&s,&i,query,&f,&a));assert(a.rise>a.ncr*.1f);
    assert(closef(a.rise,sqrtf(1.0001f-.25f)*1.6f*.1f));
    f=(Fixture){.eye_overlap=true,.clamp_hit=true,.clamp_distance=0,.rise_hit=true,.rise_distance=.1f};
    assert(pe_camera_geometry_evaluate(&s,&i,query,&f,&a)&&closef(a.rise,.0095f));
    /* Initial overlap is owned by callback: a miss must not manufacture clamp. */
    f=(Fixture){.eye_overlap=true};assert(pe_camera_geometry_evaluate(&s,&i,query,&f,&a)&&!a.clamped&&closef(a.distance,6));
    f=(Fixture){.plane=true,.plane_z=-3};assert(pe_camera_geometry_evaluate(&s,&i,query,&f,&a)&&a.clamped);
    assert(closef(a.distance,3-(a.ncr-.01f)+a.near_clip));
    f=(Fixture){.plane=true,.plane_z=-.01f};assert(pe_camera_geometry_evaluate(&s,&i,query,&f,&a)&&!a.clamped&&closef(a.ncr,.05f));
    /* Zero-length target-to-eye correction skips the clamp cast entirely. */
    i.position.z=-.8f;i.distance=.8f;f=(Fixture){.block_lines=true};
    assert(pe_camera_geometry_evaluate(&s,&i,query,&f,&a)&&!a.clamped&&count_kind(&f,PE_CAMERA_GEOMETRY_SPHERE_CAST)==1);
    /* Camera-relative up is not the world-up component of rise direction. */
    i=input();i.forward=(PeVec3){0,1,0};i.up=(PeVec3){0,0,-1};i.position=(PeVec3){0,-.2f,0};i.distance=.2f;f=(Fixture){0};
    assert(pe_camera_geometry_evaluate(&s,&i,query,&f,&a)&&a.rise>0&&closef(a.position.y,-.2f+1.3f*a.rise));
}
static void rollback(void){
    PeCameraGeometryInput i=input();PeCameraGeometryState s={.rise=.2f},before=s;PeCameraGeometryResult r,old;memset(&old,0xa5,sizeof old);r=old;
    Fixture complete={.target_threshold=.4f,.block_lines=true,.clamp_hit=true,.clamp_distance=1};assert(pe_camera_geometry_evaluate(&s,&i,query,&complete,&r));
    for(unsigned n=1;n<=complete.count;n++)for(int invalid=0;invalid<2;invalid++){
        Fixture f={.target_threshold=.4f,.block_lines=true,.clamp_hit=true,.clamp_distance=1,.fail_at=invalid?0:n,.invalid_at=invalid?n:0};r=old;
        assert(!pe_camera_geometry_step(&s,&i,query,&f,&r));assert(!memcmp(&s,&before,sizeof s)&&!memcmp(&r,&old,sizeof r));
    }
    for(int k=0;k<12;k++){
        PeCameraGeometryInput bad=i;PeCameraGeometryState state=s;Fixture f={0};r=old;
        switch(k){case 0:bad.position.x=NAN;break;case 1:bad.aspect=INFINITY;break;case 2:bad.aspect=0;break;
        case 3:bad.near_clip=0;break;case 4:bad.fov_degrees=180;break;case 5:bad.distance=-1;break;
        case 6:bad.activity=NAN;break;case 7:bad.right.x=-1;break;case 8:bad.up.z=.1f;break;
        case 9:state.rise=NAN;break;case 10:bad.aspect=FLT_MAX;break;default:bad.forward.z=.5f;break;}
        PeCameraGeometryState keep=state;assert(!pe_camera_geometry_step(&state,&bad,query,&f,&r));assert(!memcmp(&state,&keep,sizeof state)&&!memcmp(&r,&old,sizeof r));
    }
    Fixture f={.eye_overlap=true,.clamp_hit=true,.clamp_distance=999};r=old;assert(!pe_camera_geometry_step(&s,&i,query,&f,&r)&&!memcmp(&s,&before,sizeof s)&&!memcmp(&r,&old,sizeof r));
    assert(!pe_camera_geometry_step(NULL,&i,query,&f,&r)&&!pe_camera_geometry_evaluate(&s,NULL,query,&f,&r)&&!pe_camera_geometry_evaluate(&s,&i,NULL,&f,&r));
}
int main(void){open_and_preview();search_and_rings();clamp_and_rise();rollback();puts("Camera geometry: PASS (analytical callbacks; no host/physics integration)");return 0;}
