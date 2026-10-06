/* Experimental tool helper only; intentionally outside runtime CORE. */
#include "../tools/human_rotation_math.h"
#include <fenv.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"human rotation:%d %s\n",__LINE__,#x);failures++;}}while(0)
static void rejected(float m[3],float lo[3],float hi[3],float sign[3]){
    float out[4]={11,12,13,14},before[4];memcpy(before,out,sizeof out);
    CHECK(!pe_human_rotation(m,lo,hi,sign,out));CHECK(!memcmp(out,before,sizeof out));
}
/* Independent pinned-native oracle values. The tolerance also admits the
 * separately measured portable sqrt fallback; no engine is needed to run these. */
static void golden(void){
    static const struct {float m[3],lo[3],hi[3],sign[3],q[4];} cases[]={
        {{-1.0f,0.0f,0.0f},{-0.4f,-1.2f,-2.4f},{0.8f,1.7f,2.8f},{1.0f,1.0f,1.0f},{-0.198746413f,0.0f,0.0f,0.980050981f}},
        {{0.0f,-1.0f,0.0f},{-0.4f,-1.2f,-2.4f},{0.8f,1.7f,2.8f},{1.0f,1.0f,1.0f},{0.0f,-0.564704597f,0.0f,0.825293064f}},
        {{0.0f,0.0f,-1.0f},{-0.4f,-1.2f,-2.4f},{0.8f,1.7f,2.8f},{1.0f,1.0f,1.0f},{0.0f,0.0f,-0.931995511f,0.362470001f}},
        {{0.0f,0.0f,0.0f},{-0.4f,-1.2f,-2.4f},{0.8f,1.7f,2.8f},{1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f,1.0f}},
        {{0.0f,0.0f,1.0f},{-0.4f,-1.2f,-2.4f},{0.8f,1.7f,2.8f},{1.0f,1.0f,1.0f},{0.0f,0.0f,0.985438764f,0.170031279f}},
        {{0.0f,1.0f,0.0f},{-0.4f,-1.2f,-2.4f},{0.8f,1.7f,2.8f},{1.0f,1.0f,1.0f},{0.0f,0.751262128f,0.0f,0.660003901f}},
        {{1.0f,0.0f,0.0f},{-0.4f,-1.2f,-2.4f},{0.8f,1.7f,2.8f},{1.0f,1.0f,1.0f},{0.389528871f,0.0f,0.0f,0.921014309f}},
        {{2.0f,-2.0f,1.5f},{-0.4f,-1.2f,-2.4f},{0.8f,1.7f,2.8f},{1.0f,1.0f,1.0f},{0.221027136f,-0.929798067f,0.201346695f,0.214667469f}},
    };
    for(unsigned i=0;i<sizeof cases/sizeof cases[0];i++){
        float q[4];CHECK(pe_human_rotation(cases[i].m,cases[i].lo,cases[i].hi,cases[i].sign,q));
        for(unsigned k=0;k<4;k++)CHECK(fabsf(q[k]-cases[i].q[k])<=5e-7f);
    }
}
int main(void){
    float m[3]={.2f,-.3f,1},lo[3]={-.4f,-1.2f,-2.4f},hi[3]={.8f,1.7f,2.8f},sign[3]={1,-1,1},out[4];
    int rounding=fegetround();CHECK(fesetround(FE_TONEAREST)==0);golden();
    CHECK(pe_human_rotation(m,lo,hi,sign,out));
    for(unsigned i=0;i<3;i++){
        float keep=m[i];m[i]=NAN;rejected(m,lo,hi,sign);m[i]=INFINITY;rejected(m,lo,hi,sign);
        m[i]=nextafterf(16,INFINITY);rejected(m,lo,hi,sign);m[i]=-m[i];rejected(m,lo,hi,sign);m[i]=keep;
        keep=lo[i];lo[i]=NAN;rejected(m,lo,hi,sign);lo[i]=-101;rejected(m,lo,hi,sign);lo[i]=hi[i]+1;rejected(m,lo,hi,sign);lo[i]=keep;
        keep=hi[i];hi[i]=INFINITY;rejected(m,lo,hi,sign);hi[i]=101;rejected(m,lo,hi,sign);hi[i]=keep;
        keep=sign[i];sign[i]=0;rejected(m,lo,hi,sign);sign[i]=NAN;rejected(m,lo,hi,sign);sign[i]=keep;
    }
    rejected(NULL,lo,hi,sign);rejected(m,NULL,hi,sign);rejected(m,lo,NULL,sign);rejected(m,lo,hi,NULL);
    CHECK(!pe_human_rotation(m,lo,hi,sign,NULL));
    int modes[]={FE_UPWARD,FE_DOWNWARD,FE_TOWARDZERO};
    for(unsigned i=0;i<sizeof modes/sizeof modes[0];i++){
        CHECK(fesetround(modes[i])==0);rejected(m,lo,hi,sign);
    }
    CHECK(fesetround(FE_TONEAREST)==0);
    for(unsigned i=0;i<3;i++){m[i]=i==1?-16:16;lo[i]=-100;hi[i]=100;}
    CHECK(pe_human_rotation(m,lo,hi,sign,out));
    for(unsigned i=0;i<4;i++)CHECK(isfinite(out[i]));
    /* Every input may alias the result; all values stage before output commit. */
    float reference[4],alias[4]={m[0],m[1],m[2],19};
    CHECK(pe_human_rotation(m,lo,hi,sign,reference));
    CHECK(pe_human_rotation(alias,lo,hi,sign,alias));CHECK(!memcmp(alias,reference,sizeof alias));
    CHECK(fesetround(rounding)==0);
    printf("human rotation helper: %d failures\n",failures);return failures?1:0;
}
