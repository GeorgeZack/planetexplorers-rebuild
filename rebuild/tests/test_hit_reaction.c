#include "pe_hit_reaction.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static PeHitReactionInput player(bool shield){
    PeHitReactionInput in={0};
    assert(pe_hit_reaction_player_thresholds(shield,in.thresholds));return in;
}
static PeHitReactionDecision decide(PeHitReactionInput in,float force){
    PeHitReactionDecision out;in.force=force;assert(pe_hit_reaction_decide(in,&out));return out;
}
static void boundaries(void){
    const float thresholds[4]={250,400,750,1750};
    for(unsigned air=0;air<2;air++){
        PeHitReactionInput in=player(false);in.in_air=air!=0;
        for(unsigned i=0;i<4;i++){
            float force=thresholds[i]*(air?.5f:1.f);
            PeHitReactionDecision below=decide(in,nextafterf(force,-INFINITY));
            PeHitReactionDecision exact=decide(in,force),above=decide(in,nextafterf(force,INFINITY));
            assert((unsigned)below.effect==i);assert((unsigned)exact.effect==i+1);
            assert(above.effect==exact.effect);
            assert(exact.threshold_scale==(air?.5f:1.f));
        }
    }
    PeHitReactionInput in=player(false);
    PeHitReactionDecision out=decide(in,0);assert(out.effect==PE_HIT_REACTION_NONE&&out.request_hit_ik&&out.requested_action==0);
    out=decide(in,250);assert(out.effect==PE_HIT_REACTION_WHACKED&&!out.request_hit_ik&&out.requested_action==5);
    out=decide(in,400);assert(out.effect==PE_HIT_REACTION_REPULSED&&out.request_hit_ik&&out.requested_action==6);
    out=decide(in,750);assert(out.effect==PE_HIT_REACTION_WENTFLY&&!out.request_hit_ik&&out.requested_action==7);
    out=decide(in,1750);assert(out.effect==PE_HIT_REACTION_KNOCKED&&!out.request_hit_ik&&out.requested_action==7);
    /* Source does not sort modified thresholds before its if/else chain. */
    in.thresholds[0]=500;in.thresholds[1]=100;out=decide(in,400);assert(out.effect==PE_HIT_REACTION_NONE);
    out=decide(in,500);assert(out.effect==PE_HIT_REACTION_REPULSED);
}
static void angles_and_shield(void){
    PeHitReactionInput in=player(false);in.force=400;
    const float angles[]={0,45,90,135,180};
    const float threshold[]={1,.9925000369548798f,.9850000739097595f,.9910000562667847f,.9970000386238098f};
    const float force[]={400,398.0000138282776f,396.0000276565552f,398.0000138282776f,400};
    for(unsigned i=0;i<5;i++){
        in.angle_degrees=angles[i];PeHitReactionDecision out;
        assert(pe_hit_reaction_decide(in,&out));
        assert(fabsf(out.threshold_scale-threshold[i])<2e-7f);
        assert(fabsf(out.scaled_force-force[i])<1e-4f);
        assert(out.effect==PE_HIT_REACTION_REPULSED);
    }
    in.angle_degrees=0;assert(decide(in,249).effect==PE_HIT_REACTION_NONE);
    in.angle_degrees=90;assert(decide(in,249).effect==PE_HIT_REACTION_WHACKED);
    /* Shield buff thresholds change reaction class, not HP damage or facing. */
    in=player(true);assert(in.thresholds[0]==250&&in.thresholds[1]==300&&in.thresholds[2]==9999&&in.thresholds[3]==9999);
    assert(decide(in,299).effect==PE_HIT_REACTION_WHACKED);
    assert(decide(in,300).effect==PE_HIT_REACTION_REPULSED);
    assert(decide(in,1750).effect==PE_HIT_REACTION_REPULSED);
    assert(decide(in,9999).effect==PE_HIT_REACTION_KNOCKED);
    in.angle_degrees=180;assert(decide(in,400).effect==PE_HIT_REACTION_REPULSED);
    /* No manufactured 30-degree shield cone in damage/reaction selection. */
    for(int angle=0;angle<=180;angle++){
        in.angle_degrees=(float)angle;assert(decide(in,400).effect==PE_HIT_REACTION_REPULSED);
    }
}
static void events_and_invalid(void){
    const int ids[]={10100,10400,10250};const float forces[]={100,400,250};
    const PeHitReactionEffect effects[]={PE_HIT_REACTION_NONE,PE_HIT_REACTION_REPULSED,PE_HIT_REACTION_WHACKED};
    for(unsigned i=0;i<3;i++){
        float force=-1;assert(pe_hit_reaction_hare_force(ids[i],&force)&&force==forces[i]);
        assert(decide(player(false),force).effect==effects[i]);
    }
    float force=123;assert(!pe_hit_reaction_hare_force(57,&force)&&force==123);
    assert(!pe_hit_reaction_hare_force(10100,NULL));assert(!pe_hit_reaction_player_thresholds(false,NULL));
    PeHitReactionDecision out;memset(&out,0xa5,sizeof out);PeHitReactionDecision before=out;
    PeHitReactionInput in=player(false);assert(!pe_hit_reaction_decide(in,NULL));
    const float invalid_angles[]={-1,180.001f,INFINITY,-INFINITY,NAN};
    for(unsigned i=0;i<sizeof invalid_angles/sizeof *invalid_angles;i++){
        in.angle_degrees=invalid_angles[i];assert(!pe_hit_reaction_decide(in,&out));assert(!memcmp(&before,&out,sizeof out));
    }
    const float invalid_scalars[]={-1,INFINITY,-INFINITY,NAN};
    for(unsigned i=0;i<sizeof invalid_scalars/sizeof *invalid_scalars;i++){
        in=player(false);in.force=invalid_scalars[i];assert(!pe_hit_reaction_decide(in,&out));assert(!memcmp(&before,&out,sizeof out));
        for(unsigned j=0;j<4;j++){
            in=player(false);in.thresholds[j]=invalid_scalars[i];assert(!pe_hit_reaction_decide(in,&out));assert(!memcmp(&before,&out,sizeof out));
        }
    }
    in=player(false);assert(decide(in,FLT_MAX).effect==PE_HIT_REACTION_KNOCKED);
    memset(in.thresholds,0,sizeof in.thresholds);assert(decide(in,0).effect==PE_HIT_REACTION_KNOCKED);
}
int main(void){boundaries();angles_and_shield();events_and_invalid();puts("hit reaction decision checks passed");return 0;}
