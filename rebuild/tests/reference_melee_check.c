/* Optional installed-data fixture. No graphics, gameplay input or source-file
 * edits. Clock mapping and controlled target transform are explicit arguments;
 * a contact here is NOT an observed original-game hit. See REFERENCE_MELEE.md. */
#include "pe_knife_contact.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {double recording,clip,wall;} Clock;
typedef struct {
    const char *knife,*knife_boxes,*target,*target_boxes,*clock,*target_clip;
    PeVec3 position;float yaw,scale;double target_time;
    PeCombatGender gender;PeCombatView view;bool position_set,easy;
} Options;
static bool number(const char *s,double *value){char *end;errno=0;double x=strtod(s,&end);if(!*s||*end||errno||!isfinite(x))return false;*value=x;return true;}
static void usage(void){fprintf(stderr,"reference_melee_check --knife PEAC --knife-hitboxes PEHB --target PEAC --target-hitboxes PEHB --clock-map PEMC --target-position X Y Z [--target-yaw DEGREES] [--target-scale .8..1.3] [--target-clip NAME] [--target-time SECONDS] [--gender female|male] [--view third|first] [--difficulty normal|easy]\n");}
static bool options(int argc,char **argv,Options *o){
    *o=(Options){.target_clip="idle",.scale=1};
    for(int i=1;i<argc;i++){
        const char *name=argv[i];if(i+1>=argc)return false;const char *value=argv[++i];double x;
        if(!strcmp(name,"--knife"))o->knife=value;else if(!strcmp(name,"--knife-hitboxes"))o->knife_boxes=value;
        else if(!strcmp(name,"--target"))o->target=value;else if(!strcmp(name,"--target-hitboxes"))o->target_boxes=value;
        else if(!strcmp(name,"--clock-map"))o->clock=value;else if(!strcmp(name,"--target-clip"))o->target_clip=value;
        else if(!strcmp(name,"--target-position")){
            double y,z;if(i+2>=argc||!number(value,&x)||!number(argv[++i],&y)||!number(argv[++i],&z)||fabs(x)>10000||fabs(y)>10000||fabs(z)>10000)return false;
            o->position=(PeVec3){(float)x,(float)y,(float)z};o->position_set=true;
        }else if(!strcmp(name,"--target-yaw")){if(!number(value,&x)||fabs(x)>360000)return false;o->yaw=(float)x;}
        else if(!strcmp(name,"--target-scale")){if(!number(value,&x)||x<.8||x>1.3)return false;o->scale=(float)x;}
        else if(!strcmp(name,"--target-time")){if(!number(value,&x)||x<0||x>86400)return false;o->target_time=x;}
        else if(!strcmp(name,"--gender")){if(!strcmp(value,"female"))o->gender=PE_COMBAT_FEMALE;else if(!strcmp(value,"male"))o->gender=PE_COMBAT_MALE;else return false;}
        else if(!strcmp(name,"--view")){if(!strcmp(value,"third"))o->view=PE_COMBAT_THIRD_PERSON;else if(!strcmp(value,"first"))o->view=PE_COMBAT_FIRST_PERSON;else return false;}
        else if(!strcmp(name,"--difficulty")){if(!strcmp(value,"easy"))o->easy=true;else if(strcmp(value,"normal"))return false;}
        else return false;
    }
    return o->knife&&o->knife_boxes&&o->target&&o->target_boxes&&o->clock&&o->position_set;
}
static size_t load_clock(const char *path,Clock map[1024]){
    FILE *f=fopen(path,"rb");if(!f)return 0;
    if(fseek(f,0,SEEK_END)){fclose(f);return 0;}long length=ftell(f);
    if(length<6||length>262144||fseek(f,0,SEEK_SET)){fclose(f);return 0;}
    char *text=malloc((size_t)length+1);if(!text){fclose(f);return 0;}
    bool ok=fread(text,1,(size_t)length,f)==(size_t)length&&fgetc(f)==EOF&&!ferror(f);fclose(f);text[length]=0;
    for(long i=0;i<length;i++)if((unsigned char)text[i]>126||((unsigned char)text[i]<32&&text[i]!='\n'&&text[i]!='\r'))ok=false;
    size_t count=0;bool header=false;char *line=text;
    while(ok&&*line){
        char *next=strchr(line,'\n');if(next)*next++=0;else next=line+strlen(line);
        size_t n=strlen(line);if(n&&line[n-1]=='\r')line[--n]=0;
        if(!header){header=true;if(strcmp(line,"PEMC1"))ok=false;line=next;continue;}
        Clock row;char extra;
        if(count==1024||sscanf(line,"%lf %lf %lf %c",&row.recording,&row.clip,&row.wall,&extra)!=3||
           !isfinite(row.recording)||!isfinite(row.clip)||!isfinite(row.wall)||row.recording<0||row.clip<0||row.wall<0||
           row.recording>86400||row.clip>60||row.wall>60){ok=false;break;}
        if(count&&(row.recording<=map[count-1].recording||row.clip<map[count-1].clip||row.wall<map[count-1].wall)){ok=false;break;}
        if(!count&&(row.clip!=0||row.wall!=0)){ok=false;break;}
        map[count++]=row;line=next;
    }
    free(text);return ok&&count>=3?count:0;
}
static bool unchanged(PeCombat *action,PeKnifeContactAttack *attack,PeKnifeContactTarget *target,PeKnifeContactStatus expected){
    PeCombat old=*action;PeKnifeContactTarget before=*target;PeKnifeContactResult result,saved;memset(&result,0x3c,sizeof result);saved=result;
    return pe_knife_contact_resolve(action,attack,target,&result)==expected&&!memcmp(&old,action,sizeof old)&&!memcmp(&before,target,sizeof before)&&!memcmp(&saved,&result,sizeof result);
}
int main(int argc,char **argv){
    Options o;if(!options(argc,argv,&o)){usage();return 1;}Clock clock[1024];size_t samples=load_clock(o.clock,clock);if(!samples){fprintf(stderr,"Invalid/empty PEMC clock map\n");return 1;}
    char error[256];PeActor *knife=NULL,*hare=NULL;PeHitboxes *weapon_boxes=NULL,*defence=NULL;int status=1;
    knife=pe_actor_load(o.knife,error,sizeof error);if(!knife)goto fail;
    hare=pe_actor_load(o.target,error,sizeof error);if(!hare)goto fail;
    weapon_boxes=pe_hitboxes_load(o.knife_boxes,knife,error,sizeof error);if(!weapon_boxes)goto fail;
    defence=pe_hitboxes_load(o.target_boxes,hare,error,sizeof error);if(!defence)goto fail;
    const PeActorClip *recording=pe_actor_clip(knife,0);size_t target_clip=pe_actor_find_clip(hare,o.target_clip),weapon_index=SIZE_MAX;
    if(pe_actor_stats(knife).clips!=1||!recording||clock[samples-1].recording>recording->duration+.00001||target_clip==PE_ACTOR_BIND_POSE){snprintf(error,sizeof error,"Actor clip or recording clock mismatch");goto fail;}
    for(size_t i=0;i<pe_hitboxes_count(weapon_boxes);i++){const PeHitboxDefinition *d=pe_hitboxes_get(weapon_boxes,i);if(d->kind==PE_HITBOX_ATTACK&&d->joint!=PE_HITBOX_DISABLED){if(weapon_index!=SIZE_MAX){snprintf(error,sizeof error,"Fixture requires exactly one enabled weapon capsule");goto fail;}weapon_index=i;}}
    if(weapon_index==SIZE_MAX){snprintf(error,sizeof error,"No enabled weapon capsule");goto fail;}
    PeCombat action;pe_combat_init(&action);float stamina=100;
    if(!pe_combat_begin(&action,o.gender,o.view,1,&stamina)){snprintf(error,sizeof error,"Cannot start stationary knife action");goto fail;}
    float factor=o.scale*(o.easy?.5f:1);
    PeKnifeContactTarget target={.entity_id=2,.actor=hare,.hitboxes=defence,.clip=target_clip,.seconds=o.target_time,.position=o.position,.yaw=o.yaw,.scale=o.scale,.defense=29*factor,.hp=212*factor,.max_hp=212*factor,.eligible=true};
    PeKnifeContactAttack attack={.entity_id=1,.attack=19,.eligible=true};
    printf("CONTROLLED FIXTURE: no original hit asserted; player root=(0,0,0), yaw=0 scale=1\n");
    printf("Target root=(%.6g,%.6g,%.6g) yaw=%.6g scale=%.6g difficulty=%s HP=%.6g defense=%.6g; pose=%s @%.6gs\n",(double)o.position.x,(double)o.position.y,(double)o.position.z,(double)o.yaw,(double)o.scale,o.easy?"easy":"normal",(double)target.hp,(double)target.defense,o.target_clip,o.target_time);
    printf("Clock: %zu explicit updates, recording %.9g..%.9gs, authored0..%.9gs; activation reconstructed from source events, not observed\n",samples,clock[0].recording,clock[samples-1].recording,clock[samples-1].clip);
    unsigned hits=0,closed=0,miss_checks=0,dedup_checks=0;double prev_clip=0,prev_wall=0;PeVec3 lo={INFINITY,INFINITY,INFINITY},hi={-INFINITY,-INFINITY,-INFINITY};
    for(size_t i=0;i<samples;i++){
        PeCombatAdvance advance=pe_combat_advance(&action,clock[i].clip-prev_clip,clock[i].wall-prev_wall,&stamina);prev_clip=clock[i].clip;prev_wall=clock[i].wall;
        if(advance==PE_COMBAT_INVALID||advance==PE_COMBAT_NEXT_STAGE){snprintf(error,sizeof error,"Unexpected action clock transition");goto fail;}
        if(!pe_hitbox_pose(weapon_boxes,knife,weapon_index,0,clock[i].recording,(PeVec3){0},0,1,&attack.weapon)){snprintf(error,sizeof error,"Recorded knife capsule pose failed");goto fail;}
        PeVec3 points[]={attack.weapon.a,attack.weapon.b};for(unsigned p=0;p<2;p++){
            lo.x=fminf(lo.x,points[p].x);lo.y=fminf(lo.y,points[p].y);lo.z=fminf(lo.z,points[p].z);hi.x=fmaxf(hi.x,points[p].x);hi.y=fmaxf(hi.y,points[p].y);hi.z=fmaxf(hi.z,points[p].z);}
        if(!action.contact_open){PeKnifeContactTarget fresh=target;fresh.entity_id=99;if(!unchanged(&action,&attack,&fresh,PE_KNIFE_CONTACT_IGNORED)){snprintf(error,sizeof error,"Closed-window mutation");goto fail;}closed++;continue;}
        PeKnifeContactTarget far=target;far.entity_id=99;far.position.x+=100;
        if(!unchanged(&action,&attack,&far,PE_KNIFE_CONTACT_MISS)){snprintf(error,sizeof error,"Separated-target miss failed");goto fail;}miss_checks++;
        PeKnifeContactResult result;float before=target.hp;PeKnifeContactStatus contact=pe_knife_contact_resolve(&action,&attack,&target,&result);
        if(contact==PE_KNIFE_CONTACT_INVALID){snprintf(error,sizeof error,"Contact resolver rejected valid fixture");goto fail;}
        if(contact==PE_KNIFE_CONTACT_HIT){
            float expected;if(!pe_combat_damage(19,target.defense,false,0,false,result.contact_scale,&expected)||fabsf(expected-result.damage)>.0001f||fabsf(target.hp-fmaxf(0,before-expected))>.0001f||result.died){snprintf(error,sizeof error,"Unexpected controlled HP result");goto fail;}
            hits++;printf("CONTACT recording=%.9g authored=%.9g part=%s scale=%.6g damage=%.9g HP=%.9g->%.9g\n",clock[i].recording,clock[i].clip,pe_hitboxes_get(defence,result.defence_index)->name,(double)result.contact_scale,(double)result.damage,(double)before,(double)target.hp);
            if(!unchanged(&action,&attack,&target,PE_KNIFE_CONTACT_IGNORED)){snprintf(error,sizeof error,"Duplicate contact changed HP/action");goto fail;}dedup_checks++;
        }
    }
    printf("Knife axis bounds=(%.6g,%.6g,%.6g)..(%.6g,%.6g,%.6g), original radius last=%.6g\n",(double)lo.x,(double)lo.y,(double)lo.z,(double)hi.x,(double)hi.y,(double)hi.z,(double)attack.weapon.radius);
    if(!closed||!miss_checks||hits>1||(hits&&dedup_checks!=1)){snprintf(error,sizeof error,"Insufficient or repeated contact coverage");goto fail;}
    printf("Reference melee: hits=%u closed=%u separated_misses=%u dedupe=%u finalHP=%.9g result=%s\n",hits,closed,miss_checks,dedup_checks,(double)target.hp,hits?"PASS":"NO_CONTACT (HP/dedupe contact checks not reached)");
    status=hits?0:2;goto done;
fail:fprintf(stderr,"Reference melee failed: %s\n",error);
done:pe_hitboxes_free(weapon_boxes);pe_hitboxes_free(defence);pe_actor_free(knife);pe_actor_free(hare);return status;
}
