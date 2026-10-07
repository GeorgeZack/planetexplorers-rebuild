#include "pe_sword_variant.h"
#include "../generated/pe_sword_variant_source.h"
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"sword variant:%d: %s\n",__LINE__,#c);failures++;}}while(0)
static void expect(PeVec3 v,PeSwordVariant expected){PeSwordVariant actual=PE_SWORD_VARIANT_AIR;CHECK(pe_sword_variant_select((PeSwordVariantInput){.velocity=v,.movement_present=true},&actual));CHECK(actual==expected);}
int pe_test_sword_variant(void){
    failures=0;CHECK(pe_sword_variant_source.move_speed==4.5f&&pe_sword_variant_source.sprint_speed==7&&pe_sword_variant_source.velocity_change_limit==12);
    expect((PeVec3){0},PE_SWORD_VARIANT_ORDINARY);expect((PeVec3){.05f,0,0},PE_SWORD_VARIANT_ORDINARY);
    expect((PeVec3){2.5f,0,0},PE_SWORD_VARIANT_ORDINARY);expect((PeVec3){4.5f,0,0},PE_SWORD_VARIANT_ORDINARY);
    expect((PeVec3){nextafterf(4.5f,INFINITY),0,0},PE_SWORD_VARIANT_RUNNING);
    expect((PeVec3){0,7,0},PE_SWORD_VARIANT_RUNNING);expect((PeVec3){0,0,nextafterf(7,INFINITY)},PE_SWORD_VARIANT_SPRINT);
    /* Golden outputs from pure installed Mono+UnityEngine.Vector3.magnitude,
     * corpus4101; these expectations do not call the C implementation oracle. */
    const struct {PeVec3 v;uint32_t bits;PeSwordVariant branch;} gold[]={
        {{2.7f,3.6f,0},0x40900000,PE_SWORD_VARIANT_ORDINARY},
        {{-2.2190206050872803f,-2.0396177768707275f,3.341542959213257f},0x40900000,PE_SWORD_VARIANT_ORDINARY},
        {{1.5f,3,3},0x40900000,PE_SWORD_VARIANT_ORDINARY},
        {{4.2f,5.6f,0},0x40dfffff,PE_SWORD_VARIANT_RUNNING},
        {{-4.326123237609863f,.9592961072921753f,.7838452458381653f},0x40900001,PE_SWORD_VARIANT_RUNNING},
        {{2.4800307750701904f,-3.053530216217041f,2.185267686843872f},0x408fffff,PE_SWORD_VARIANT_ORDINARY},
        {{-1.6282126903533936f,5.6058125495910645f,-3.8631324768066406f},0x40e00001,PE_SWORD_VARIANT_SPRINT},
        {{-2.2478020191192627f,6.598243236541748f,.6407603025436401f},0x40e00000,PE_SWORD_VARIANT_RUNNING}};
    for(unsigned i=0;i<sizeof gold/sizeof gold[0];i++){
        float speed=-1;uint32_t bits=0;CHECK(pe_sword_variant_speed(gold[i].v,&speed));memcpy(&bits,&speed,sizeof bits);CHECK(bits==gold[i].bits);expect(gold[i].v,gold[i].branch);
    }
    PeSwordVariantInput in={.velocity={100,0,0},.movement_present=true,.in_air=true,.motor_present=true,.spine_in_water=true};PeSwordVariant out;
    CHECK(pe_sword_variant_select(in,&out)&&out==PE_SWORD_VARIANT_AIR);
    in.in_air=false;CHECK(pe_sword_variant_select(in,&out)&&out==PE_SWORD_VARIANT_WATER);
    in.motor_present=false;CHECK(pe_sword_variant_select(in,&out)&&out==PE_SWORD_VARIANT_SPRINT);
    in.movement_present=false;CHECK(pe_sword_variant_select(in,&out)&&out==PE_SWORD_VARIANT_ORDINARY);
    in.motor_present=true;CHECK(pe_sword_variant_select(in,&out)&&out==PE_SWORD_VARIANT_WATER);
    const PeVec3 invalid[]={{NAN,0,0},{0,INFINITY,0},{0,0,-INFINITY},{FLT_MAX,0,0}};
    for(unsigned i=0;i<sizeof invalid/sizeof invalid[0];i++){
        float value=123;out=PE_SWORD_VARIANT_RUNNING;in.velocity=invalid[i];
        CHECK(!pe_sword_variant_speed(invalid[i],&value)&&value==123);
        CHECK(!pe_sword_variant_select(in,&out)&&out==PE_SWORD_VARIANT_RUNNING);
    }
    CHECK(!pe_sword_variant_select((PeSwordVariantInput){0},NULL));CHECK(!pe_sword_variant_speed((PeVec3){0},NULL));
    return failures;
}
