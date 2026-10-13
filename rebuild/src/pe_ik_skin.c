#include "pe_ik_skin.h"
#include <math.h>
#include <string.h>

static void matrix(PeFbbikQuat q,float m[9]){
    float x=q.x*2,y=q.y*2,z=q.z*2,xx=q.x*x,yy=q.y*y,zz=q.z*z,xy=q.x*y,xz=q.x*z,yz=q.y*z,wx=q.w*x,wy=q.w*y,wz=q.w*z;
    m[0]=1-(yy+zz);m[1]=xy-wz;m[2]=xz+wy;m[3]=xy+wz;m[4]=1-(xx+zz);m[5]=yz-wx;m[6]=xz-wy;m[7]=yz+wx;m[8]=1-(xx+yy);
}
bool pe_ik_skin_deltas(const PeFbbikRig *rig,const PeFbbikTransform *animated,const PeFbbikTransform *solved,float delta[PE_FBBIK_MAX_BONES][12]){
    if(!rig||!animated||!solved||!delta||!rig->bone_count||rig->bone_count>PE_FBBIK_MAX_BONES)return false;
    float result[PE_FBBIK_MAX_BONES][12];
    for(unsigned b=0;b<rig->bone_count;b++){
        PeFbbikTransform a,s;float ra[9],rs[9];
        if(!pe_fbbik_world(rig,animated,b,&a)||!pe_fbbik_world(rig,solved,b,&s))return false;
        matrix(a.rotation,ra);matrix(s.rotation,rs);
        float pa[3]={a.position.x,a.position.y,a.position.z},ps[3]={s.position.x,s.position.y,s.position.z};
        for(int i=0;i<3;i++){
            float moved=0;
            /* R = Rs * transpose(Ra); t = ps - R * pa */
            for(int j=0;j<3;j++){float v=rs[i*3]*ra[j*3]+rs[i*3+1]*ra[j*3+1]+rs[i*3+2]*ra[j*3+2];result[b][i*4+j]=v;moved+=v*pa[j];}
            result[b][i*4+3]=ps[i]-moved;
        }
        for(int k=0;k<12;k++)if(!isfinite(result[b][k]))return false;
    }
    memcpy(delta,result,sizeof(float)*12*rig->bone_count);return true;
}
void pe_ik_skin_point(const float d[12],const float p[3],float out[3]){
    float x=p[0],y=p[1],z=p[2];
    for(int i=0;i<3;i++)out[i]=d[i*4]*x+d[i*4+1]*y+d[i*4+2]*z+d[i*4+3];
}
