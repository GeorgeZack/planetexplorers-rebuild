#include "pe_ik_skin.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static unsigned failures;
#define CHECK(v) do{if(!(v)){fprintf(stderr,"ik_skin:%d %s\n",__LINE__,#v);failures++;}}while(0)
static float gap(const float a[3],PeVec3 b){float x=a[0]-b.x,y=a[1]-b.y,z=a[2]-b.z;return sqrtf(x*x+y*y+z*z);}
int pe_test_ik_skin(void){
    failures=0;const PeFbbikRig *rig=pe_fbbik_female_rig();
    PeFbbikTransform rest[PE_FBBIK_MAX_BONES]={0};static float delta[PE_FBBIK_MAX_BONES][12];
    CHECK(pe_fbbik_female_rest(rest));
    /* An unchanged pose gives the identity for every bone. */
    CHECK(pe_ik_skin_deltas(rig,rest,rest,delta));
    for(unsigned b=0;b<rig->bone_count;b++)for(int k=0;k<12;k++)CHECK(fabsf(delta[b][k]-(k%5==0?1:0))<1e-5f);   /* float rounding at metre scale */
    /* A real solve: each bone's delta carries its animated position, and a point fixed to it, to the solved one. */
    PeFbbik state;PeFbbikInput in;PeFbbikOutput out;
    CHECK(pe_fbbik_init(&state,rig,rest)&&pe_fbbik_input(&state,rest,&in));
    in.effector[0].position_offset=(PeVec3){.03f,-.15f,.02f};in.effector[5].position_offset=(PeVec3){.1f,.05f,0};
    CHECK(pe_fbbik_solve(&state,&in,&out)&&pe_ik_skin_deltas(rig,rest,out.local,delta));
    unsigned moved=0;
    for(unsigned b=0;b<rig->bone_count;b++){
        PeFbbikTransform a,s;CHECK(pe_fbbik_world(rig,rest,b,&a)&&pe_fbbik_world(rig,out.local,b,&s));
        float p[3]={a.position.x,a.position.y,a.position.z},q[3];pe_ik_skin_point(delta[b],p,q);CHECK(gap(q,s.position)<2e-6f);
        if(gap(p,s.position)>1e-3f)moved++;
        /* A point 10 cm along the bone's animated local x axis lands on the solved bone's local x axis. */
        float x=a.rotation.x*2,y=a.rotation.y*2,z=a.rotation.z*2;
        float ax[3]={1-(a.rotation.y*y+a.rotation.z*z),a.rotation.x*y+a.rotation.w*z,a.rotation.x*z-a.rotation.w*y};
        float sx[3]={1-(s.rotation.y*s.rotation.y*2+s.rotation.z*s.rotation.z*2),s.rotation.x*s.rotation.y*2+s.rotation.w*s.rotation.z*2,s.rotation.x*s.rotation.z*2-s.rotation.w*s.rotation.y*2};
        (void)x;float tip[3]={p[0]+ax[0]*.1f,p[1]+ax[1]*.1f,p[2]+ax[2]*.1f};pe_ik_skin_point(delta[b],tip,tip);
        PeVec3 expected={s.position.x+sx[0]*.1f,s.position.y+sx[1]*.1f,s.position.z+sx[2]*.1f};CHECK(gap(tip,expected)<5e-6f);
        /* Each delta is a rotation: orthonormal rows. */
        for(int i=0;i<3;i++)for(int j=0;j<3;j++){float d=0;for(int k=0;k<3;k++)d+=delta[b][i*4+k]*delta[b][j*4+k];CHECK(fabsf(d-(i==j))<1e-5f);}
    }
    CHECK(moved>10);
    /* Invalid input leaves the output alone. */
    static float kept[PE_FBBIK_MAX_BONES][12];memcpy(kept,delta,sizeof kept);
    PeFbbikTransform bad[PE_FBBIK_MAX_BONES];memcpy(bad,rest,sizeof bad);bad[4].position.y=NAN;
    CHECK(!pe_ik_skin_deltas(rig,rest,bad,delta)&&!pe_ik_skin_deltas(rig,0,rest,delta)&&!pe_ik_skin_deltas(0,rest,rest,delta)&&!pe_ik_skin_deltas(rig,rest,rest,0));
    CHECK(!memcmp(kept,delta,sizeof kept));
    return failures?1:0;
}
