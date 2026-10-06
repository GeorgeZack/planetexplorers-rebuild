#include "pe_capsule_history.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"capsule-history:%d %s\n",__LINE__,#c);failures++;}}while(0)
static bool near(PeVec3 a,PeVec3 b){return fabsf(a.x-b.x)<2e-5f&&fabsf(a.y-b.y)<2e-5f&&fabsf(a.z-b.z)<2e-5f;}
/* Golden outputs from exact original source methods compiled and run under the
 * installed game libmono + pinned UnityEngine managed assembly. This is pure
 * math in an isolated process, not a Unity scene execution. The seeded cases
 * specifically guard near-parallel branch/tie behavior: float-rounding every
 * scalar product changed these contact positions by up to1.43m. */
static const struct {const char *name;float input[23];int hit;float output[6];} golden[]={
    {"count1_previous_hit",{0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.100000001f,0.0f,0.0f,0.100000001f,0.0f,0.0f,1.0f,0.0f,0.0f,0.0f,-1.89999998f,0.0f,0.0f,-1.89999998f,0.0f,0.0f,1.0f},1,{-0.95f,0.0f,0.0f,-1.0f,0.0f,0.0f}},
    {"count1_previous_miss",{0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.100000001f,0.0f,0.0f,0.100000001f,0.0f,0.0f,1.0f,0.0f,0.0f,0.0f,2.0f,0.0f,0.0f,2.0f,0.0f,0.0f,1.0f},0,{0,0,0,0,0,0}},
    {"strict_tangent",{0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,1.0f,0.0f,0.0f,0.0f,2.0f,0.0f,0.0f,2.0f,0.0f,0.0f,1.0f},0,{0,0,0,0,0,0}},
    {"intermediate_only",{-3.0f,0.0f,0.0f,-3.0f,0.0f,0.0f,3.0f,0.0f,0.0f,3.0f,0.0f,0.0f,0.5f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.200000003f},1,{0.0f,0.0f,0.0f,1.0f,0.0f,0.0f}},
    {"parallel_tie",{0.0f,0.0f,0.0f,0.0f,1.0f,0.0f,0.0f,0.0f,0.0f,0.0f,1.0f,0.0f,0.5f,0.0f,0.0f,0.0f,0.200000003f,0.0f,0.0f,0.200000003f,1.0f,0.0f,0.5f},1,{0.1f,1.0f,0.0f,1.0f,0.0f,0.0f}},
    {"short_segment",{0.0f,0.0f,0.0f,0.000500000024f,0.0f,0.0f,0.0f,0.0f,0.0f,0.000500000024f,0.0f,0.0f,0.100000001f,0.0f,0.0f,0.0f,0.00039999999f,0.100000001f,0.0f,0.00039999999f,0.100500003f,0.0f,0.100000001f},1,{0.0002f,0.0500000045f,0.0f,0.003999968f,0.999992f,0.0f}},
    {"radial_quarter",{2.0f,0.0f,0.0f,2.0f,1.0f,0.0f,1.0f,0.0f,1.0f,1.0f,1.0f,1.0f,0.100000001f,1.0f,0.0f,0.0f,1.70700002f,0.0f,0.707000017f,1.70700002f,1.0f,0.707000017f,0.100000001f},1,{1.74941993f,0.06594201f,0.650440037f,-0.7023273f,0.0f,0.71185416f}},
    {"seeded_26",{2.04950953f,-3.26620317f,2.31935382f,0.336680353f,-3.79883575f,2.74400783f,0.440542251f,-4.72707367f,4.23637724f,0.585388064f,-5.47504663f,3.4642868f,0.258811712f,0.614387989f,-0.43849656f,-0.397759348f,1.55169606f,-3.18345451f,2.19087648f,-0.161133111f,-3.7160871f,2.61553073f,0.505768955f},1,{1.20135081f,-3.44954038f,2.44436622f,-0.362365216f,-0.719876349f,0.592004657f}},
    {"seeded_52",{-3.10332012f,2.69541526f,-2.05544043f,-2.81841922f,1.60787535f,-2.57039094f,-1.83394313f,3.69265556f,-0.379083753f,-2.88648915f,-0.0408718325f,-2.31154323f,0.153241545f,0.969163775f,0.351349175f,0.39498207f,-3.56617665f,3.41038132f,-2.20232654f,-3.28127599f,2.32284117f,-2.71727705f,0.446930498f},1,{-3.05228567f,2.23202872f,-2.39879656f,0.488194436f,-0.289244115f,0.82341f}},
    {"seeded_130",{3.91409492f,2.56283379f,-1.32454121f,2.48238087f,3.42788434f,-1.16900671f,2.47590256f,0.816020846f,-2.50904536f,1.82533729f,4.08581972f,-2.17382121f,0.795337796f,-0.759087145f,-0.870705485f,-0.908692241f,3.94388318f,2.66442347f,-2.10767603f,2.51216912f,3.52947402f,-1.9521414f,0.497775555f},1,{3.908525f,2.63975263f,-1.803618f,-0.559063137f,-0.256064832f,-0.7885932f}},
    {"seeded_182",{1.53793406f,0.183418423f,-3.04962659f,2.95658779f,1.66228294f,-3.82741404f,-0.347060114f,1.68416369f,-2.51596212f,1.89117968f,0.327981204f,-2.22166538f,0.788898647f,0.320418477f,-0.489365697f,0.912488103f,0.89091599f,0.944027841f,-3.00505567f,2.3095696f,2.42289233f,-3.78284311f,0.237263143f},1,{2.42006254f,2.20626187f,-3.77170777f,-0.8377177f,0.20069322f,0.5078889f}},
    {"seeded_299",{1.35788345f,0.912440002f,-0.211478218f,2.3994503f,2.13933897f,0.57913053f,1.3313688f,-0.770553172f,-1.40079367f,3.8008256f,2.92170382f,0.605656326f,0.661721587f,0.0f,0.0f,0.0f,1.52453005f,0.341689348f,0.044781018f,2.56609702f,1.56858826f,0.835389733f,0.655901313f},1,{1.49429941f,0.6879103f,-0.04276161f,0.721277f,-0.5903908f,-0.3622129f}},
    {"seeded_390",{-1.95471311f,-1.58480668f,-1.56398106f,-1.34897017f,-2.78616023f,-2.5516386f,-3.43294048f,-1.37226999f,-0.000758838491f,0.0140017839f,-4.29434443f,-4.32560635f,0.159016863f,-0.67501986f,-0.92077148f,0.235471174f,-2.81151009f,-0.733741164f,-1.70903301f,-2.20576715f,-1.93509471f,-2.69669056f,0.792310238f},1,{-1.74540007f,-2.14171f,-2.16302156f,-0.443503469f,-0.6954876f,-0.5653332f}},
    {"seeded_403",{-0.843680263f,1.28247666f,-3.19847441f,-0.836957693f,1.83613908f,-2.14577198f,-0.287103355f,3.26673961f,-3.54945374f,0.935997427f,2.88728619f,-2.02713537f,0.674624324f,0.0f,0.0f,0.0f,-0.879612446f,2.24313354f,-3.2881732f,-0.872889876f,2.79679608f,-2.23547101f,0.509643257f},1,{-0.858610153f,2.28591418f,-2.38219023f,0.5085932f,0.848934233f,-0.143679261f}},
    {"seeded_429",{0.909810722f,-1.70843887f,-2.80169582f,0.120292813f,-2.91361165f,-4.36850405f,0.965211213f,-3.14476848f,-2.43537855f,-0.538552165f,-1.68808424f,-2.93520069f,0.691776693f,0.0f,0.0f,0.0f,0.715820253f,-2.70490098f,-2.88490701f,-0.0736976415f,-3.91007376f,-4.45171547f,0.06040911f},1,{0.710624933f,-2.656586f,-2.91945243f,-0.22138761f,-0.333834261f,0.916265368f}},
    {"seeded_442",{2.83159828f,1.16782987f,-0.626633942f,2.01230073f,0.308121979f,-1.09414732f,2.33361721f,0.93105495f,0.740762472f,0.283029675f,1.2136426f,-1.80403841f,0.779394865f,-0.701303959f,0.105762206f,0.275885016f,2.80753922f,1.98913193f,-0.786882102f,1.98824179f,1.1294241f,-1.25439537f,0.0569016039f},1,{2.52255917f,1.63249683f,-0.939530134f,-0.9039042f,0.412356824f,0.113662295f}},
    {"seeded_481",{3.89174962f,0.817703545f,-0.998731315f,5.30181026f,1.55553246f,0.0283607095f,5.2753849f,2.69646645f,0.233599991f,5.59147596f,0.162205473f,0.84995079f,0.471811891f,0.0f,0.0f,0.0f,4.75874186f,1.67943382f,-1.26892281f,6.16880274f,2.41726279f,-0.241830766f,0.710408211f},1,{4.77853441f,1.44457865f,-0.7126582f,0.7527391f,0.400912344f,0.5221621f}},
    {"seeded_494",{-2.33224177f,-0.936440229f,1.95828629f,-1.88186932f,0.910467863f,1.15202463f,-2.37511778f,-0.516791582f,3.26295471f,-0.173484161f,1.93371201f,-0.279796809f,0.715502858f,0.338996559f,-0.690855265f,0.141746923f,-2.67890239f,-0.535404325f,2.625247f,-2.22852993f,1.31150377f,1.81898546f,0.628947616f},1,{-2.51441836f,-0.7135289f,2.30909562f,0.5678866f,0.788207352f,0.237137064f}},
};
static PeVec3 vec(const float *v){return (PeVec3){v[0],v[1],v[2]};}
static PeCapsuleHistory point_history(float from,float to,float radius){
    return (PeCapsuleHistory){.previous_a={from,0,0},.previous_b={from,0,0},.current={{to,0,0},{to,0,0},radius}};
}
static void rejected(const PeCapsuleHistory *h,PeHitboxCapsule target,PeCapsuleHistoryStatus status){
    PeCapsuleHistoryContact sentinel,out;memset(&sentinel,0x5a,sizeof sentinel);out=sentinel;
    CHECK(pe_capsule_history_contact(h,target,&out)==status);CHECK(!memcmp(&out,&sentinel,sizeof out));
}
int pe_test_capsule_history(void){
    failures=0;
    for(unsigned i=0;i<sizeof golden/sizeof golden[0];i++){
        const float *f=golden[i].input;
        PeCapsuleHistory h={vec(f),vec(f+3),{vec(f+6),vec(f+9),f[12]},vec(f+13)};
        PeHitboxCapsule t={vec(f+16),vec(f+19),f[22]};PeCapsuleHistoryContact out;
        PeCapsuleHistoryStatus status=pe_capsule_history_contact(&h,t,&out);
        if(status!=golden[i].hit){fprintf(stderr,"golden %s status\n",golden[i].name);failures++;continue;}
        if(status==PE_CAPSULE_HISTORY_HIT){
            CHECK(near(out.contact.point,vec(golden[i].output)));
            CHECK(near(out.hit_direction,vec(golden[i].output+3)));
            CHECK(out.sample_index<out.sample_count);
        }else rejected(&h,t,PE_CAPSULE_HISTORY_MISS);
    }
    PeCapsuleHistory h=point_history(-3,3,.5f);PeHitboxCapsule t={{0,0,0},{0,0,0},.2f};PeCapsuleHistoryContact out;
    CHECK(pe_capsule_history_contact(&h,t,&out)==PE_CAPSULE_HISTORY_HIT);
    CHECK(out.sample_count==7&&out.sample_index==3&&near(out.sampled.a,(PeVec3){0}));
    /* First source-order overlap, not nearest/end-of-frame contact. */
    t.radius=1.2f;CHECK(pe_capsule_history_contact(&h,t,&out)==PE_CAPSULE_HISTORY_HIT);
    CHECK(out.sample_index==2&&fabsf(out.sampled.a.x+1)<2e-6f);
    /* Current radius determines both work count and overlap, not old radius. */
    h.current.radius=3;CHECK(pe_capsule_history_contact(&h,t,&out)==PE_CAPSULE_HISTORY_HIT);
    CHECK(out.sample_count==2&&out.sample_index==0&&out.sampled.radius==3);
    h=point_history(0,.1f,1);t=(PeHitboxCapsule){{2,0,0},{2,0,0},1};
    rejected(&h,t,PE_CAPSULE_HISTORY_MISS); /* current would hit; only previous sampled */
    h=point_history(0,0,1);t.a.x=t.b.x=2;
    rejected(&h,t,PE_CAPSULE_HISTORY_MISS); /* strict tangent */
    t.a.x=t.b.x=nextafterf(2,0);CHECK(pe_capsule_history_contact(&h,t,&out)==PE_CAPSULE_HISTORY_HIT);
    /* Unity normalization uses magnitude>1e-5, equality squared<~1e-10. */
    t.a.x=t.b.x=0.000009f;CHECK(pe_capsule_history_contact(&h,t,&out)==PE_CAPSULE_HISTORY_HIT);
    CHECK(near(out.hit_direction,(PeVec3){0}));
    t.a.x=t.b.x=0.000011f;CHECK(pe_capsule_history_contact(&h,t,&out)==PE_CAPSULE_HISTORY_HIT);
    CHECK(near(out.hit_direction,(PeVec3){1,0,0}));
    h=point_history(0,0,1);t=(PeHitboxCapsule){{0,0,0},{0,0,0},1};
    CHECK(pe_capsule_history_contact(&h,t,&out)==PE_CAPSULE_HISTORY_HIT);
    CHECK(near(out.hit_direction,(PeVec3){0})); /* coincident axes no invented fallback normal */
    /* A near-zero parent disables radial correction by Unity vector equality;
     * a parent just beyond that threshold produces the arc, missing the chord. */
    h=(PeCapsuleHistory){.previous_a={1,0,0},.previous_b={1,0,0},
        .current={{0,0,1},{0,0,1},.05f},.parent_center={.000001f,0,0}};
    t=(PeHitboxCapsule){{.5f,0,.5f},{.5f,0,.5f},.001f};
    CHECK(pe_capsule_history_contact(&h,t,&out)==PE_CAPSULE_HISTORY_HIT);
    CHECK(out.sample_count==15&&out.sample_index==7);
    h.parent_center.x=.00002f;rejected(&h,t,PE_CAPSULE_HISTORY_MISS);
    h=point_history(0,8190,1);t=(PeHitboxCapsule){{0,20,0},{0,20,0},1};
    rejected(&h,t,PE_CAPSULE_HISTORY_MISS); /*4096 samples admitted, all visited*/
    t=(PeHitboxCapsule){{0,0,0},{0,0,0},1};CHECK(pe_capsule_history_contact(&h,t,&out)==PE_CAPSULE_HISTORY_HIT);
    CHECK(out.sample_count==PE_CAPSULE_HISTORY_MAX_SAMPLES);
    h.current.a.x=h.current.b.x=8192;rejected(&h,t,PE_CAPSULE_HISTORY_INVALID);
    h=point_history(0,0,1);h.previous_a.x=NAN;rejected(&h,t,PE_CAPSULE_HISTORY_INVALID);
    h=point_history(0,0,1);h.parent_center.z=INFINITY;rejected(&h,t,PE_CAPSULE_HISTORY_INVALID);
    h=point_history(0,0,0);rejected(&h,t,PE_CAPSULE_HISTORY_INVALID);
    h=point_history(0,0,1);h.current.radius=INFINITY;rejected(&h,t,PE_CAPSULE_HISTORY_INVALID);
    h=point_history(0,0,1);h.current.a.x=1000001;rejected(&h,t,PE_CAPSULE_HISTORY_INVALID);
    h=point_history(0,0,1);t.radius=-1;rejected(&h,t,PE_CAPSULE_HISTORY_INVALID);
    t.radius=1;rejected(NULL,t,PE_CAPSULE_HISTORY_INVALID);
    CHECK(pe_capsule_history_contact(&h,t,NULL)==PE_CAPSULE_HISTORY_INVALID);
    return failures;
}
