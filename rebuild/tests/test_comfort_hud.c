#include "pe_comfort_hud.h"
#include "../generated/pe_comfort_hud_source.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"comfort hud:%d %s\n",__LINE__,#c);failures++;}}while(0)
static float curve(float t){float v=NAN;CHECK(pe_comfort_hud_curve(t,&v));return v;}
static PeComfortHud settled(float comfort){PeComfortHud h;pe_comfort_hud_reset(&h);CHECK(pe_comfort_hud_frame(&h,comfort,500,0));return h;}
int pe_test_comfort_hud(void){
    failures=0;
    /* Serialized prefab values. */
    CHECK(pe_comfort_hud_source.fall_value==10&&pe_comfort_hud_source.initial_last_comfort==0&&pe_comfort_hud_source.initial_alpha==1);
    PeComfortHud h;pe_comfort_hud_reset(&h);
    CHECK(h.last_comfort==0&&h.alpha==1&&h.slider==1&&!h.playing&&h.curve_time==0&&h.face==PE_COMFORT_FACE3);
    CHECK(!strcmp(pe_comfort_hud_face_sprite(PE_COMFORT_FACE1),"face1")&&!strcmp(pe_comfort_hud_face_sprite(PE_COMFORT_FACE3),"face3")&&!pe_comfort_hud_face_sprite(0));

    /* The curve passes through every key, clamps both ends and rises to exactly 1 at 1 s. */
    for(unsigned i=0;i<PE_COMFORT_HUD_SOURCE_KEYS;i++)CHECK(curve(pe_comfort_hud_source_keys[i].time)==pe_comfort_hud_source_keys[i].value);
    CHECK(curve(-5)==curve(0)&&curve(0)==.98f&&curve(1)==1&&curve(1e9f)==1&&curve(.5f)==0);
    float v;CHECK(!pe_comfort_hud_curve(NAN,&v)&&!pe_comfort_hud_curve(INFINITY,&v)&&!pe_comfort_hud_curve(0,NULL));
    float previous=curve(.85f);
    for(float t=.86f;t<1;t+=.01f){float a=curve(t);CHECK(a>previous&&a<1);previous=a;}
    CHECK(curve(.4f)>0&&curve(.4f)<.6f&&curve(.25f)>.3f&&curve(.25f)<.6f);

    /* A fresh component's first frame never flashes: it rises from 0. */
    h=settled(300);CHECK(!h.playing&&h.alpha==1&&h.last_comfort==300&&fabsf(h.slider-.6f)<1e-6f&&h.face==PE_COMFORT_FACE1);
    /* A drop of exactly the fall value does not flash; more than that does, from time 0 plus this frame. */
    CHECK(pe_comfort_hud_frame(&h,290,500,.1f)&&!h.playing&&h.alpha==1&&h.last_comfort==290);
    CHECK(pe_comfort_hud_frame(&h,279.5f,500,.1f)&&h.playing&&h.curve_time==.1f&&h.alpha==curve(.1f)&&h.last_comfort==279.5f);
    /* A rise while playing keeps the flash going and stores the new value. */
    CHECK(pe_comfort_hud_frame(&h,281,500,.1f)&&h.playing&&fabsf(h.curve_time-.2f)<1e-6f&&h.last_comfort==281);
    /* A second large drop restarts the curve. */
    CHECK(pe_comfort_hud_frame(&h,250,500,.05f)&&h.playing&&h.curve_time==.05f&&h.alpha==curve(.05f));
    /* It stops on the first frame whose alpha reaches 1, and alpha keeps that value. */
    unsigned frames=0;while(h.playing&&frames<100){CHECK(pe_comfort_hud_frame(&h,250,500,.1f));frames++;}
    CHECK(!h.playing&&h.alpha==1&&frames==10);
    CHECK(pe_comfort_hud_frame(&h,250,500,.1f)&&h.alpha==1&&h.curve_time>=1);
    /* One long frame jumps straight to the clamped end. */
    h=settled(300);CHECK(pe_comfort_hud_frame(&h,200,500,3)&&!h.playing&&h.alpha==1&&h.curve_time==3);
    /* A zero-time frame during a drop leaves alpha at the curve's start. */
    h=settled(300);CHECK(pe_comfort_hud_frame(&h,200,500,0)&&h.playing&&h.alpha==.98f);
    /* Many small drops never add up: the comparison is with the previous frame. */
    h=settled(500);for(int i=1;i<=40;i++)CHECK(pe_comfort_hud_frame(&h,500-9.9f*i,500,.016f));CHECK(!h.playing&&h.alpha==1);
    /* Paused hosts simply do not call it; the state holds. A drop across the gap flashes on return. */
    h=settled(300);PeComfortHud held=h;CHECK(!memcmp(&held,&h,sizeof h));
    CHECK(pe_comfort_hud_frame(&h,275,500,.02f)&&h.playing);

    /* Face thresholds on the slider: < 0.2 face3, < 0.5 face2, else face1. */
    h=settled(100);CHECK(h.face==PE_COMFORT_FACE2);           /* exactly 0.2 */
    h=settled(99.99f);CHECK(h.face==PE_COMFORT_FACE3);
    h=settled(250);CHECK(h.face==PE_COMFORT_FACE1);           /* exactly 0.5 */
    h=settled(249.99f);CHECK(h.face==PE_COMFORT_FACE2);
    h=settled(0);CHECK(h.face==PE_COMFORT_FACE3&&h.slider==0&&!h.playing);
    /* A maximum that is not positive gives 0; the fall check still reads raw comfort. */
    pe_comfort_hud_reset(&h);CHECK(pe_comfort_hud_frame(&h,300,0,0)&&h.slider==0&&h.face==PE_COMFORT_FACE3&&h.last_comfort==300);
    CHECK(pe_comfort_hud_frame(&h,280,-1,.1f)&&h.playing&&h.slider==0);
    /* Over the maximum: the slider clamps (adapter). */
    pe_comfort_hud_reset(&h);CHECK(pe_comfort_hud_frame(&h,600,500,0)&&h.slider==1&&h.face==PE_COMFORT_FACE1);

    /* Invalid input or state changes nothing. */
    h=settled(300);held=h;
    CHECK(!pe_comfort_hud_frame(&h,NAN,500,.1f)&&!pe_comfort_hud_frame(&h,300,INFINITY,.1f)&&!pe_comfort_hud_frame(&h,300,500,-.1f)&&
          !pe_comfort_hud_frame(&h,300,500,NAN)&&!pe_comfort_hud_frame(&h,INFINITY,500,.1f)&&!pe_comfort_hud_frame(NULL,300,500,.1f));
    CHECK(!memcmp(&held,&h,sizeof h));
    h.face=0;held=h;CHECK(!pe_comfort_hud_frame(&h,300,500,.1f)&&!memcmp(&held,&h,sizeof h));
    h=settled(300);h.slider=2;held=h;CHECK(!pe_comfort_hud_frame(&h,300,500,.1f)&&!memcmp(&held,&h,sizeof h));
    h=settled(300);h.curve_time=-1;held=h;CHECK(!pe_comfort_hud_frame(&h,300,500,.1f)&&!memcmp(&held,&h,sizeof h));
    h=settled(300);h.alpha=NAN;held=h;CHECK(!pe_comfort_hud_frame(&h,300,500,.1f));
    /* A curve time that overflows fails without changing the state. */
    h=settled(300);CHECK(pe_comfort_hud_frame(&h,200,500,0));h.curve_time=3e38f;held=h;
    CHECK(!pe_comfort_hud_frame(&h,200,500,3e38f)&&!memcmp(&held,&h,sizeof h));
    pe_comfort_hud_reset(NULL);
    return failures?1:0;
}
