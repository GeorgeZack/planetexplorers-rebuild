#include "pe_shield_action.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef PE_SOURCE_ROOT
#define PE_SOURCE_ROOT "."
#endif
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"shield action:%d %s\n",__LINE__,#c);failures++;}}while(0)
#define BIT(i) PE_SHIELD_ACTION_BIT(i)
static PeShieldActionInput input(void){return (PeShieldActionInput){.request=PE_SHIELD_REQUEST_HOLD,
    .shield_instance_id=91,.animator_present=true,.move_present=true,.host_capable=true};}
static void atomic(PeShieldAction *a,PeShieldActionInput in){
    PeShieldAction old=*a;PeShieldActionUpdate out;memset(&out,0x5a,sizeof out);PeShieldActionUpdate saved=out;
    CHECK(!pe_shield_action_step(a,in,&out));CHECK(!memcmp(a,&old,sizeof old)&&!memcmp(&out,&saved,sizeof out));
}
/* Validate all56 actual exported row15 mask columns, not merely the two most
 * visible SwordAttack/Sprint fields. The existing exporter binds this JSON to
 * original database/source hashes; no Unity/runtime assets are needed. */
static void source_row(void){
    static const char *const names[]={"Ground","InAir","InWater","Rotate","Sprint","Jump","Repulsed","Wentfly","Knocked","Death","GetUp","Revive","Step","HoldEquip","SwordAttack","HoldShield","Halt","Fall","GunHold","GunReload","GunShoot","BowHold","BowShoot","BowReload","Sleep","Eat","HoldAimEquip","Dig","Fell","Gather","PickUpItem","GetOnVehicle","JetPack","Parachute","Glider","Climb","Build","HoldFlashlight","Stuned","Pump","Sit","Operation","Draw","Throw","Lie","GetOnTrain","Cutscene","Whacked","Cure","HeavyEquipment","DrillingDig","Hand","Leisure","RopeGunShoot","Abnormal","Ride"};
    FILE *f=fopen(PE_SOURCE_ROOT "/rebuild/generated/pe_starter_sword_shield_source.json","rb");CHECK(f);if(!f)return;
    char text[131072];size_t size=fread(text,1,sizeof text-1,f);CHECK(!ferror(f)&&feof(f));fclose(f);text[size]=0;
    char *row=strstr(text,"\"ActionType\": \"15\"");CHECK(row);if(!row)return;
    char *end=strchr(row,'}');CHECK(end);if(!end)return;*end=0;
    CHECK(sizeof names/sizeof names[0]==PE_SHIELD_MASK_COUNT);
    unsigned required=0;
    for(unsigned i=0;i<PE_SHIELD_MASK_COUNT;i++){
        char key[96];snprintf(key,sizeof key,"\"%s\": \"",names[i]);char *value=strstr(row,key);CHECK(value);if(!value)continue;
        value+=strlen(key);CHECK(value[0]=='0'||value[0]=='2');
        CHECK(((pe_shield_action_forbidden_masks()>>i)&1u)==(unsigned)(value[0]=='2'));required+=value[0]=='2';
    }
    CHECK(required==26);
    CHECK(strstr(row,"\"PauseAction\": \"40\"")&&strstr(row,"\"EndAction\": \"12,58\"")&&strstr(row,"\"EndImmediately\": \"2\""));
}
int pe_test_shield_action(void){
    failures=0;source_row();PeShieldAction a={0};PeShieldActionUpdate out;PeShieldActionInput in=input();
    /* No stamina/durability fields to invent thresholds; all source unconstrained
     * bits, including air/water/not grounded and absent EquipmentHold, admit. */
    for(unsigned i=0;i<PE_SHIELD_MASK_COUNT;i++){
        in=input();in.source_masks=BIT(i);a=(PeShieldAction){0};
        bool allowed=!(pe_shield_action_forbidden_masks()&BIT(i));
        CHECK(pe_shield_action_source_admissible(true,in.source_masks)==allowed);
        CHECK(pe_shield_action_step(&a,in,&out)&&out.holding==allowed&&out.denied==!allowed);
    }
    CHECK(!pe_shield_action_source_admissible(false,0));
    in=input();a=(PeShieldAction){0};CHECK(pe_shield_action_step(&a,in,&out));
    CHECK(out.admitted&&out.started&&out.holding&&out.mount_buff&&!out.cancel_buff&&out.hold_buff_id==30200177);
    CHECK(out.set_sword_style&&!out.restore_base_style&&a.instance_id==91);
    CHECK(out.pause_actions==BIT(40)&&out.end_actions==(BIT(12)|BIT(58))&&out.end_immediately_actions==BIT(2));
    /* Held input is idempotent; active DoAction bypasses admission dependencies.
     * Accepted SwordAttack relation cancellation is a separate explicit event. */
    in.source_masks=pe_shield_action_forbidden_masks();CHECK(pe_shield_action_step(&a,in,&out));
    CHECK(out.holding&&!out.admitted&&!out.started&&!out.mount_buff&&!out.pause_actions);
    in.request=PE_SHIELD_REQUEST_NONE;in.action_paused=true;CHECK(pe_shield_action_step(&a,in,&out)&&out.holding);
    in.action_paused=false;
    in.cancel=true;in.request=PE_SHIELD_REQUEST_HOLD;CHECK(pe_shield_action_step(&a,in,&out));
    CHECK(out.ended&&!out.holding&&out.cancel_buff&&out.continue_actions==BIT(40)&&out.restore_base_style&&a.phase==PE_SHIELD_ACTION_IDLE);
    in.cancel=false;CHECK(pe_shield_action_step(&a,in,&out)&&out.denied&&!out.holding);
    in.source_masks=0;CHECK(pe_shield_action_step(&a,in,&out)&&out.started&&out.holding); /* Held key retries. */
    in.request=PE_SHIELD_REQUEST_RELEASE;CHECK(pe_shield_action_step(&a,in,&out)&&out.ended);
    CHECK(pe_shield_action_step(&a,in,&out)&&!out.ended&&!out.cancel_buff);
    /* Wait for both source EndAction requests and external pause; mount once. */
    in=input();in.running_actions=BIT(12)|BIT(58);a=(PeShieldAction){0};
    CHECK(pe_shield_action_step(&a,in,&out)&&out.admitted&&!out.started&&!out.holding&&a.phase==PE_SHIELD_ACTION_WAITING);
    in.running_actions=BIT(58);CHECK(pe_shield_action_step(&a,in,&out)&&!out.started&&!out.admitted&&!out.end_actions);
    in.running_actions=0;in.action_paused=true;CHECK(pe_shield_action_step(&a,in,&out)&&!out.started);
    in.action_paused=false;in.request=PE_SHIELD_REQUEST_NONE;CHECK(pe_shield_action_step(&a,in,&out)&&out.started&&out.mount_buff);
    /* EquipmentHold owns movement style even if the distinct mask bit is false. */
    in.running_actions=BIT(13);CHECK(pe_shield_action_step(&a,in,&out)&&out.holding&&!out.set_sword_style);
    in.request=PE_SHIELD_REQUEST_RELEASE;CHECK(pe_shield_action_step(&a,in,&out)&&out.ended&&!out.restore_base_style);
    in=input();in.hide_equipment_inactive=true;CHECK(pe_shield_action_step(&a,in,&out)&&out.set_shield_visible&&out.shield_visible);
    in.request=PE_SHIELD_REQUEST_RELEASE;CHECK(pe_shield_action_step(&a,in,&out)&&out.set_shield_visible&&!out.shield_visible);
    /* Host capability is an explicit extra gate, not a source stamina rule. */
    for(unsigned reason=0;reason<5;reason++){
        in=input();a=(PeShieldAction){0};CHECK(pe_shield_action_step(&a,in,&out)&&out.holding);
        if(reason==0)in.host_capable=false;
        if(reason==1)in.animator_present=false;
        if(reason==2)in.shield_instance_id=0;
        if(reason==3)in.shield_instance_id=92;
        if(reason==4)in.cancel=true;
        CHECK(pe_shield_action_step(&a,in,&out)&&out.ended&&out.cancel_buff&&!out.holding&&!a.instance_id);
        if(reason==3)CHECK(pe_shield_action_step(&a,in,&out)&&out.started&&a.instance_id==92);
    }
    in=input();in.host_capable=false;a=(PeShieldAction){0};CHECK(pe_shield_action_step(&a,in,&out)&&out.denied&&!out.admitted);
    /* Cancel a waiting relation before it mounts: resumes the paused owner. */
    in=input();in.running_actions=BIT(58);CHECK(pe_shield_action_step(&a,in,&out));in.cancel=true;
    CHECK(pe_shield_action_step(&a,in,&out)&&out.ended&&out.continue_actions==BIT(40)&&!out.mount_buff);
    in=input();in.source_masks=BIT(56);atomic(&a,in);in=input();in.running_actions=BIT(63);atomic(&a,in);
    in=input();in.request=(PeShieldActionRequest)99;atomic(&a,in);in=input();a.phase=PE_SHIELD_ACTION_HOLDING;atomic(&a,in);
    a=(PeShieldAction){.instance_id=91};atomic(&a,in);a=(PeShieldAction){.phase=(PeShieldActionPhase)99,.instance_id=91};atomic(&a,in);
    puts("shield action: source56-mask row, waiting relations, idempotent hold, cancellation and capability boundary checked");
    return failures;
}
