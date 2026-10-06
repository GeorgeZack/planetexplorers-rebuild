#include "pe_attack_playback.h"
#include "../generated/pe_attack_playback_source.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"attack playback:%d %s\n",__LINE__,#x);failures++;}}while(0)
int pe_test_attack_playback(void){
    failures=0;PeAttackPlayback p={0};PeAttackPlaybackSample s={0};float speed=-1;
    /* Independent installed-native selector/coefficient/Horner replay. See
     * docs/reference/unity5-curve-arithmetic.json; exact-key side matters. */
    const uint32_t golden[][2]={{0x3d800000,0x3e835cc4},{0x3dcccccb,0x3c23d740},
        {0x3dcccccc,0x3c23d70a},{0x3dcccccd,0x3c23d70a},{0x3e400000,0x3c23d70a},
        {0x3ec00000,0x3f1f06ab},{0x3f200000,0x3f94ec80},{0x3f7fdff6,0x3f7fff4b},{0x3f7fffff,0x3f7fff4b}};
    for(unsigned i=0;i<sizeof golden/sizeof golden[0];i++){
        float time,value;uint32_t bits;memcpy(&time,&golden[i][0],sizeof time);
        CHECK(pe_attack_playback_curve(time,&value));memcpy(&bits,&value,sizeof bits);CHECK(bits==golden[i][1]);
    }
    float untouched=-7;CHECK(!pe_attack_playback_curve(NAN,&untouched)&&untouched==-7);
    CHECK(!pe_attack_playback_curve(-.01f,&untouched)&&untouched==-7);
    CHECK(!pe_attack_playback_curve(nextafterf(1,INFINITY),&untouched)&&untouched==-7);
    CHECK(!pe_attack_playback_contact(&p));
    CHECK(pe_attack_playback_sample(&p,0,33,100,&s)&&s.curve_active&&s.curve_time==0);
    CHECK(pe_attack_playback_speed(s,pe_attack_playback_keys[0].value,&speed)&&speed==1.37f);
    CHECK(pe_attack_playback_sample(&p,100,33,100,&s)&&!s.curve_active);
    CHECK(pe_attack_playback_speed(s,NAN,&speed)&&speed==1.37f);
    CHECK(pe_attack_playback_contact(&p)&&p.last_contact==100);
    CHECK(pe_attack_playback_sample(&p,100,33,100,&s)&&!s.curve_active&&s.elapsed==100);
    CHECK(p.frame_contact==0); /* Same-display catch-up keeps pre-contact speed. */
    /* Notify reset is independent of damage and can repeat at one frame time. */
    CHECK(pe_attack_playback_contact(&p)&&p.last_contact==100);
    CHECK(pe_attack_playback_sample(&p,100.1f,33,20,&s)&&s.curve_active&&s.elapsed>0);
    CHECK(p.frame_contact==100);
    CHECK(pe_attack_playback_speed(s,.01f,&speed)&&speed<.02f); /* No final clamp. */
    CHECK(pe_attack_playback_speed(s,1.3f,&speed)&&speed>1.4f);
    /* Source timestamp survives different weapon/action/cancel lifetimes. */
    CHECK(pe_attack_playback_sample(&p,100.2f,1,100,&s)&&s.curve_active&&p.last_contact==100);
    CHECK(pe_attack_playback_speed(s,1.3f,&speed)&&speed==1.3f);
    CHECK(pe_attack_playback_contact(&p)&&p.last_contact==100.2f);
    CHECK(pe_attack_playback_sample(&p,100.3f,1,0,&s)&&s.curve_active);
    p=(PeAttackPlayback){0};float duration=pe_attack_playback_source.duration_seconds;
    CHECK(pe_attack_playback_sample(&p,nextafterf(duration,0),33,100,&s)&&s.curve_active&&s.curve_time<1);
    CHECK(pe_attack_playback_sample(&p,duration,33,100,&s)&&!s.curve_active);
    CHECK(pe_attack_playback_speed(s,0,&speed)&&speed==1.37f);
    PeAttackPlayback before=p;PeAttackPlaybackSample prior=s;
    CHECK(!pe_attack_playback_sample(&p,nextafterf(duration,0),33,100,&s));
    CHECK(!memcmp(&p,&before,sizeof p)&&!memcmp(&s,&prior,sizeof s));
    CHECK(!pe_attack_playback_sample(&p,NAN,33,100,&s));
    CHECK(!pe_attack_playback_sample(&p,duration,34,100,&s));
    CHECK(!pe_attack_playback_sample(&p,duration,33,NAN,&s));
    CHECK(!memcmp(&p,&before,sizeof p)&&!memcmp(&s,&prior,sizeof s));
    s.curve_active=true;float old_speed=speed;
    CHECK(!pe_attack_playback_speed(s,1,&speed)&&speed==old_speed);
    p.last_contact=INFINITY;before=p;
    CHECK(!pe_attack_playback_contact(&p)&&!memcmp(&p,&before,sizeof p));
    return failures;
}
