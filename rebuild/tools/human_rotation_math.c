#include "human_rotation_math.h"
#include <fenv.h>
#include <float.h>
#include <math.h>
#include <string.h>
#if defined(__SSE__) && !defined(PE_HUMAN_PORTABLE_SQRT)
#include <xmmintrin.h>
#define NATIVE_RSQRT 1
#else
#define NATIVE_RSQRT 0
#endif
static float add(float a,float b){volatile float x=a+b;return x;}
static float sub(float a,float b){volatile float x=a-b;return x;}
static float mul(float a,float b){volatile float x=a*b;return x;}
static float divide(float a,float b){volatile float x=a/b;return x;}
bool pe_human_rotation_native_rsqrt(void){return NATIVE_RSQRT;}
static float reciprocal_sqrt(float a){
#if NATIVE_RSQRT
    return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(a)));
#else
    return divide(1,sqrtf(a));
#endif
}
static void normalize(float q[4]){
    float square[4],pair[4],result[4];
    for(unsigned i=0;i<4;i++)square[i]=mul(q[i],q[i]);
    for(unsigned i=0;i<4;i++)pair[i]=add(square[i],square[(i+1)%4]);
    for(unsigned i=0;i<4;i++){
        float n=add(pair[i],pair[(i+2)%4]);
        float seed=fminf(mul(reciprocal_sqrt(n),0.9999998211860657f),FLT_MAX);
        float correction=sub(3.000000238418579f,mul(mul(n,seed),seed));
        result[i]=mul(mul(mul(seed,.5f),correction),q[i]);
    }
    memcpy(q,result,sizeof result);
}
static float trig_polynomial(float p){
    float square=mul(p,p),fourth=mul(square,square);
    float quadratic=add(mul(square,-41.28318405151367f),6.2831854820251465f);
    return mul(add(mul(fourth,76.03546142578125f),quadratic),p);
}
bool pe_human_rotation(const float m[3],const float lo[3],const float hi[3],
    const float sign[3],float out[4]){
    if(!m||!lo||!hi||!sign||!out||fegetround()!=FE_TONEAREST)return false;
    float tangent[3];
    for(unsigned i=0;i<3;i++){
        if(!isfinite(m[i])||fabsf(m[i])>16||!isfinite(lo[i])||!isfinite(hi[i])||
           fabsf(lo[i])>100||fabsf(hi[i])>100||lo[i]>hi[i]||
           (sign[i]!=1&&sign[i]!=-1))return false;
        /* Preserve the native zero/wrong-sign limit branches as well as the
         * ordinary negative-minimum/positive-maximum case. */
        float negative=lo[i]<0?mul(-m[i],lo[i]):lo[i]>0?m[i]:0;
        float positive=hi[i]>0?mul(m[i],hi[i]):hi[i]<0?m[i]:0;
        float angle=m[i]<0?negative:positive;
        float shifted=add(fabsf(angle),3.1415927410125732f);
        float turns=truncf(divide(shifted,6.2831854820251465f));
        float wrapped=sub(sub(shifted,mul(turns,6.2831854820251465f)),3.1415927410125732f);
        if(signbit(angle))wrapped=-wrapped;
        float half=fminf(1.5707954168319702f,fmaxf(-1.5707954168319702f,mul(.5f,wrapped)));
        float u=mul(half,.15915493667125702f);
        float a=sub(.25f,fabsf(sub(u,nearbyintf(u))));
        float v=add(u,-.25f);
        float b=sub(.25f,fabsf(sub(v,nearbyintf(v))));
        tangent[i]=divide(trig_polynomial(b),trig_polynomial(a));
        if(signbit(sign[i]))tangent[i]=-tangent[i];
    }
    float x=tangent[0],y=tangent[1],z=tangent[2];
    float q[4]={x,add(y,mul(x,z)),sub(z,mul(x,y)),1};
    normalize(q);normalize(q);
    for(unsigned i=0;i<4;i++)if(!isfinite(q[i]))return false;
    memcpy(out,q,sizeof q);return true;
}
