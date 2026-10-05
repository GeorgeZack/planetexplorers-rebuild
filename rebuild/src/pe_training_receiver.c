#include "pe_training_receiver.h"
#include <math.h>
#include <float.h>
#include <stdlib.h>
static bool finite3(PeVec3 v){return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z);}
bool pe_training_receiver_ray(unsigned i,PeTrainingReceiverRay*out){
    if(!out||i>=50)return false;
    PeTrainingReceiverRay r={.range=20};
    if(i<25){r.origin=(PeVec3){10.5f+.5f*(i/5),2.5f+.5f*(i%5),28.5f};r.direction.z=-1;}
    else{i-=25;r.origin=(PeVec3){17.5f+.5f*(i/5),20.5f,10.5f+.5f*(i%5)};r.direction.y=-1;}
    *out=r;return true;
}
static float box(PeVec3 o,PeVec3 d,PeVec3 lo,PeVec3 hi,float range){
    float origins[]={o.x,o.y,o.z},dirs[]={d.x,d.y,d.z},mins[]={lo.x,lo.y,lo.z},maxs[]={hi.x,hi.y,hi.z};
    float enter=0,exit=range;
    /* Unity raycasts do not detect a collider containing the ray origin. */
    if(o.x>lo.x&&o.x<hi.x&&o.y>lo.y&&o.y<hi.y&&o.z>lo.z&&o.z<hi.z)return FLT_MAX;
    for(unsigned i=0;i<3;i++){
        if(dirs[i]==0){if(origins[i]<mins[i]||origins[i]>maxs[i])return FLT_MAX;continue;}
        float a=(mins[i]-origins[i])/dirs[i],b=(maxs[i]-origins[i])/dirs[i];
        if(a>b){float t=a;a=b;b=t;}if(a>enter)enter=a;if(b<exit)exit=b;if(enter>exit)return FLT_MAX;
    }
    return enter;
}
bool pe_training_receiver_boxes(const PeTrainingReceiverRay*r,PeTrainingReceiverHit*out){
    if(!r||!out||!finite3(r->origin)||!finite3(r->direction)||!isfinite(r->range)||r->range<0||r->range>100000)return false;
    double len=sqrt((double)r->direction.x*r->direction.x+(double)r->direction.y*r->direction.y+(double)r->direction.z*r->direction.z);
    if(!isfinite(len)||len<1e-12)return false;
    PeVec3 d={(float)(r->direction.x/len),(float)(r->direction.y/len),(float)(r->direction.z/len)};
    const PeVec3 lo[]={{10,2,9.4f},{17,1.4f,10}},hi[]={{13,5,9.6f},{20,1.6f,13}};
    PeTrainingReceiverHit hit={0};float nearest=FLT_MAX;
    for(unsigned i=0;i<2;i++){float distance=box(r->origin,d,lo[i],hi[i],r->range);if(distance<nearest){nearest=distance;hit=(PeTrainingReceiverHit){true,i+1,distance};}}
    *out=hit;return true;
}
bool pe_training_receivers_evaluate(PeTrainingReceiverQuery query,void*context,PeTrainingReceiverResult*out){
    if(!query||!out)return false;
    PeTrainingReceiverResult result={0};
    for(unsigned i=0;i<50;i++){
        PeTrainingReceiverRay ray;PeTrainingReceiverHit hit={0};pe_training_receiver_ray(i,&ray);
        if(!query(context,&ray,&hit)||hit.receiver>2||!isfinite(hit.distance)||hit.distance<0||hit.distance>ray.range||
           (!hit.hit&&(hit.receiver||hit.distance!=0)))return false;
        if(hit.hit&&hit.receiver){result.receiver_mask|=UINT64_C(1)<<i;result.score-=100;}else result.score++;
    }
    result.complete=result.score>0;*out=result;return true;
}

bool pe_training_receiver_mesh(PeMeshData*out){
    if(!out||out->vertices||out->count)return false;
    PeVertex*vertices=calloc(72,sizeof*vertices);if(!vertices)return false;
    const PeVec3 lo[]={{10,2,9.4f},{17,1.4f,10}},hi[]={{13,5,9.6f},{20,1.6f,13}};
    const unsigned faces[6][4]={{0,4,6,2},{1,3,7,5},{0,1,5,4},{2,6,7,3},{0,2,3,1},{4,5,7,6}};
    const PeVec3 normals[]={{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}};
    const unsigned tri[]={0,1,2,0,2,3};size_t n=0;
    for(unsigned b=0;b<2;b++){
        PeVec3 corners[8];for(unsigned i=0;i<8;i++)corners[i]=(PeVec3){i&1?hi[b].x:lo[b].x,i&2?hi[b].y:lo[b].y,i&4?hi[b].z:lo[b].z};
        for(unsigned f=0;f<6;f++)for(unsigned j=0;j<6;j++)vertices[n++]=(PeVertex){corners[faces[f][tri[j]]],normals[f],0};
    }
    *out=(PeMeshData){vertices,n};return true;
}
