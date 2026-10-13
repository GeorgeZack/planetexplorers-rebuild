#include "pe_fbbik.h"
#include "../generated/pe_fbbik_source.h"
#include <math.h>
#include <string.h>

typedef PeVec3 V;
typedef PeFbbikQuat Q;
typedef PeFbbikTransform T;
typedef PeFbbikRig R;
static const V ZERO={0,0,0};
static const Q IDENTITY={0,0,0,1};

/* Engine value-type adapters. Thresholds follow the Unity 5 managed bodies
 * (Vector3 equality, normalized, Project, Quaternion equality); the natives
 * are conventional and unqualified. */
static V add(V a,V b){return (V){a.x+b.x,a.y+b.y,a.z+b.z};}
static V sub(V a,V b){return (V){a.x-b.x,a.y-b.y,a.z-b.z};}
static V mul(V a,float s){return (V){a.x*s,a.y*s,a.z*s};}
static float dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static V cross(V a,V b){return (V){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static float mag(V a){return sqrtf(dot(a,a));}
static V unit(V a){float m=mag(a);return m>1e-5f?mul(a,1/m):ZERO;}
static bool same(V a,V b){V d=sub(a,b);return dot(d,d)<9.99999944e-11f;}
static float clampf(float v,float lo,float hi){return v<lo?lo:v>hi?hi:v;}
static V lerp(V a,V b,float t){t=clampf(t,0,1);return add(a,mul(sub(b,a),t));}
static float distance(V a,V b){return mag(sub(a,b));}
static Q qmul(Q a,Q b){
    return (Q){a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y+a.y*b.w+a.z*b.x-a.x*b.z,
               a.w*b.z+a.z*b.w+a.x*b.y-a.y*b.x,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
}
static Q qinv(Q a){return (Q){-a.x,-a.y,-a.z,a.w};}
static V rotate(Q q,V p){
    float x=q.x*2,y=q.y*2,z=q.z*2,xx=q.x*x,yy=q.y*y,zz=q.z*z,xy=q.x*y,xz=q.x*z,yz=q.y*z,wx=q.w*x,wy=q.w*y,wz=q.w*z;
    return (V){(1-(yy+zz))*p.x+(xy-wz)*p.y+(xz+wy)*p.z,(xy+wz)*p.x+(1-(xx+zz))*p.y+(yz-wx)*p.z,
               (xz-wy)*p.x+(yz+wx)*p.y+(1-(xx+yy))*p.z};
}
static Q qunit(Q q){
    float m=sqrtf(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    return m>1e-5f?(Q){q.x/m,q.y/m,q.z/m,q.w/m}:IDENTITY;
}
static bool qsame(Q a,Q b){return a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w>0.999999f;}
static Q qlerp(Q a,Q b,float t){
    t=clampf(t,0,1);
    float s=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w<0?-1.0f:1.0f;
    return qunit((Q){a.x+t*(s*b.x-a.x),a.y+t*(s*b.y-a.y),a.z+t*(s*b.z-a.z),a.w+t*(s*b.w-a.w)});
}
static V perpendicular(V a){
    V c=cross((V){1,0,0},a);
    if(dot(c,c)<1e-6f)c=cross((V){0,1,0},a);
    return unit(c);
}
static Q axis_angle(V axis,float radians){float s=sinf(radians*.5f);return (Q){axis.x*s,axis.y*s,axis.z*s,cosf(radians*.5f)};}
static Q from_to(V from,V to){
    V a=unit(from),b=unit(to);
    if(same(a,ZERO)||same(b,ZERO))return IDENTITY;
    float d=dot(a,b);
    if(d>=1-1e-6f)return IDENTITY;
    if(d<=-1+1e-6f){V p=perpendicular(a);return (Q){p.x,p.y,p.z,0};}
    V c=cross(a,b);
    return qunit((Q){c.x,c.y,c.z,1+d});
}
static Q look(V forward,V up){
    V z=unit(forward);
    if(same(z,ZERO))return IDENTITY;
    V x=cross(up,z);
    if(mag(x)<1e-5f)return from_to((V){0,0,1},z);
    x=unit(x);
    V y=cross(z,x);
    float trace=x.x+y.y+z.z;Q q;
    if(trace>0){float s=sqrtf(trace+1)*2;q=(Q){(y.z-z.y)/s,(z.x-x.z)/s,(x.y-y.x)/s,.25f*s};}
    else if(x.x>y.y&&x.x>z.z){float s=sqrtf(1+x.x-y.y-z.z)*2;q=(Q){.25f*s,(y.x+x.y)/s,(z.x+x.z)/s,(y.z-z.y)/s};}
    else if(y.y>z.z){float s=sqrtf(1+y.y-x.x-z.z)*2;q=(Q){(y.x+x.y)/s,.25f*s,(z.y+y.z)/s,(z.x-x.z)/s};}
    else{float s=sqrtf(1+z.z-x.x-y.y)*2;q=(Q){(z.x+x.z)/s,(z.y+y.z)/s,.25f*s,(x.y-y.x)/s};}
    return qunit(q);
}
static float angle(V a,V b){return acosf(clampf(dot(unit(a),unit(b)),-1,1))*57.29578f;}
static void ortho(V *normal,V *tangent){
    V n=unit(*normal);
    if(same(n,ZERO))n=(V){1,0,0};
    V t=sub(*tangent,mul(n,dot(n,*tangent)));
    t=mag(t)<1e-5f?perpendicular(n):unit(t);
    *normal=n;*tangent=t;
}
static V project(V a,V normal){
    float n=dot(normal,normal);
    return n<1.401298e-45f?ZERO:mul(normal,dot(a,normal)/n);
}
static V slerp(V a,V b,float t){
    t=clampf(t,0,1);
    float ma=mag(a),mb=mag(b);
    if(ma<1e-5f||mb<1e-5f)return lerp(a,b,t);
    float d=dot(a,b)/(ma*mb),length=ma+(mb-ma)*t;
    if(d>1-1e-6f)return lerp(a,b,t);
    V from=mul(a,1/ma);
    if(d<-1+1e-6f)return mul(rotate(axis_angle(perpendicular(from),3.14159274f*t),from),length);
    return mul(rotate(axis_angle(unit(cross(a,b)),acosf(d)*t),from),length);
}
static V joint(V a,V b,float length){return add(b,mul(unit(sub(a,b)),length));}

static T world(const R *r,const T *l,unsigned b){
    T t=l[b];
    for(int p=r->parent[b];p>=0;p=r->parent[p]){
        t.position=add(l[p].position,rotate(l[p].rotation,t.position));
        t.rotation=qmul(l[p].rotation,t.rotation);
    }
    return t;
}
static void set_rotation(const R *r,T *l,unsigned b,Q q){
    int p=r->parent[b];
    /* Renormalize: a conjugate stands in for the inverse, and without this the
     * float norm error doubles at every written level of the hierarchy. */
    l[b].rotation=qunit(p<0?q:qmul(qinv(world(r,l,(unsigned)p).rotation),q));
}
static void set_position(const R *r,T *l,unsigned b,V v){
    int p=r->parent[b];
    if(p<0){l[b].position=v;return;}
    T w=world(r,l,(unsigned)p);
    l[b].position=rotate(qinv(w.rotation),sub(v,w.position));
}
static void turn(const R *r,T *l,unsigned b,Q target,float weight){
    set_rotation(r,l,b,weight<1?qlerp(world(r,l,b).rotation,target,weight):target);
}

static unsigned nodes_of(unsigned chain){return chain?3u:1u;}
static unsigned bone_of(const R *r,unsigned node){return r->chain_bone[node/3][node%3];}
static bool finite_v(V a){return isfinite(a.x)&&isfinite(a.y)&&isfinite(a.z);}
static bool finite_q(Q a){return isfinite(a.x)&&isfinite(a.y)&&isfinite(a.z)&&isfinite(a.w);}
static bool finite_pose(const R *r,const T *l){
    for(unsigned i=0;i<r->bone_count;i++)if(!finite_v(l[i].position)||!finite_q(l[i].rotation))return false;
    return true;
}
static bool valid_rig(const R *r){
    if(!r||!r->bone_count||r->bone_count>PE_FBBIK_MAX_BONES||r->root>=r->bone_count)return false;
    for(unsigned i=0;i<r->bone_count;i++)if(r->parent[i]>=(int)i||r->parent[i]<-1)return false;
    for(unsigned c=0;c<PE_FBBIK_CHAINS;c++)for(unsigned n=0;n<nodes_of(c);n++)if(r->chain_bone[c][n]>=r->bone_count)return false;
    for(unsigned i=0;i<PE_FBBIK_CONSTRAINTS;i++)
        if(!r->constraint[i].chain1||!r->constraint[i].chain2||r->constraint[i].chain1>=PE_FBBIK_CHAINS||r->constraint[i].chain2>=PE_FBBIK_CHAINS)return false;
    for(unsigned e=0;e<PE_FBBIK_EFFECTORS;e++){
        unsigned n=r->effector_node[e];
        if(n>=PE_FBBIK_NODES||n%3>=nodes_of(n/3))return false;
        for(unsigned i=0;i<3;i++){
            unsigned p=r->effector_plane[e][i];
            if(p!=PE_FBBIK_NONE&&(p>=PE_FBBIK_NODES||p%3>=nodes_of(p/3)))return false;
        }
    }
    for(unsigned i=0;i<2;i++)if(r->body_child[i]>=PE_FBBIK_NODES||r->body_child[i]%3>=nodes_of(r->body_child[i]/3))return false;
    if(r->spine_count<2||r->spine_count>PE_FBBIK_MAX_SPINE||r->spine_root_index>=r->spine_count)return false;
    for(unsigned i=0;i<r->spine_count;i++)if(r->spine[i]>=r->bone_count)return false;
    /* IKMappingSpine finds its root node through GetPoint on the body chain. */
    if(r->spine[r->spine_root_index]!=r->chain_bone[0][0]||r->head>=r->bone_count)return false;
    for(unsigned i=0;i<PE_FBBIK_LIMBS;i++)if(r->limb_parent[i]!=PE_FBBIK_NONE&&r->limb_parent[i]>=r->bone_count)return false;
    return true;
}

const PeFbbikRig *pe_fbbik_female_rig(void){return &pe_fbbik_source_rig;}
unsigned pe_fbbik_female_bone_id(unsigned bone){return bone<pe_fbbik_source_rig.bone_count?pe_fbbik_source_bone_ids[bone]:0;}
bool pe_fbbik_female_rest(PeFbbikTransform local[PE_FBBIK_MAX_BONES]){
    if(!local)return false;
    memset(local,0,sizeof(T)*PE_FBBIK_MAX_BONES);
    for(unsigned i=0;i<PE_FBBIK_MAX_BONES;i++)local[i].rotation=IDENTITY;
    memcpy(local,pe_fbbik_source_rest,sizeof pe_fbbik_source_rest);
    return true;
}
bool pe_fbbik_world(const PeFbbikRig *rig,const PeFbbikTransform *local,unsigned bone,PeFbbikTransform *out){
    if(!valid_rig(rig)||!local||!out||bone>=rig->bone_count||!finite_pose(rig,local))return false;
    *out=world(rig,local,bone);
    return true;
}

bool pe_fbbik_init(PeFbbik *state,const PeFbbikRig *rig,const PeFbbikTransform *local){
    if(!state||!valid_rig(rig)||!local||!finite_pose(rig,local))return false;
    PeFbbik s={.rig=rig,.initiated=true};
    for(unsigned i=0;i<PE_FBBIK_LIMBS;i++){
        /* IKConstraintBend.Initiate */
        T a=world(rig,local,rig->chain_bone[i+1][0]),b=world(rig,local,rig->chain_bone[i+1][1]),c=world(rig,local,rig->chain_bone[i+1][2]);
        V upper=sub(b.position,a.position),limb=sub(c.position,a.position),direction=upper,n=limb;
        ortho(&n,&direction);n=upper;ortho(&n,&direction);
        s.bend_direction[i]=direction;
        if(!(angle(upper,limb)<1)){s.bend_default_local[i]=rotate(qinv(a.rotation),direction);s.bend_local_set[i]=true;}
        s.bend_default_child[i]=rotate(qinv(c.rotation),cross(unit(limb),direction));
        if(rig->limb_parent[i]!=PE_FBBIK_NONE){
            T p=world(rig,local,rig->limb_parent[i]);
            s.parent_swing_axis[i]=rotate(qinv(p.rotation),sub(a.position,p.position));
        }
    }
    for(unsigned e=0;e<PE_FBBIK_EFFECTORS;e++){
        T t=world(rig,local,bone_of(rig,rig->effector_node[e]));
        s.effector_position[e]=t.position;s.effector_rotation[e]=t.rotation;
    }
    *state=s;
    return true;
}

bool pe_fbbik_input(const PeFbbik *state,const PeFbbikTransform *local,PeFbbikInput *out){
    if(!state||!state->initiated||!local||!out)return false;
    PeFbbikInput in={.weight=1};
    for(unsigned i=0;i<PE_FBBIK_MAX_BONES;i++)in.local[i].rotation=IDENTITY;
    memcpy(in.local,local,sizeof(T)*state->rig->bone_count);
    for(unsigned e=0;e<PE_FBBIK_EFFECTORS;e++){in.effector[e].position=state->effector_position[e];in.effector[e].rotation=state->effector_rotation[e];}
    *out=in;
    return true;
}

typedef struct {
    const PeFbbik *s; const R *r; const PeFbbikInput *in;
    T l[PE_FBBIK_MAX_BONES];
    float w; bool full;
    V sp[PE_FBBIK_NODES],off[PE_FBBIK_NODES],tp[PE_FBBIK_NODES];
    Q srot[PE_FBBIK_NODES];
    float len[PE_FBBIK_NODES],epw[PE_FBBIK_NODES],erw[PE_FBBIK_NODES];
    float length[PE_FBBIK_CHAINS],root_length[PE_FBBIK_CHAINS],sqr1[PE_FBBIK_CHAINS],sqr_dif[PE_FBBIK_CHAINS];
    float reach_force[PE_FBBIK_CHAINS],span[PE_FBBIK_CHAINS],cross_fade[PE_FBBIK_CHAINS],pull_parent_sum;
    float nominal[PE_FBBIK_CONSTRAINTS],fade[PE_FBBIK_CONSTRAINTS],push_elasticity[PE_FBBIK_CONSTRAINTS];
    bool rigid[PE_FBBIK_CONSTRAINTS];
    V body_offset,animated[PE_FBBIK_EFFECTORS],child_local[2];
    float pos_w[PE_FBBIK_EFFECTORS],maintain[PE_FBBIK_EFFECTORS];
    bool first[PE_FBBIK_EFFECTORS];
    Q plane_offset[PE_FBBIK_EFFECTORS],animated_plane[PE_FBBIK_EFFECTORS];
    Q limb_target1[PE_FBBIK_LIMBS],limb_target2[PE_FBBIK_LIMBS],limb_maintain[PE_FBBIK_LIMBS];
    V limb_plane1[PE_FBBIK_LIMBS];
    Q spine_target[2],head_maintain; V spine_plane[2];
    float spine_length[PE_FBBIK_MAX_SPINE];
    V spine_swing[PE_FBBIK_MAX_SPINE],spine_twist[PE_FBBIK_MAX_SPINE],ik[PE_FBBIK_MAX_SPINE];
    V offset;
} C;

static V pos(const C *c,unsigned bone){return world(c->r,c->l,bone).position;}
static Q rot(const C *c,unsigned bone){return world(c->r,c->l,bone).rotation;}
static Q plane_look(V a,V b,V d){return same(a,d)?IDENTITY:look(sub(b,a),sub(d,a));}

static V hand_body_pull(const C *c,unsigned effector,unsigned chain,V offset){
    V a=pos(c,c->r->chain_bone[chain][0]);
    V d=sub(c->in->effector[effector].position,add(a,offset));
    /* The original reads the lengths left by the previous update; bones are rigid. */
    float reach=distance(a,pos(c,c->r->chain_bone[chain][1]))+distance(pos(c,c->r->chain_bone[chain][1]),pos(c,c->r->chain_bone[chain][2]));
    float m=mag(d);
    return m<reach?ZERO:mul(mul(d,1/m),m-reach);
}
static void pull_body(C *c){
    const R *r=c->r;
    if(r->iterations<1||(r->pull_body_vertical==0&&r->pull_body_horizontal==0))return;
    V o=mul(hand_body_pull(c,5,1,ZERO),clampf(c->in->effector[5].position_weight,0,1));
    o=add(o,mul(hand_body_pull(c,6,2,o),clampf(c->in->effector[6].position_weight,0,1)));
    V up=rotate(rot(c,r->root),(V){0,1,0}),vertical=ZERO,horizontal=ZERO;
    if(r->pull_body_vertical!=0)vertical=mul(project(o,up),r->pull_body_vertical);
    if(r->pull_body_horizontal!=0){V n=up,t=o;ortho(&n,&t);horizontal=mul(project(o,t),r->pull_body_horizontal);}
    c->body_offset=add(vertical,horizontal);
}
static V clamp_direction(V direction,V normal,float weight,bool *changed){
    *changed=false;
    if(!(weight>0))return direction;
    if(!(weight<1)){*changed=true;return normal;}
    float d=1-angle(normal,direction)/180;
    if(d>weight)return direction;
    *changed=true;
    return slerp(normal,direction,clampf(d/weight,0,1)*clampf(1-(weight-d)/(1-d),0,1));
}
static void limit_bend(C *c,unsigned limb,float effector_weight){
    const R *r=c->r;unsigned b1=r->chain_bone[limb+1][0],b2=r->chain_bone[limb+1][1],b3=r->chain_bone[limb+1][2];
    V normal=rotate(rot(c,b1),mul(c->s->bend_default_local[limb],-1)),lower=sub(pos(c,b3),pos(c,b2));
    bool changed;V clamped=clamp_direction(lower,normal,0.5049999952316284f*c->w,&changed);
    Q hand=rot(c,b3);
    if(changed)set_rotation(r,c->l,b2,qmul(from_to(lower,clamped),rot(c,b2)));
    if(effector_weight>0){
        V upper=sub(pos(c,b2),pos(c,b1)),fore=sub(pos(c,b3),pos(c,b2));
        ortho(&upper,&fore);
        set_rotation(r,c->l,b2,qlerp(rot(c,b2),qmul(from_to(fore,normal),rot(c,b2)),effector_weight*c->w));
    }
    if(changed||effector_weight>0)set_rotation(r,c->l,b3,hand);
}
static void effector_pre_solve(C *c,unsigned e){
    const R *r=c->r;const PeFbbikEffector *in=&c->in->effector[e];unsigned n=r->effector_node[e];
    V offset=e?in->position_offset:add(in->position_offset,c->body_offset);
    c->pos_w[e]=clampf(in->position_weight,0,1)*c->w;
    c->maintain[e]=clampf(in->maintain_relative_position_weight,0,1);
    c->epw[n]=c->pos_w[e];c->erw[n]=clampf(in->rotation_weight,0,1)*c->w;c->srot[n]=in->rotation;
    c->off[n]=add(c->off[n],mul(offset,c->w));
    if(!e&&r->effect_child_nodes[e]&&c->full)for(unsigned i=0;i<2;i++){
        unsigned child=r->body_child[i];
        c->child_local[i]=sub(pos(c,bone_of(r,child)),pos(c,bone_of(r,n)));
        c->off[child]=add(c->off[child],mul(offset,c->w));
    }
    const unsigned char *p=r->effector_plane[e];
    c->animated_plane[e]=IDENTITY;c->plane_offset[e]=IDENTITY;
    if(p[0]!=PE_FBBIK_NONE&&p[1]!=PE_FBBIK_NONE&&p[2]!=PE_FBBIK_NONE&&c->maintain[e]>0)
        c->animated_plane[e]=look(sub(pos(c,bone_of(r,p[1])),pos(c,bone_of(r,p[0]))),sub(pos(c,bone_of(r,p[2])),pos(c,bone_of(r,p[0]))));
    c->first[e]=true;
}
static void effector_update(C *c,unsigned e){
    const R *r=c->r;unsigned n=r->effector_node[e];const unsigned char *p=r->effector_plane[e];
    if(c->first[e]){c->animated[e]=add(c->tp[n],c->off[n]);c->first[e]=false;}
    V target;c->plane_offset[e]=IDENTITY;
    if(p[0]==PE_FBBIK_NONE)target=c->sp[n];
    else if(!(c->maintain[e]>0))target=c->animated[e];
    else{
        /* A serialized effector with planeBone1 alone is an end effector without plane nodes. */
        V a=sub(c->sp[p[1]],c->sp[p[0]]),b=sub(c->sp[p[2]],c->sp[p[0]]);
        Q plane=same(a,ZERO)?IDENTITY:look(a,b),turned=qmul(plane,qinv(c->animated_plane[e]));
        V moved=add(c->sp[p[0]],rotate(turned,sub(c->tp[n],c->tp[p[0]])));
        c->plane_offset[e]=qlerp(IDENTITY,turned,c->maintain[e]);
        target=lerp(c->animated[e],add(moved,c->off[n]),c->maintain[e]);
    }
    c->sp[n]=lerp(target,c->in->effector[e].position,c->pos_w[e]);
    if(!e&&r->effect_child_nodes[e])for(unsigned i=0;i<2;i++)
        c->sp[r->body_child[i]]=lerp(c->sp[r->body_child[i]],add(c->sp[n],c->child_local[i]),c->pos_w[e]);
}
static bool end_effector(const C *c,unsigned e){return c->r->effector_plane[e][0]!=PE_FBBIK_NONE;}
static void update_effectors(C *c,bool end){
    for(unsigned e=0;e<PE_FBBIK_EFFECTORS;e++)if(end_effector(c,e)==end)effector_update(c,e);
}
static void constraint_pre_solve(C *c,unsigned i){
    const R *r=c->r;unsigned a=r->constraint[i].chain1,b=r->constraint[i].chain2;
    c->nominal[i]=distance(c->tp[a*3],c->tp[b*3]);
    c->rigid[i]=!(c->push_elasticity[i]>0)&&!(r->constraint[i].pull_elasticity>0);
    c->fade[i]=c->rigid[i]?1-(.5f+(r->pull[a]-r->pull[b])*.5f):.5f;
}
static void chain_read(C *c,unsigned chain){
    const R *r=c->r;unsigned base=chain*3,count=nodes_of(chain);
    for(unsigned n=0;n<count;n++){c->tp[base+n]=pos(c,r->chain_bone[chain][n]);c->sp[base+n]=add(c->tp[base+n],c->off[base+n]);}
    c->length[chain]=0;
    for(unsigned n=0;n+1<count;n++){c->len[base+n]=distance(c->tp[base+n],c->tp[base+n+1]);c->length[chain]+=c->len[base+n];}
    if(count==3){c->sqr1[chain]=c->len[base]*c->len[base];c->sqr_dif[chain]=c->sqr1[chain]-c->len[base+1]*c->len[base+1];}
    c->reach_force[chain]=count==3?r->reach[chain]*clampf(c->epw[base+2],0,1):0;
    if(c->full&&r->push[chain]>0&&count>1)c->span[chain]=distance(c->tp[base],c->tp[base+count-1]);
}
static void body_chain_read(C *c){
    const R *r=c->r;
    for(unsigned k=1;k<PE_FBBIK_CHAINS;k++)c->root_length[k]=distance(c->tp[k*3],c->tp[0]);
    if(!c->full)return;
    for(unsigned i=0;i<PE_FBBIK_CONSTRAINTS;i++)constraint_pre_solve(c,i);
    float sum=c->epw[0];c->pull_parent_sum=0;
    for(unsigned k=1;k<PE_FBBIK_CHAINS;k++){sum+=c->epw[k*3]*r->pull[k];c->pull_parent_sum+=r->pull[k];}
    sum=sum<1?1:sum;
    for(unsigned k=1;k<PE_FBBIK_CHAINS;k++)c->cross_fade[k]=c->epw[k*3]*r->pull[k]/sum;
    if(c->pull_parent_sum<1)c->pull_parent_sum=1;
}
static V bend_direction(C *c,unsigned limb){
    const PeFbbik *s=c->s;unsigned base=(limb+1)*3;float weight=c->in->bend_weight[limb];
    if(!(weight<1))return unit(s->bend_direction[limb]);
    V span=sub(c->sp[base+2],c->sp[base]);
    V dir=rotate(from_to(sub(c->tp[base+2],c->tp[base]),span),sub(c->tp[base+1],c->tp[base]));
    if(c->erw[base+2]>0)dir=lerp(dir,mul(cross(span,rotate(c->srot[base+2],s->bend_default_child[limb])),-1),c->erw[base+2]);
    /* ApplyBendConstraints copies the end effector's planeRotationOffset here
     * when iterations>0. Otherwise the original keeps a stale rotationOffset;
     * this owner then uses identity. */
    Q offset=c->full?c->plane_offset[5+limb]:IDENTITY;
    if(!qsame(offset,IDENTITY))dir=rotate(qmul(from_to(rotate(offset,span),span),offset),dir);
    return weight>0?lerp(dir,unit(s->bend_direction[limb]),weight):dir;
}
static void trigonometric(C *c,bool bend){
    for(unsigned k=1;k<PE_FBBIK_CHAINS;k++){
        unsigned base=k*3;V span=sub(c->sp[base+2],c->sp[base]);float m=mag(span);
        if(m==0)continue;
        float reach=clampf(m,0,c->length[k]*0.9990000128746033f);
        V dir=mul(mul(span,1/m),reach),normal=bend?bend_direction(c,k-1):sub(c->sp[base+1],c->sp[base]);
        float x=((reach*reach+c->sqr_dif[k])/2)/reach,y2=c->sqr1[k]-x*x,y=(float)sqrt((double)(y2<0?0:y2));
        c->sp[base+1]=add(c->sp[base],same(dir,ZERO)?ZERO:rotate(look(dir,normal),(V){0,y,x}));
    }
}
static void reach(C *c){
    for(unsigned k=1;k<PE_FBBIK_CHAINS;k++){
        unsigned base=k*3;float force=c->reach_force[k];
        if(!(force>0))continue;
        V span=sub(c->sp[base+2],c->sp[base]);
        if(same(span,ZERO))continue;
        float m=mag(span),f=clampf(m/c->length[k],1-force,1+force)-1;
        f=clampf(f+force,-1,1);
        if(c->r->reach_smoothing[k]==1)f*=f;else if(c->r->reach_smoothing[k]==2)f*=f*f;
        V step=mul(mul(mul(span,1/m),c->length[k]),clampf(f,0,m));
        c->sp[base]=add(c->sp[base],mul(step,1-c->epw[base]));
        c->sp[base+2]=add(c->sp[base+2],step);
    }
}
static void push(C *c){
    V sum=ZERO;
    for(unsigned k=1;k<PE_FBBIK_CHAINS;k++){
        unsigned base=k*3;
        if(!(c->r->push[k]>0))continue;
        V span=sub(c->sp[base+2],c->sp[base]);float m=mag(span);
        if(m==0)continue;
        float f=1-m/c->span[k];
        if(!(f>0))continue;
        if(c->r->push_smoothing[k]==1)f*=f;else if(c->r->push_smoothing[k]==2)f*=f*f;
        V step=mul(mul(mul(span,-1),f),c->r->push[k]);
        c->sp[base]=add(c->sp[base],step);
        sum=add(sum,mul(step,c->r->push_parent[k]));
    }
    c->sp[0]=add(c->sp[0],sum);
}
static void child_constraints(C *c){
    const R *r=c->r;
    for(unsigned i=0;i<PE_FBBIK_CONSTRAINTS;i++){
        float pull=r->constraint[i].pull_elasticity,push_e=c->push_elasticity[i];
        if(!(push_e<1)&&!(pull<1))continue;
        unsigned a=r->constraint[i].chain1*3,b=r->constraint[i].chain2*3;
        V d=sub(c->sp[b],c->sp[a]);float m=mag(d),f=1;
        if(m==c->nominal[i]||m==0)continue;
        if(!c->rigid[i])f=1-(m>c->nominal[i]?pull:push_e);
        f*=1-c->nominal[i]/m;
        V step=mul(d,f);
        c->sp[a]=add(c->sp[a],mul(step,c->fade[i]));
        c->sp[b]=sub(c->sp[b],mul(step,1-c->fade[i]));
    }
}
static void stage1(C *c){
    for(unsigned k=1;k<PE_FBBIK_CHAINS;k++){
        unsigned base=k*3;
        c->sp[base+1]=joint(c->sp[base+1],c->sp[base+2],c->len[base+1]);
        c->sp[base]=joint(c->sp[base],c->sp[base+1],c->len[base]);
    }
    V root=c->sp[0];
    child_constraints(c);
    for(unsigned k=1;k<PE_FBBIK_CHAINS;k++){
        V wanted=c->sp[k*3];
        if(c->root_length[k]>0)wanted=joint(c->sp[0],c->sp[k*3],c->root_length[k]);
        if(c->pull_parent_sum>0)root=add(root,mul(sub(wanted,c->sp[0]),c->r->pull[k]/c->pull_parent_sum));
    }
    c->sp[0]=lerp(root,c->sp[0],c->r->pin[0]);
}
static void stage2(C *c){
    unsigned passes=c->r->iterations<2?2:c->r->iterations>4?4:c->r->iterations;
    for(unsigned pass=0;pass<passes;pass++){
        child_constraints(c);
        for(unsigned k=1;k<PE_FBBIK_CHAINS;k++){
            V d=sub(c->sp[k*3],c->sp[0]);float m=mag(d),want=c->root_length[k];
            if(want==m||m==0)continue;
            V step=mul(d,1-want/m);
            c->sp[0]=add(c->sp[0],mul(step,c->cross_fade[k]));
            c->sp[k*3]=sub(c->sp[k*3],mul(step,1-c->cross_fade[k]));
        }
    }
    for(unsigned k=1;k<PE_FBBIK_CHAINS;k++){
        unsigned base=k*3;V root=c->sp[0];
        if(c->root_length[k]>0)root=joint(c->sp[base],root,c->root_length[k]);
        c->sp[base]=root;
        c->sp[base+1]=joint(c->sp[base+1],c->sp[base],c->len[base]);
        c->sp[base+2]=joint(c->sp[base+2],c->sp[base+1],c->len[base+1]);
    }
}

/* BoneMap.UpdatePlane: rotation into the animated plane frame, and optionally
 * the bone's position in it. */
static void update_plane(const C *c,unsigned bone,unsigned n1,unsigned n2,unsigned n3,Q *target,V *plane){
    const R *r=c->r;V a=pos(c,bone_of(r,n1));
    Q last=plane_look(a,pos(c,bone_of(r,n2)),pos(c,bone_of(r,n3)));
    *target=qinv(qmul(qinv(rot(c,bone)),last));
    if(plane)*plane=rotate(qinv(last),sub(pos(c,bone),a));
}
static Q target_rotation(const C *c,unsigned n1,unsigned n2,unsigned n3){return plane_look(c->sp[n1],c->sp[n2],c->sp[n3]);}
static void spine_read(C *c){
    const R *r=c->r;unsigned last=r->spine_count-1;
    update_plane(c,r->spine[0],0,3*3,4*3,&c->spine_target[0],&c->spine_plane[0]);
    V arms=sub(c->tp[1*3],c->tp[2*3]);
    for(unsigned i=0;i<last;i++){
        T a=world(r,c->l,r->spine[i]);V next=sub(pos(c,r->spine[i+1]),a.position),twist=arms,n=next;
        c->spine_length[i]=mag(next);
        c->spine_swing[i]=rotate(qinv(a.rotation),next);
        ortho(&n,&twist);
        c->spine_twist[i]=rotate(qinv(a.rotation),twist);
    }
    update_plane(c,r->spine[last],0,1*3,2*3,&c->spine_target[1],&c->spine_plane[1]);
}
static void spine_write(C *c){
    const R *r=c->r;unsigned last=r->spine_count-1,root=r->spine_root_index;
    V low=add(c->sp[0],rotate(target_rotation(c,0,3*3,4*3),c->spine_plane[0]));
    V high=add(c->sp[0],rotate(target_rotation(c,0,1*3,2*3),c->spine_plane[1]));
    if(r->spine_count>3||root!=1){
        V shift=sub(c->sp[0],pos(c,r->spine[root]));
        for(unsigned i=0;i<=last;i++)c->ik[i]=add(pos(c,r->spine[i]),shift);
        for(unsigned pass=0;pass<r->spine_iterations;pass++){
            c->ik[last]=high;
            for(unsigned i=last;i-->0;)c->ik[i]=joint(c->ik[i],c->ik[i+1],c->spine_length[i]);
            c->ik[0]=low;
            for(unsigned i=1;i<=last;i++)c->ik[i]=joint(c->ik[i],c->ik[i-1],c->spine_length[i-1]);
            c->ik[root]=c->sp[0];
        }
    }else{c->ik[0]=low;c->ik[root]=c->sp[0];}
    c->ik[last]=high;
    set_position(r,c->l,r->spine[0],c->ik[0]);
    set_rotation(r,c->l,r->spine[0],qmul(target_rotation(c,0,3*3,4*3),c->spine_target[0]));
    for(unsigned i=1;i<last;i++){
        T t=world(r,c->l,r->spine[i]);
        set_rotation(r,c->l,r->spine[i],qmul(from_to(rotate(t.rotation,c->spine_swing[i]),sub(c->ik[i+1],t.position)),t.rotation));
        if(!(r->spine_twist_weight>0))continue;
        float weight=(float)i/((float)r->spine_count-2)*r->spine_twist_weight;
        t=world(r,c->l,r->spine[i]);
        V twist=sub(c->sp[1*3],c->sp[2*3]),n=sub(c->ik[i+1],t.position);
        ortho(&n,&twist);
        turn(r,c->l,r->spine[i],qmul(from_to(rotate(t.rotation,c->spine_twist[i]),twist),t.rotation),weight);
    }
    set_position(r,c->l,r->spine[last],c->ik[last]);
    set_rotation(r,c->l,r->spine[last],qmul(target_rotation(c,0,1*3,2*3),c->spine_target[1]));
}
static void limb_read(C *c,unsigned limb){
    unsigned base=(limb+1)*3;const unsigned char *b=c->r->chain_bone[limb+1];
    update_plane(c,b[0],base,base+1,base+2,&c->limb_target1[limb],&c->limb_plane1[limb]);
    update_plane(c,b[1],base+1,base+2,base,&c->limb_target2[limb],0);
    c->limb_maintain[limb]=rot(c,b[2]);
}
static void limb_write(C *c,unsigned limb){
    const R *r=c->r;unsigned base=(limb+1)*3;const unsigned char *b=r->chain_bone[limb+1];
    float weight=clampf(r->limb_weight[limb],0,1);
    if(!(weight>0))return;
    if(c->full){
        if(r->limb_parent[limb]!=PE_FBBIK_NONE){
            T p=world(r,c->l,r->limb_parent[limb]);
            turn(r,c->l,r->limb_parent[limb],qmul(from_to(rotate(p.rotation,c->s->parent_swing_axis[limb]),sub(c->sp[base],p.position)),p.rotation),weight);
        }
        set_position(r,c->l,b[0],weight<1?lerp(pos(c,b[0]),c->sp[base],weight):c->sp[base]);
    }
    turn(r,c->l,b[0],qmul(target_rotation(c,base,base+1,base+2),c->limb_target1[limb]),weight);
    turn(r,c->l,b[1],qmul(target_rotation(c,base+1,base+2,base),c->limb_target2[limb]),weight);
    float maintain=r->limb_maintain_rotation[limb]*weight*c->w;
    if(maintain>0)set_rotation(r,c->l,b[2],qlerp(rot(c,b[2]),c->limb_maintain[limb],maintain));
    float effector=weight*c->erw[base+2];
    if(effector>0)turn(r,c->l,b[2],c->srot[base+2],effector);
}

bool pe_fbbik_solve(const PeFbbik *state,const PeFbbikInput *input,PeFbbikOutput *out){
    if(!state||!state->initiated||!input||!out||!valid_rig(state->rig))return false;
    const R *r=state->rig;
    if(!finite_pose(r,input->local)||!isfinite(input->weight))return false;
    for(unsigned e=0;e<PE_FBBIK_EFFECTORS;e++){
        const PeFbbikEffector *x=&input->effector[e];
        if(!finite_v(x->position_offset)||!finite_v(x->position)||!finite_q(x->rotation)||!isfinite(x->position_weight)
           ||!isfinite(x->rotation_weight)||!isfinite(x->maintain_relative_position_weight))return false;
    }
    for(unsigned i=0;i<PE_FBBIK_LIMBS;i++)if(!isfinite(input->bend_weight[i]))return false;
    PeFbbikOutput result={0};
    memcpy(result.local,input->local,sizeof result.local);
    if(!(input->weight>0)){*out=result;return true;}
    C c={.s=state,.r=r,.in=input,.w=clampf(input->weight,0,1),.full=r->iterations>0};
    memcpy(c.l,input->local,sizeof c.l);
    for(unsigned i=0;i<r->bone_count;i++)c.l[i].rotation=qunit(c.l[i].rotation);
    float push_elasticity=clampf(1-r->spine_stiffness,0,1);
    for(unsigned i=0;i<PE_FBBIK_CONSTRAINTS;i++)c.push_elasticity[i]=i<2?push_elasticity:r->constraint[i].push_elasticity;

    /* IKSolverFullBodyBiped.ReadPose, then IKSolverFullBody.ReadPose */
    pull_body(&c);
    for(unsigned i=0;i<PE_FBBIK_LIMBS;i++)limit_bend(&c,i,input->effector[5+i].position_weight);
    for(unsigned e=0;e<PE_FBBIK_EFFECTORS;e++)effector_pre_solve(&c,e);
    for(unsigned k=0;k<PE_FBBIK_CHAINS;k++)chain_read(&c,k);
    body_chain_read(&c);
    if(c.full){spine_read(&c);c.head_maintain=rot(&c,r->head);}
    for(unsigned i=0;i<PE_FBBIK_LIMBS;i++)limb_read(&c,i);

    /* IKSolverFullBody.Solve */
    for(unsigned i=0;i<r->iterations;i++){
        update_effectors(&c,true);
        push(&c);
        reach(&c);
        update_effectors(&c,false);
        trigonometric(&c,false);
        stage1(&c);
        update_effectors(&c,false);
        stage2(&c);
    }
    update_effectors(&c,true);
    if(!c.full){
        const PeFbbikEffector *body=&input->effector[0];
        c.offset=lerp(body->position_offset,sub(body->position,add(pos(&c,bone_of(r,r->effector_node[0])),body->position_offset)),body->position_weight);
        for(unsigned e=0;e<5;e++)c.sp[r->effector_node[e]]=add(c.sp[r->effector_node[e]],c.offset);
    }
    trigonometric(&c,true);

    /* WritePose */
    if(c.full){
        spine_write(&c);
        float head=c.w*r->head_maintain_rotation;
        if(head>0)set_rotation(r,c.l,r->head,qlerp(rot(&c,r->head),c.head_maintain,head));
    }else set_position(r,c.l,r->spine[0],add(pos(&c,r->spine[0]),c.offset));
    for(unsigned i=0;i<PE_FBBIK_LIMBS;i++)limb_write(&c,i);

    if(!finite_pose(r,c.l))return false;
    for(unsigned n=0;n<PE_FBBIK_NODES;n++)if(n%3<nodes_of(n/3)&&!finite_v(c.sp[n]))return false;
    result.solved=true;
    memcpy(result.local,c.l,sizeof result.local);
    memcpy(result.node,c.sp,sizeof result.node);
    *out=result;
    return true;
}
