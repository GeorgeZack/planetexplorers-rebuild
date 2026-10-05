#include "pe_hare_attack.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL hare attack %s:%d: %s\n",__FILE__,__LINE__,#x);failures++;}}while(0)
static void selection_tests(void){
    PeHareAttack a;pe_hare_attack_init(&a);
    CHECK(pe_hare_attack_candidates(&a,0,true,31)==0);
    CHECK(pe_hare_attack_candidates(&a,1,true,31)==0);
    CHECK(pe_hare_attack_candidates(&a,nextafter(1,INFINITY),true,31)==31);
    CHECK(pe_hare_attack_candidates(&a,2,false,31)==0);
    CHECK(pe_hare_attack_candidates(&a,2,true,32)==0);
    CHECK(pe_hare_attack_candidates(&a,NAN,true,31)==0);
    CHECK(pe_hare_attack_candidates(&a,INFINITY,true,31)==0);
    CHECK(pe_hare_attack_candidates(&a,-1,true,31)==0);
    PeHareAttack before=a;
    CHECK(!pe_hare_attack_begin(&a,2,true,20,2)&&!memcmp(&a,&before,sizeof a));
    CHECK(pe_hare_attack_begin(&a,2,true,20,1)&&a.attack==4);
    CHECK(pe_hare_attack_candidates(&a,100,true,31)==0);
    pe_hare_attack_end(&a);
    CHECK(pe_hare_attack_candidates(&a,3,true,31)==0); /* Strict behavior cooldown. */
    CHECK(pe_hare_attack_candidates(&a,3.01,true,31)==15);
    CHECK(pe_hare_attack_candidates(&a,2+pe_hare_attack_def(4)->skill_cooldown,true,31)==31);
    CHECK(pe_hare_attack_begin(&a,4,true,20,0)&&a.attack==2);
    pe_hare_attack_end(&a);
    CHECK(pe_hare_attack_candidates(&a,5.1,true,31)==27);
    CHECK(pe_hare_attack_candidates(&a,5.5,true,31)==31);
    a.shared_ready=6;
    CHECK(pe_hare_attack_candidates(&a,5.9,true,31)==0);
    CHECK(pe_hare_attack_candidates(&a,6,true,31)==31);
    for(unsigned i=0;i<5;i++){
        pe_hare_attack_init(&a);CHECK(pe_hare_attack_begin(&a,2,true,31,i)&&a.attack==i);
        CHECK(pe_hare_attack_def(i)->skill_id>0);
    }
    CHECK(pe_hare_attack_def(5)==NULL);
    CHECK(pe_hare_attack_bone(PE_HARE_RIGHT_FOREARM)->palette_index==35);
    CHECK(pe_hare_attack_bone(PE_HARE_LEFT_FOREARM)->palette_index==13);
    CHECK(pe_hare_attack_bone(0)==NULL&&pe_hare_attack_bone(3)==NULL);
}
static void standard_windows(void){
    for(unsigned i=0;i<5;i++)if(i!=2){
        PeHareAttack a;PeHareAttackEvents e;pe_hare_attack_init(&a);
        CHECK(pe_hare_attack_begin(&a,2,true,31,i));
        const PeHareAttackWindow *w=&pe_hare_attack_def(i)->windows[0];
        CHECK(!pe_hare_attack_record_contact(&a,0,0,1));
        CHECK(pe_hare_attack_update(&a,nextafter(w->start,0),&e)&&e.open_windows==0);
        CHECK(pe_hare_attack_update(&a,w->start,&e)&&e.opened_windows==1&&e.active_bones==w->bones);
        CHECK(pe_hare_attack_record_contact(&a,0,0,1));
        CHECK(!pe_hare_attack_record_contact(&a,0,0,1)); /* Both forearms share guide target list. */
        CHECK(!pe_hare_attack_record_contact(&a,0,1,1));
        CHECK(!pe_hare_attack_record_contact(&a,1,0,1));
        CHECK(!pe_hare_attack_record_contact(&a,0,0,0));
        CHECK(pe_hare_attack_update(&a,w->end,&e)&&e.open_windows==1&&e.opened_windows==0);
        CHECK(pe_hare_attack_record_contact(&a,0,0,2));
        CHECK(pe_hare_attack_update(&a,nextafter(w->end,INFINITY),&e)&&e.closed_windows==1&&!e.monster_end_windows);
        CHECK(!pe_hare_attack_record_contact(&a,0,0,3));
        CHECK(pe_hare_attack_update(&a,2,&e)&&a.active&&!e.open_windows); /* Host owns state exit. */
        pe_hare_attack_end(&a);CHECK(!a.active&&!pe_hare_attack_update(&a,3,&e));
    }
}
static void extended_windows(void){
    PeHareAttack a;PeHareAttackEvents e;pe_hare_attack_init(&a);
    CHECK(pe_hare_attack_begin(&a,2,true,31,2));
    const PeHareAttackDef *d=pe_hare_attack_def(2);
    CHECK(pe_hare_attack_update(&a,d->windows[0].start,&e)&&e.open_windows==1&&e.active_bones==1);
    CHECK(pe_hare_attack_record_contact(&a,0,0,42));
    CHECK(pe_hare_attack_update(&a,d->windows[0].end,&e)&&!e.open_windows&&e.closed_windows==1&&e.monster_end_windows==1);
    CHECK(pe_hare_attack_update(&a,d->windows[0].end,&e)&&!e.monster_end_windows);
    CHECK(pe_hare_attack_update(&a,d->windows[1].start,&e)&&e.open_windows==2&&e.active_bones==2);
    CHECK(!pe_hare_attack_record_contact(&a,1,0,42)); /* Animation window is not a skill guide. */
    CHECK(pe_hare_attack_record_contact(&a,1,1,42));
    CHECK(!pe_hare_attack_record_contact(&a,1,1,42));
    CHECK(pe_hare_attack_update(&a,d->windows[1].end,&e)&&!e.open_windows&&e.closed_windows==2&&e.monster_end_windows==2);
    pe_hare_attack_end(&a);CHECK(pe_hare_attack_begin(&a,4,true,31,2));
    CHECK(pe_hare_attack_update(&a,1,&e)&&!e.open_windows&&!e.opened_windows&&e.monster_end_windows==3);
    CHECK(pe_hare_attack_update(&a,1,&e)&&!e.monster_end_windows);
}
static void invalid_and_capacity(void){
    PeHareAttack a;PeHareAttackEvents e;pe_hare_attack_init(&a);CHECK(pe_hare_attack_begin(&a,2,true,31,0));
    CHECK(pe_hare_attack_update(&a,.5,&e));
    for(uint64_t i=1;i<=PE_HARE_ATTACK_TARGET_CAPACITY;i++)CHECK(pe_hare_attack_record_contact(&a,0,0,i));
    CHECK(!pe_hare_attack_record_contact(&a,0,0,99));
    PeHareAttack before=a;PeHareAttackEvents saved=e;
    const double bad[]={NAN,INFINITY,-1,.49};
    for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++){
        CHECK(!pe_hare_attack_update(&a,bad[i],&e));
        CHECK(!memcmp(&a,&before,sizeof a)&&!memcmp(&e,&saved,sizeof e));
    }
    CHECK(!pe_hare_attack_update(&a,.6,NULL)&&!memcmp(&a,&before,sizeof a));
    CHECK(!pe_hare_attack_begin(&a,3,true,31,0)&&!memcmp(&a,&before,sizeof a));
    pe_hare_attack_end(&a);CHECK(pe_hare_attack_begin(&a,4,true,31,0));
    CHECK(pe_hare_attack_update(&a,.5,&e)&&pe_hare_attack_record_contact(&a,0,0,1));
    CHECK(!pe_hare_attack_record_contact(NULL,0,0,1));
}
int pe_test_hare_attack(void){failures=0;selection_tests();standard_windows();extended_windows();invalid_and_capacity();return failures;}
