/* CPU-only checks of the actual sandbox health adapters. Including the host
 * with section garbage collection does not create a window or call raylib.
 * Contact-count inputs represent committed events; capsule/dedupe/late rollback
 * are separately covered by test_hare_damage and test_hunt_combat. */
#include "../src/pe_sandbox_view.c"
#include <unistd.h>
static unsigned failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"sandbox health host:%d: %s\n",__LINE__,#c);failures++;}}while(0)

static void fresh_health(PeGame *g){
    g->mode=PE_GAME_ADVENTURE;g->hp=100;g->max_hp=500;
    g->passive_recovery=(PePassiveRecovery){0};g->stamina_recovery=(PeStaminaRecovery){0};g->revival=(PeRevival){0};pe_hunger_fresh(&g->hunger);
}
static void frame_gate(PeGame *g){
    fresh_health(g);PeAttributeClock clock={.seconds=10};
    for(unsigned i=0;i<7;i++)CHECK(sandbox_health_frame(&clock,g,1.0/60,true,false));
    CHECK(clock.frame==7&&clock.attribute_frame==0&&g->hp==100&&g->passive_recovery.evaluations==0);
    CHECK(sandbox_health_frame(&clock,g,1.0/60,true,false));
    CHECK(clock.frame==8&&clock.attribute_frame==8&&g->hp==105&&g->passive_recovery.evaluations==1);
    /* Actual fixed simulation calls cannot advance the display/buff clocks. */
    PePassiveRecovery recovery=g->passive_recovery;PeAttributeClock before=clock;double world=g->game_seconds;
    for(unsigned i=0;i<6;i++)pe_game_step(g,(PeInput){0},PE_FIXED_DT);
    CHECK(g->game_seconds>world&&g->hp==105);
    CHECK(!memcmp(&recovery,&g->passive_recovery,sizeof recovery)&&!memcmp(&before,&clock,sizeof clock));
    /* The global updater reschedules from the observed frame, not modulo8. */
    clock.frame=24;CHECK(sandbox_health_frame(&clock,g,0,true,false));
    CHECK(clock.frame==25&&clock.attribute_frame==25);
    for(unsigned i=0;i<7;i++)CHECK(sandbox_health_frame(&clock,g,0,true,false));
    CHECK(clock.frame==32&&clock.attribute_frame==25);
    CHECK(sandbox_health_frame(&clock,g,0,true,false));CHECK(clock.frame==33&&clock.attribute_frame==33);
    /* Native pause/focus freezes scaled time and entity state, but display
     * frame progression remains external to the entity. */
    recovery=g->passive_recovery;double seconds=clock.seconds;uint64_t frame=clock.frame;
    for(unsigned i=0;i<8;i++)CHECK(sandbox_health_frame(&clock,g,.25,true,true));
    for(unsigned i=0;i<8;i++)CHECK(sandbox_health_frame(&clock,g,.25,false,false));
    CHECK(clock.seconds==seconds&&clock.frame==frame+16&&g->hp==105);
    CHECK(!memcmp(&recovery,&g->passive_recovery,sizeof recovery));
    CHECK(sandbox_health_frame(&clock,g,.125,true,false));CHECK(clock.seconds==seconds+.125);
    recovery=g->passive_recovery;before=clock;float hp=g->hp;
    CHECK(!sandbox_health_frame(&clock,g,NAN,true,false));
    CHECK(!memcmp(&recovery,&g->passive_recovery,sizeof recovery)&&!memcmp(&before,&clock,sizeof clock)&&g->hp==hp);
    g->max_hp=NAN;CHECK(!sandbox_health_frame(&clock,g,.1,true,false));g->max_hp=500;
    CHECK(!memcmp(&recovery,&g->passive_recovery,sizeof recovery)&&!memcmp(&before,&clock,sizeof clock)&&g->hp==hp);
}
static void contacts(PeGame *g){
    fresh_health(g);PeAttributeClock clock={.seconds=20};
    CHECK(sandbox_health_frame(&clock,g,0,true,false));
    float damage=-1;CHECK(pe_combat_damage(64,0,true,1,false,1,&damage)&&damage==0);
    float hp=g->hp;g->hp=fmaxf(0,g->hp-damage);
    sandbox_health_contacts(g,7,8); /* Admitted hit despite unchanged HP. */
    CHECK(g->hp==hp&&g->passive_recovery.last_hurt==20);
    PePassiveRecovery recovery=g->passive_recovery;
    sandbox_health_contacts(g,8,8); /* Miss/ignored/rolled-back count: no notification. */
    CHECK(!memcmp(&recovery,&g->passive_recovery,sizeof recovery));
    sandbox_health_contacts(g,8,10); /* Multiple contacts in this display frame. */
    CHECK(!memcmp(&recovery,&g->passive_recovery,sizeof recovery)&&clock.seconds==20&&clock.frame==1);
    for(unsigned i=0;i<7;i++)CHECK(sandbox_health_frame(&clock,g,0,true,false));
    CHECK(clock.frame==8&&g->hp==100); /* Zero damage still blocks recovery. */
    CHECK(sandbox_health_frame(&clock,g,.25,true,false));sandbox_health_contacts(g,10,11);
    CHECK(g->passive_recovery.last_hurt==20.25f&&clock.frame==9);
    g->hp=0;sandbox_health_contacts(g,11,12);
    CHECK(!g->passive_recovery.mounted&&g->passive_recovery.evaluations==0);
    for(unsigned i=0;i<40;i++)CHECK(sandbox_health_frame(&clock,g,.25,true,false));
    CHECK(g->hp==0&&!g->passive_recovery.mounted); /* No resurrection. */
    fresh_health(g);g->mode=PE_GAME_LEGACY;recovery=g->passive_recovery;
    CHECK(sandbox_health_frame(&clock,g,.25,true,false));sandbox_health_contacts(g,0,1);
    CHECK(g->hp==100&&!memcmp(&recovery,&g->passive_recovery,sizeof recovery));
}
static unsigned char *read_file(const char *path,size_t *size){
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long n=ftell(f);
    if(n<16||fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char *p=malloc((size_t)n);if(!p){fclose(f);return NULL;}
    bool ok=fread(p,1,(size_t)n,f)==(size_t)n;if(fclose(f))ok=false;
    if(!ok){free(p);return NULL;}*size=(size_t)n;return p;
}
static void save_load(void){
    char path[]="build/health-host-save-XXXXXX",error[256];int fd=mkstemp(path);
    PeGame *g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded),*snapshot=malloc(sizeof *snapshot);
    unsigned char *first=NULL,*second=NULL;size_t n=0,m=0;
    CHECK(fd>=0&&g&&loaded&&snapshot);if(fd<0||!g||!loaded||!snapshot)goto done;close(fd);
    CHECK(pe_game_init_mode(g,943,PE_GAME_ADVENTURE));g->hp=100;
    PeAttributeClock clock={.seconds=100};CHECK(sandbox_health_frame(&clock,g,0,true,false));
    sandbox_health_contacts(g,0,1);CHECK(g->passive_recovery.last_hurt==100);
    CHECK(pe_game_save(g,path,error,sizeof error));first=read_file(path,&n);CHECK(first&&first[4]==PE_SAVE_VERSION);
    /* Transient time/frame/phase is absent from the current V21 payload. */
    g->passive_recovery=(PePassiveRecovery){.loaded=true};
    CHECK(pe_game_save(g,path,error,sizeof error));second=read_file(path,&m);
    CHECK(first&&second&&n==m&&!memcmp(first,second,n));
    PeAttributeClock outside=clock;
    CHECK(pe_game_load(loaded,path,error,sizeof error));
    PePassiveRecovery expected={.loaded=true};
    CHECK(loaded->hp==100&&!memcmp(&loaded->passive_recovery,&expected,sizeof expected));
    CHECK(!memcmp(&clock,&outside,sizeof clock));
    CHECK(sandbox_health_frame(&clock,loaded,0,true,false));
    CHECK(!loaded->passive_recovery.mounted&&loaded->passive_recovery.created==100&&loaded->passive_recovery.last_hurt==0);
    for(unsigned i=0;i<6;i++)CHECK(sandbox_health_frame(&clock,loaded,.0625,true,false));
    CHECK(clock.frame==8&&clock.seconds==100.375&&loaded->hp==100&&!loaded->passive_recovery.mounted);
    CHECK(sandbox_health_frame(&clock,loaded,.0625,true,false));
    CHECK(!loaded->passive_recovery.mounted&&loaded->hp==100);
    CHECK(sandbox_health_frame(&clock,loaded,.0625,true,false));
    CHECK(clock.seconds==100.5&&loaded->passive_recovery.mounted&&loaded->hp==100);
    for(unsigned i=0;i<6;i++)CHECK(sandbox_health_frame(&clock,loaded,0,true,false));
    CHECK(clock.frame==16&&loaded->hp==105); /* No invented four seconds after load. */
    memcpy(snapshot,loaded,sizeof *snapshot);uint64_t world_hash=pe_world_hash(&loaded->world);outside=clock;
    FILE *bad=fopen(path,"wb");CHECK(bad);if(bad){CHECK(fwrite("invalid",1,7,bad)==7);CHECK(fclose(bad)==0);}
    CHECK(!pe_game_load(loaded,path,error,sizeof error));
    CHECK(!memcmp(snapshot,loaded,sizeof *snapshot)&&pe_world_hash(&loaded->world)==world_hash&&!memcmp(&clock,&outside,sizeof clock));
    /* Loading a saved death resets the transient state, never HP. */
    g->hp=0;CHECK(pe_game_save(g,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error));
    for(unsigned i=0;i<16;i++)CHECK(sandbox_health_frame(&clock,loaded,.125,true,false));
    CHECK(loaded->hp==0&&!loaded->passive_recovery.mounted);
done:
    if(fd>=0)unlink(path);
    free(first);free(second);free(snapshot);
    pe_game_destroy(g);pe_game_destroy(loaded);free(g);free(loaded);
}
/* Hunger runs on the same display frame: the drain at mount, then starvation
 * once hunger is at or below 50, with its message, and its stamina pre-adds. */
static void hunger(PeGame *g){
    fresh_health(g);g->hp=500;PeAttributeClock clock={.seconds=30};
    unsigned frames=0;
    while(frames<9&&g->hunger.hunger==300){CHECK(sandbox_health_frame(&clock,g,0,true,false));frames++;}
    CHECK(frames==8&&clock.attribute_frame==clock.frame&&fabsf(g->hunger.hunger-(300-.347f))<1e-3f);
    g->hunger.hunger=g->hunger.raw=40;
    for(unsigned i=0;i<4;i++)CHECK(sandbox_health_frame(&clock,g,.25,true,false));
    CHECK(!g->hunger.starving);CHECK(sandbox_health_frame(&clock,g,.25,true,false));
    CHECK(g->hunger.starving&&!strcmp(g->message,pe_hunger_source_starvation_notice)&&pe_game_stamina_reduce_percent(g)==2);
    /* Pause freezes the condition's clock. */
    float elapsed=g->hunger.end_elapsed;
    for(unsigned i=0;i<8;i++)CHECK(sandbox_health_frame(&clock,g,.25,true,true));
    CHECK(g->hunger.end_elapsed==elapsed);
    /* Ending emits nothing: an unrelated message survives it. */
    snprintf(g->message,sizeof g->message,"sentinel");
    g->hunger.hunger=g->hunger.raw=60;
    for(unsigned i=0;i<2&&g->hunger.starving;i++)CHECK(sandbox_health_frame(&clock,g,.1,true,false));
    CHECK(!g->hunger.starving&&!strcmp(g->message,"sentinel"));
    /* Native adapter: a later start shows the notice again. The original's notice
     * clears its triggers after firing, so it shows once per player component. */
    g->hunger.hunger=g->hunger.raw=40;
    for(unsigned i=0;i<6&&!g->hunger.starving;i++)CHECK(sandbox_health_frame(&clock,g,.25,true,false));
    CHECK(g->hunger.starving&&!strcmp(g->message,pe_hunger_source_starvation_notice));
}
int main(void){
    PeGame *g=calloc(1,sizeof *g);CHECK(g);if(!g)return 1;
    pe_game_init(g,942);frame_gate(g);contacts(g);hunger(g);pe_game_destroy(g);free(g);save_load();
    if(failures)return 1;
    puts("sandbox health host: display cadence, catch-up isolation, admitted-zero hurt, death, pause/focus and current V21 load lifecycle passed");return 0;
}
