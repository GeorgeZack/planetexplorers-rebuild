#include "pe_harvest.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"harvest:%d %s\n",__LINE__,#c);failures++;}}while(0)
static int total(const PeInventory *bag){int n=0;for(int i=0;i<pe_inventory_capacity(bag);i++)n+=bag->slots[i].count;return n;}
static void empty(PeInventory *bag){memset(bag,0,sizeof *bag);bag->next_instance_id=1;}
static uint32_t step(uint32_t state){if(!state)state=0x9e3779b9u;state^=state<<13;state^=state>>17;state^=state<<5;return state;}
int pe_test_harvest(void){
    failures=0;PeInventory *bag=calloc(1,sizeof *bag),*before=calloc(1,sizeof *before);CHECK(bag&&before);if(!bag||!before){free(bag);free(before);return failures;}
    const PeHarvestSource *tree=pe_harvest_source(48);CHECK(tree&&tree->duration==.48571f&&tree->self_count==16&&tree->fixed_count==0);
    CHECK(pe_harvest_source(15)&&pe_harvest_source(37)&&pe_harvest_source(40)&&pe_harvest_source(146)&&!pe_harvest_source(11));
    float hp=-1;CHECK(pe_harvest_damage(48,1,1,255,100,&hp)&&fabsf(hp-206.429f)<.001f);
    CHECK(pe_harvest_damage(48,2,3,255,100,&hp)&&fabsf(hp-(255-48.571f/6))<.001f);
    CHECK(pe_harvest_damage(15,1,1,255,1,&hp)&&hp==0);CHECK(pe_harvest_damage(48,1,1,0,100,&hp)&&hp==0);
    hp=99;CHECK(!pe_harvest_damage(48,0,1,255,1,&hp)&&hp==99);CHECK(!pe_harvest_damage(48,1,NAN,255,1,&hp)&&hp==99);
    CHECK(!pe_harvest_damage(48,1,1,256,1,&hp)&&hp==99);CHECK(!pe_harvest_damage(48,1,1,255,-1,&hp)&&hp==99);
    CHECK(!pe_harvest_damage(11,1,1,255,1,&hp)&&hp==99);CHECK(!pe_harvest_damage(48,1,1,255,INFINITY,&hp)&&hp==99);
    /* Unit source tree:16 primary trials and ceil(16*1.2)=20 extra trials.
     * Gate draw is retained despite19.2 guaranteeing admission. */
    empty(bag);uint32_t rng=123,expected=rng;for(int i=0;i<37;i++)expected=step(expected);
    CHECK(pe_harvest_grant(bag,&rng,48,1,1,0,false));CHECK(pe_inventory_count(bag,279)==16);
    CHECK(pe_inventory_count(bag,979)+pe_inventory_count(bag,1017)==20&&total(bag)==36&&rng==expected);
    /* Nonintegral base floors primary but ceils optional loop, matching the
     * distinct int and float source loop bounds, not rounded whole yield. */
    empty(bag);rng=123;CHECK(pe_harvest_grant(bag,&rng,48,1,1,.01f,false));CHECK(pe_inventory_count(bag,279)==16&&total(bag)==36);
    empty(bag);rng=123;CHECK(pe_harvest_grant(bag,&rng,48,1,1.1f,0,false));CHECK(pe_inventory_count(bag,279)==17&&total(bag)==39);
    empty(bag);rng=123;CHECK(pe_harvest_grant(bag,&rng,48,2,1,0,false));CHECK(pe_inventory_count(bag,279)==64&&total(bag)==141);
    /* Fixed herb count ignores scale and bonus; primary fallback and optional
     * seed/nut outcomes all occur across deterministic independent streams. */
    unsigned seeds=0,fallbacks=0,nuts=0;for(uint32_t seed=1;seed<=600;seed++){
        empty(bag);rng=seed;CHECK(pe_harvest_grant(bag,&rng,15,3,2,5,false));CHECK(pe_inventory_count(bag,238)==2);
        int optional=pe_inventory_count(bag,943);CHECK(optional<=1&&total(bag)==2+optional);seeds+=(unsigned)optional;
        empty(bag);rng=seed;CHECK(pe_harvest_grant(bag,&rng,37,1,1,0,false));
        int count=total(bag);CHECK(count>=3&&count<=4);fallbacks+=(unsigned)pe_inventory_count(bag,1583);nuts+=(unsigned)pe_inventory_count(bag,388);seeds+=(unsigned)pe_inventory_count(bag,940);
        empty(bag);rng=seed;CHECK(pe_harvest_grant(bag,&rng,40,1,1,0,false));CHECK(total(bag)>=4&&total(bag)<=5);seeds+=(unsigned)pe_inventory_count(bag,944);
    }CHECK(seeds>0&&fallbacks>0&&nuts>0);
    /* Failed late optional grant rolls back primary additions and RNG as well.
     *31 full tool slots leave one new wood stack; optional species cannot fit. */
    empty(bag);CHECK(pe_inventory_add(bag,12,31));*before=*bag;rng=123;
    CHECK(!pe_harvest_grant(bag,&rng,48,1,1,0,false)&&rng==123&&!memcmp(bag,before,sizeof *bag));
    CHECK(pe_inventory_count(bag,279)==0);
    /* A spare fourth slot makes the identical failed roll stream retryable. */
    CHECK(pe_inventory_remove(bag,12,2));rng=123;CHECK(pe_harvest_grant(bag,&rng,48,1,1,0,false)&&total(bag)==65);
    empty(bag);*before=*bag;rng=0;
#define BAD(id,w,h,b) CHECK(!pe_harvest_grant(bag,&rng,id,w,h,b,false)&&rng==0&&!memcmp(bag,before,sizeof *bag))
    BAD(99999,1,1,0);BAD(48,0,1,0);BAD(48,1,INFINITY,0);BAD(48,1,1,-1);BAD(48,1,1,NAN);BAD(48,64,64,0);BAD(48,1,1,INFINITY);
#undef BAD
    CHECK(!pe_harvest_grant(NULL,&rng,48,1,1,0,false));CHECK(!pe_harvest_grant(bag,NULL,48,1,1,0,false));
    /* Original seeds belong to the Items category while primary herbs belong
     * to Resources. A late seed roll must preserve the already-planned herb
     * grant and RNG when only Items is full; Resources space is insufficient. */
    uint32_t seed_with_drop=0;
    for(uint32_t seed=1;seed<=600&&!seed_with_drop;seed++){
        empty(bag);rng=seed;CHECK(pe_harvest_grant(bag,&rng,15,1,1,0,false));
        if(pe_inventory_count(bag,943))seed_with_drop=seed;
    }
    CHECK(seed_with_drop);empty(bag);bag->categorized=true;
    CHECK(pe_item_def(940)->tab==0&&pe_item_def(238)->tab==2);
    CHECK(pe_inventory_add(bag,940,pe_item_def(940)->stack_limit*PE_INVENTORY_CATEGORY_SLOTS));
    *before=*bag;rng=seed_with_drop;
    CHECK(!pe_harvest_grant(bag,&rng,15,1,1,0,false)&&rng==seed_with_drop&&!memcmp(bag,before,sizeof *bag));
    CHECK(pe_inventory_count(bag,238)==0&&pe_inventory_count(bag,943)==0);
    CHECK(pe_inventory_remove(bag,940,pe_item_def(940)->stack_limit));
    CHECK(pe_harvest_grant(bag,&rng,15,1,1,0,false)&&pe_inventory_count(bag,238)==2&&pe_inventory_count(bag,943)==1);
    /* Unsupported skill system does not accidentally select rare branch. Empty
     * rare metadata consumes no draw and changes no base optional production. */
    empty(bag);empty(before);rng=123;uint32_t second=123;CHECK(pe_harvest_grant(bag,&rng,48,1,1,0,false));CHECK(pe_harvest_grant(before,&second,48,1,1,0,true));CHECK(rng==second&&!memcmp(bag,before,sizeof *bag));
    free(bag);free(before);return failures;
}
