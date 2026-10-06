#include "pe_hare_skill.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL hare skill %s:%d: %s\n",__FILE__,__LINE__,#x);failures++;}}while(0)
static void simple_skills(void){
    const int ids[]={30100003,30100004,30100008,30100046,30100047};
    for(unsigned attack=0;attack<5;attack++){
        const PeHareSkillDef*d=pe_hare_skill_def(attack);CHECK(d&&d->skill_id==ids[attack]);
        if(attack==2)continue;
        PeHareSkill s;pe_hare_skill_init(&s);CHECK(pe_hare_skill_begin(&s,attack));
        CHECK(s.active&&s.contact_enabled&&s.guide==0&&s.seconds==0);
        unsigned ticks=0;
        while(s.active&&ticks<100){CHECK(pe_hare_skill_step(&s,0));ticks++;}
        CHECK(ticks==(attack==4?64u:60u));
        CHECK(s.completed&&!s.contact_enabled&&s.phase==PE_HARE_SKILL_DONE);
        CHECK(s.seconds>=d->main[0]&&s.seconds<d->main[0]+PE_HARE_SKILL_DT);
        PeHareSkill before=s;CHECK(pe_hare_skill_step(&s,0)&&!memcmp(&s,&before,sizeof s));
    }
    CHECK(pe_hare_skill_def(5)==NULL);
}
static void separate_guides(void){
    PeHareSkill s;pe_hare_skill_init(&s);CHECK(pe_hare_skill_begin(&s,2));
    CHECK(s.phase==PE_HARE_SKILL_PRE&&!s.contact_enabled&&s.guide==0);
    unsigned phases[PE_HARE_SKILL_DONE+1]={0},windows=0,ticks=0;
    bool last_contact=false;unsigned last_guide=0;
    while(s.active&&ticks<150){
        phases[s.phase]++;CHECK(pe_hare_skill_step(&s,0));ticks++;
        if(s.contact_enabled&&(!last_contact||s.guide!=last_guide))windows++;
        if(s.guide!=last_guide)CHECK(!s.contact_enabled&&s.phase==PE_HARE_SKILL_PRE&&s.phase_seconds==0);
        last_contact=s.contact_enabled;last_guide=s.guide;
    }
    /* Source Single .15 is slightly above 9 double 60Hz ticks. Each separate
     * wait is quantized anew; overshoot is deliberately not carried onward. */
    CHECK(phases[PE_HARE_SKILL_PRE]==37&&phases[PE_HARE_SKILL_MAIN]==25);
    CHECK(phases[PE_HARE_SKILL_ENDING_POST]==31&&ticks==93);
    CHECK(windows==2&&s.guide==1&&s.completed&&!s.contact_enabled);
    const PeHareSkillDef*d=pe_hare_skill_def(2);
    CHECK(d->guide_count==2&&d->pre[0]==(double).36f&&d->main[0]==(double).15f);
    CHECK(d->pre[1]==(double).24f&&d->main[1]==(double).24f&&d->ending_post==(double).51f);
}
static void skips_and_cancel(void){
    PeHareSkill s;pe_hare_skill_init(&s);CHECK(pe_hare_skill_begin(&s,2));
    CHECK(pe_hare_skill_step(&s,PE_HARE_SKILL_SKIP_PRE));
    CHECK(s.contact_enabled&&s.guide==0&&s.phase_seconds==0);
    CHECK(pe_hare_skill_step(&s,PE_HARE_SKILL_SKIP_MAIN|PE_HARE_SKILL_SKIP_POST));
    CHECK(s.guide==1&&s.phase==PE_HARE_SKILL_PRE&&!s.contact_enabled&&!s.skip_flags);
    CHECK(pe_hare_skill_step(&s,7));
    CHECK(s.phase==PE_HARE_SKILL_ENDING_POST&&s.active&&!s.contact_enabled&&s.phase_seconds==0);
    for(unsigned i=0;i<30;i++)CHECK(pe_hare_skill_step(&s,7)&&s.active);
    CHECK(pe_hare_skill_step(&s,7)&&s.completed); /* ending wait is never skippable */
    CHECK(pe_hare_skill_begin(&s,0)&&s.contact_enabled);
    CHECK(pe_hare_skill_step(&s,0));double elapsed=s.seconds;
    pe_hare_skill_cancel(&s);CHECK(!s.active&&!s.completed&&!s.contact_enabled&&s.seconds==elapsed);
    CHECK(pe_hare_skill_begin(&s,2)&&s.seconds==0);
    pe_hare_skill_cancel(&s);CHECK(!s.active&&!s.completed);
}
static void invalid_atomic(void){
    PeHareSkill s={0},before=s;
    CHECK(!pe_hare_skill_begin(&s,0)&&!memcmp(&s,&before,sizeof s));
    CHECK(!pe_hare_skill_step(&s,0)&&!memcmp(&s,&before,sizeof s));
    pe_hare_skill_init(&s);before=s;
    CHECK(!pe_hare_skill_begin(&s,5)&&!memcmp(&s,&before,sizeof s));
    CHECK(pe_hare_skill_begin(&s,2));before=s;
    CHECK(!pe_hare_skill_begin(&s,0)&&!memcmp(&s,&before,sizeof s));
    CHECK(!pe_hare_skill_step(&s,8)&&!memcmp(&s,&before,sizeof s));
    s.seconds=NAN;before=s;CHECK(!pe_hare_skill_step(&s,0)&&!memcmp(&s,&before,sizeof s));
    s.seconds=0;s.guide=2;before=s;CHECK(!pe_hare_skill_step(&s,0)&&!memcmp(&s,&before,sizeof s));
    CHECK(!pe_hare_skill_begin(NULL,0)&&!pe_hare_skill_step(NULL,0));
    pe_hare_skill_init(NULL);pe_hare_skill_cancel(NULL);
}
int pe_test_hare_skill(void){failures=0;simple_skills();separate_guides();skips_and_cancel();invalid_atomic();return failures;}
