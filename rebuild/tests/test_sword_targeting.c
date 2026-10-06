#include "pe_sword_targeting.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"sword-target:%d %s\n",__LINE__,#c);failures++;}}while(0)
static PeSwordTargetPart sphere(float x,float y,float z,float radius,bool center){
    return (PeSwordTargetPart){.capsule={{x,y,z},{x,y,z},radius},.enabled=true,.center_bone=center};
}
static PeSwordTargetCandidate candidate(uint64_t id,const PeSwordTargetPart*parts,size_t count){
    return (PeSwordTargetCandidate){.view_id=id,.layer=25,.overlaps_query=true,.conflicting=true,
        .has_defence=true,.parts=parts,.part_count=count};
}
static PeSwordTargetCandidate fallback(uint64_t id,float z){
    return (PeSwordTargetCandidate){.view_id=id,.layer=25,.overlaps_query=true,.conflicting=true,
        .center_present=true,.center={0,1.5f,z}};
}
static bool acquire(const PeSwordTargetCandidate*c,size_t n,PeSwordTargetResult*out){
    return pe_sword_target_acquire((PeVec3){0},(PeVec3){0,0,1},c,n,out);
}
int pe_test_sword_targeting(void){
    failures=0;PeSwordTargetQuery q;
    CHECK(pe_sword_target_query((PeVec3){10,2,-4},&q));
    CHECK(q.center.x==10&&q.center.y==3.5f&&q.center.z==-4&&q.radius==3&&q.layer_mask==(1u<<25));
    PeSwordTargetResult out={0};CHECK(acquire(NULL,0,&out));
    CHECK(!out.selected&&!out.aim_active&&out.candidate_index==SIZE_MAX&&out.part_index==SIZE_MAX);
    PeSwordTargetPart near=sphere(0,1.5f,1,.25f,false),far=sphere(0,1.5f,2,.25f,false);
    PeSwordTargetCandidate c[3]={candidate(1,&near,1),candidate(2,&far,1)};
    CHECK(acquire(c,2,&out)&&out.view_id==2&&out.candidate_index==1&&out.point.z==1.75f);
    c[0]=candidate(2,&far,1);c[1]=candidate(1,&near,1);
    CHECK(acquire(c,2,&out)&&out.view_id==1); /* Last eligible defence, not globally nearest. */
    c[0]=fallback(3,.5f);c[1]=candidate(1,&near,1);
    CHECK(acquire(c,2,&out)&&out.view_id==3&&out.part_index==SIZE_MAX);
    c[0]=candidate(1,&near,1);c[1]=fallback(3,2);
    CHECK(acquire(c,2,&out)&&out.view_id==3); /* Defence did not lower outer100. */
    c[0]=fallback(3,2);c[1]=candidate(1,&near,1);c[2]=candidate(2,&far,1);
    CHECK(acquire(c,3,&out)&&out.view_id==2); /* Both below fallback's2m cutoff. */
    c[0]=fallback(1,2);c[1]=fallback(2,2);
    CHECK(acquire(c,2,&out)&&out.view_id==1); /* Non-defence uses strict distance. */
    c[0]=fallback(1,100);CHECK(acquire(c,1,&out)&&!out.selected);
    PeSwordTargetPart parts[]={near,sphere(0,1.5f,2,.25f,true),sphere(0,1.5f,.5f,.1f,false)};
    c[0]=candidate(1,parts,3);CHECK(acquire(c,1,&out)&&out.part_index==1&&out.point.z==1.75f);
    parts[1]=sphere(0,1.5f,4,.25f,true);
    CHECK(acquire(c,1,&out)&&out.part_index==2); /* Out-of-range centre does not suppress other parts. */
    parts[0]=sphere(0,1.5f,0,.25f,false);
    CHECK(acquire(c,1,&out)&&out.selected&&!out.aim_active&&out.part_index==0);
    parts[0]=near;parts[1]=near;c[0]=candidate(1,parts,2);
    CHECK(acquire(c,1,&out)&&out.part_index==1); /* Equal-distance parts accept later part. */
    parts[1].enabled=false;CHECK(acquire(c,1,&out)&&out.part_index==0);
    PeSwordTargetPart edge=sphere(0,1.5f,3.5f,.5f,false);c[0]=candidate(1,&edge,1);
    CHECK(acquire(c,1,&out)&&out.selected&&out.point.z==3);
    edge.capsule.a.z=edge.capsule.b.z=3.501f;CHECK(acquire(c,1,&out)&&!out.selected);
    /* The authored115-degree horizontal cone permits slightly behind targets.
     * Use margins to avoid treating float trigonometry as an exact boundary. */
    for(int sign=-1;sign<=1;sign+=2)for(int outside=0;outside<2;outside++){
        double angle=sign*(outside?115.1:114.9)*3.14159265358979323846/180.;
        edge=sphere((float)(2*sin(angle)),1.5f,(float)(2*cos(angle)),.1f,false);
        CHECK(acquire(c,1,&out)&&out.selected==!outside);
    }
    edge=(PeSwordTargetPart){.capsule={{1,0,1},{1,3,1},.5f},.enabled=true};
    CHECK(acquire(c,1,&out)&&out.selected&&fabsf(out.point.x-.6464466f)<1e-6f&&out.point.y==1.5f&&fabsf(out.direction.z-.7071068f)<1e-6f);
    /* First view visitation is retained even when that collider has no usable
     * target, and independent colliders do not infer permission/overlap. */
    c[0]=candidate(1,&near,1);c[0].conflicting=false;c[1]=candidate(1,&far,1);
    CHECK(acquire(c,2,&out)&&!out.selected);
    c[0].conflicting=true;c[0].ragdoll=true;CHECK(acquire(c,2,&out)&&!out.selected);
    for(int reason=0;reason<5;reason++){
        c[0]=candidate(1,&near,1);
        if(reason==0)c[0].self=true;
        if(reason==1)c[0].overlaps_query=false;
        if(reason==2)c[0].layer=10;
        if(reason==3)c[0].view_id=0;
        if(reason==4)c[0].conflicting=false;
        CHECK(acquire(c,1,&out)&&!out.selected);
    }
    c[0]=fallback(1,2);c[0].center_present=false;CHECK(acquire(c,1,&out)&&!out.selected);
    c[0]=candidate(1,&near,1);CHECK(acquire(c,1,&out));PeSwordTargetResult before=out;
    far.capsule.radius=NAN;c[1]=candidate(2,&far,1);
    CHECK(!acquire(c,2,&out)&&!memcmp(&out,&before,sizeof out));
    CHECK(!acquire(NULL,1,&out)&&!memcmp(&out,&before,sizeof out));
    CHECK(!acquire(c,PE_SWORD_TARGET_CANDIDATES+1,&out)&&!memcmp(&out,&before,sizeof out));
    CHECK(!pe_sword_target_acquire((PeVec3){NAN,0,0},(PeVec3){0,0,1},c,1,&out));
    CHECK(!pe_sword_target_acquire((PeVec3){0},(PeVec3){0,1,0},c,1,&out));
    CHECK(!memcmp(&out,&before,sizeof out));
    CHECK(!acquire(c,1,NULL));
    return failures;
}
