#include "pe_capsule_history.h"
#include "../generated/pe_capsule_history_source.h"
#include <math.h>

/* Installed Mono retains wider intermediates within scalar expressions, but
 * Vector3 components and scalar locals round to float. In particular Dot and
 * WhiteCat determinant expressions MUST NOT round after each product: this
 * changes the near-parallel fallback/contact point. The explicit double
 * expressions below match the isolated installed-managed oracle. Volatile
 * component operators prevent native FMA fusion across Vector3 boundaries. */
static float add(float a,float b){volatile float r=a+b;return r;}
static float sub(float a,float b){volatile float r=a-b;return r;}
static float mul(float a,float b){volatile float r=a*b;return r;}
static float divide(float a,float b){volatile float r=a/b;return r;}
static PeVec3 plus(PeVec3 a,PeVec3 b){return (PeVec3){add(a.x,b.x),add(a.y,b.y),add(a.z,b.z)};}
static PeVec3 minus(PeVec3 a,PeVec3 b){return (PeVec3){sub(a.x,b.x),sub(a.y,b.y),sub(a.z,b.z)};}
static PeVec3 scale(PeVec3 a,float f){return (PeVec3){mul(a.x,f),mul(a.y,f),mul(a.z,f)};}
static PeVec3 quotient(PeVec3 a,float f){return (PeVec3){divide(a.x,f),divide(a.y,f),divide(a.z,f)};}
static float dot(PeVec3 a,PeVec3 b){return (float)((double)a.x*b.x+(double)a.y*b.y+(double)a.z*b.z);}
static float square(PeVec3 a){return dot(a,a);}
static float magnitude(PeVec3 a){return (float)sqrt((double)square(a));}
static bool point_finite(PeVec3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z);}
static bool bounded(PeVec3 p){return point_finite(p)&&fabsf(p.x)<=1000000&&fabsf(p.y)<=1000000&&fabsf(p.z)<=1000000;}
static bool equal(PeVec3 a,PeVec3 b){return square(minus(a,b))<PE_CAPSULE_EQUALITY_SQUARED;}
static PeVec3 normalized(PeVec3 p){float m=magnitude(p);return m>PE_CAPSULE_NORMALIZE_MAGNITUDE?quotient(p,m):(PeVec3){0};}
static float clamp01(float t){if(t<0)return 0;if(t>1)return 1;return t;}
static float lerpf(float a,float b,float t){return (float)((double)a+((double)b-a)*clamp01(t));}
static PeVec3 lerp(PeVec3 a,PeVec3 b,float t){t=clamp01(t);return (PeVec3){lerpf(a.x,b.x,t),lerpf(a.y,b.y,t),lerpf(a.z,b.z,t)};}
static PeVec3 closest(PeVec3 a,PeVec3 b,PeVec3 p){
    PeVec3 d=minus(b,a);float t=square(d);
    if(t<PE_CAPSULE_POINT_SEGMENT_SQUARED)return a;
    t=clamp01(divide(dot(minus(p,a),d),t));return plus(a,scale(d,t));
}
/* Utility.cs135: intentional NaN parallel fallback and strict tie ordering;
 * the existing double-precision pe_hitbox_contact solver is not substituted. */
static void closest_pair(PeVec3 sa,PeVec3 ea,PeVec3 sb,PeVec3 eb,PeVec3 *pa,PeVec3 *pb){
    PeVec3 da=minus(ea,sa),db=minus(eb,sb),offset=minus(sa,sb);
    float k0=dot(da,db),k1=square(da),k2=dot(offset,da),k3=square(db),k4=dot(offset,db);
    float t=(float)((double)k3*k1-(double)k0*k0);
    float a=(float)(((double)k0*k4-(double)k3*k2)/t),b=(float)(((double)k1*k4-(double)k0*k2)/t);
    if(isnan(a)||isnan(b)){
        *pb=closest(sb,eb,sa);*pa=closest(sb,eb,ea);
        if(square(minus(*pb,sa))<square(minus(*pa,ea)))*pa=sa;
        else{*pb=*pa;*pa=ea;}return;
    }
    if(a<0){
        if(b<0){*pa=closest(sa,ea,sb);*pb=closest(sb,eb,sa);
            if(square(minus(*pa,sb))<square(minus(*pb,sa)))*pb=sb;else *pa=sa;
        }else if(b>1){*pa=closest(sa,ea,eb);*pb=closest(sb,eb,sa);
            if(square(minus(*pa,eb))<square(minus(*pb,sa)))*pb=eb;else *pa=sa;
        }else{*pa=sa;*pb=closest(sb,eb,sa);}
    }else if(a>1){
        if(b<0){*pa=closest(sa,ea,sb);*pb=closest(sb,eb,ea);
            if(square(minus(*pa,sb))<square(minus(*pb,ea)))*pb=sb;else *pa=ea;
        }else if(b>1){*pa=closest(sa,ea,eb);*pb=closest(sb,eb,ea);
            if(square(minus(*pa,eb))<square(minus(*pb,ea)))*pb=eb;else *pa=ea;
        }else{*pa=ea;*pb=closest(sb,eb,ea);}
    }else{
        if(b<0){*pb=sb;*pa=closest(sa,ea,sb);}
        else if(b>1){*pb=eb;*pa=closest(sa,ea,eb);}
        else{*pa=plus(sa,scale(da,a));*pb=plus(sb,scale(db,b));}
    }
}
static bool capsule_valid(PeHitboxCapsule c){return bounded(c.a)&&bounded(c.b)&&isfinite(c.radius)&&c.radius>0&&c.radius<=1000000;}
static bool count_for(PeVec3 motion,float radius,unsigned *out){
    float steps=floorf(divide(mul(PE_CAPSULE_SAMPLE_DISTANCE_FACTOR,magnitude(motion)),radius));
    if(!isfinite(steps)||steps<0||steps>=(float)PE_CAPSULE_HISTORY_MAX_SAMPLES)return false;
    *out=(unsigned)steps+1;return true;
}
PeCapsuleHistoryStatus pe_capsule_history_contact(const PeCapsuleHistory *h,PeHitboxCapsule target,PeCapsuleHistoryContact *out){
    if(!h||!out||!bounded(h->previous_a)||!bounded(h->previous_b)||!bounded(h->parent_center)||
       !capsule_valid(h->current)||!capsule_valid(target))return PE_CAPSULE_HISTORY_INVALID;
    PeVec3 move1=minus(h->current.a,h->previous_a),move2=minus(h->current.b,h->previous_b);
    unsigned count1,count2;if(!count_for(move1,h->current.radius,&count1)||!count_for(move2,h->current.radius,&count2))return PE_CAPSULE_HISTORY_INVALID;
    unsigned count=count1>count2?count1:count2;
    float r11=magnitude(minus(h->previous_a,h->parent_center)),r12=magnitude(minus(h->current.a,h->parent_center));
    float r21=magnitude(minus(h->previous_b,h->parent_center)),r22=magnitude(minus(h->current.b,h->parent_center));
    float pcount=(float)(count>1?count-1:count),radius=add(h->current.radius,target.radius);
    bool radial=!equal(h->parent_center,(PeVec3){0});
    for(unsigned i=0;i<count;i++){
        float p=divide((float)i,pcount);PeVec3 a=lerp(h->previous_a,h->current.a,p),b=lerp(h->previous_b,h->current.b,p);
        if(radial){
            if(!equal(a,h->parent_center))a=plus(h->parent_center,scale(normalized(minus(a,h->parent_center)),lerpf(r11,r12,p)));
            if(!equal(b,h->parent_center))b=plus(h->parent_center,scale(normalized(minus(b,h->parent_center)),lerpf(r21,r22,p)));
        }
        PeVec3 pa,pb;closest_pair(a,b,target.a,target.b,&pa,&pb);
        if(!point_finite(a)||!point_finite(b)||!point_finite(pa)||!point_finite(pb))return PE_CAPSULE_HISTORY_INVALID;
        PeVec3 dir=minus(pb,pa);float distance2=square(dir);
        if(!isfinite(distance2))return PE_CAPSULE_HISTORY_INVALID;
        if((double)distance2<(double)radius*radius){
            PeCapsuleHistoryContact result={0};
            result.contact.self_axis=pa;result.contact.target_axis=pb;
            result.contact.point=plus(pa,quotient(scale(dir,h->current.radius),radius));
            result.contact.normal=normalized(dir);result.contact.axis_distance=(float)sqrt((double)distance2);
            result.hit_direction=normalized(plus(plus(move1,move2),dir));
            result.sampled=(PeHitboxCapsule){a,b,h->current.radius};result.sample_index=i;result.sample_count=count;
            if(!point_finite(result.contact.point)||!point_finite(result.hit_direction))return PE_CAPSULE_HISTORY_INVALID;
            *out=result;return PE_CAPSULE_HISTORY_HIT;
        }
    }
    return PE_CAPSULE_HISTORY_MISS;
}
