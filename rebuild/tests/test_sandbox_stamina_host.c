/* CPU regression of actual host health helpers + real fixed-step gameplay.
 * Section garbage collection discards the graphical entry point; no window,
 * input injection, reference assets or original saves are used. */
#include "../src/pe_sandbox_view.c"
#include <unistd.h>
static unsigned failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"sandbox stamina host:%d: %s\n",__LINE__,#c);failures++;}}while(0)

static bool fresh(PeGame *g){
    pe_game_destroy(g);
    if(!pe_game_init_mode(g,944,PE_GAME_ADVENTURE))return false;
    g->hp=100;g->stamina=0;g->comfort=100;
    return true;
}
static void fixed_step_and_active_zero(PeGame *g){
    CHECK(fresh(g));
    /* No pre-mount fallback to the old continuous Adventure recovery. */
    for(unsigned i=0;i<120;i++)pe_game_step(g,(PeInput){0},PE_FIXED_DT);
    CHECK(g->stamina==0&&!g->stamina_recovery.initialized&&!g->passive_recovery.initialized);
    PeAttributeClock clock={.seconds=100};
    CHECK(sandbox_health_frame(&clock,g,0,true,false));
    PePassiveRecovery hp_clock=g->passive_recovery;
    PeStaminaRecovery stamina_clock=g->stamina_recovery;
    PeAttributeClock display_clock=clock;double world=g->game_seconds;
    for(unsigned i=0;i<120;i++)pe_game_step(g,(PeInput){0},PE_FIXED_DT);
    CHECK(g->game_seconds>world&&g->stamina==0&&g->comfort==100);
    CHECK(!memcmp(&display_clock,&clock,sizeof clock));
    CHECK(!memcmp(&hp_clock,&g->passive_recovery,sizeof hp_clock));
    CHECK(!memcmp(&stamina_clock,&g->stamina_recovery,sizeof stamina_clock));
    /* The real action input refreshes last consumption even at zero energy.
     * It does not need an observable reduction to retain the source lock. */
    pe_game_step(g,(PeInput){.stamina_action=true},PE_FIXED_DT);
    CHECK(g->stamina==0&&g->stamina_recovery.last_consumption==100);
    for(unsigned i=0;i<100;i++){
        CHECK(sandbox_health_frame(&clock,g,1.0/60,true,false));
        pe_game_step(g,(PeInput){.stamina_action=true},PE_FIXED_DT);
        CHECK(g->stamina==0&&g->stamina_recovery.last_consumption==g->passive_recovery.last_time);
    }
    CHECK(g->comfort<100&&g->stamina_recovery.stamina_ticks>0);
    float stamped=g->stamina_recovery.last_consumption;
    /* A positive general tool cooldown uses the same real game-step hook. */
    CHECK(sandbox_health_frame(&clock,g,.0625,true,false));
    g->tool_cooldown=.01f;pe_game_step(g,(PeInput){0},PE_FIXED_DT);
    CHECK(g->tool_cooldown==0&&g->stamina==0&&g->stamina_recovery.last_consumption>stamped);
    /* Releasing the action does not itself replenish; the later pulse does. */
    stamped=g->stamina_recovery.last_consumption;
    for(unsigned i=0;i<150;i++){
        CHECK(sandbox_health_frame(&clock,g,1.0/60,true,false));
        float before=g->stamina;pe_game_step(g,(PeInput){0},PE_FIXED_DT);
        CHECK(g->stamina==before&&g->stamina_recovery.last_consumption==stamped);
    }
    CHECK(g->stamina>0);
}
static void loaded_order_pause(PeGame *g){
    CHECK(fresh(g));g->comfort=.1f;
    PeGame *loaded=calloc(1,sizeof *loaded);CHECK(loaded);if(!loaded)return;
    char path[]="build/stamina-host-save-XXXXXX",error[256];int fd=mkstemp(path);CHECK(fd>=0);
    if(fd<0){free(loaded);return;}close(fd);
    PeAttributeClock clock={.seconds=100};
    CHECK(sandbox_health_frame(&clock,g,0,true,false));
    pe_game_step(g,(PeInput){.stamina_action=true},PE_FIXED_DT);
    CHECK(g->stamina_recovery.last_consumption==100);
    CHECK(pe_game_save(g,path,error,sizeof error));
    CHECK(pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->comfort==.1f&&loaded->stamina==0&&loaded->passive_recovery.loaded);
    PeStaminaRecovery zero={0};CHECK(!memcmp(&loaded->stamina_recovery,&zero,sizeof zero));
    CHECK(sandbox_health_frame(&clock,loaded,0,true,false));
    CHECK(!loaded->passive_recovery.mounted&&loaded->stamina_recovery.summed_delay==0);
    for(unsigned i=0;i<4;i++)CHECK(sandbox_health_frame(&clock,loaded,.125,true,false));
    CHECK(loaded->passive_recovery.mounted&&loaded->stamina==0);
    while(clock.frame<8)CHECK(sandbox_health_frame(&clock,loaded,0,true,false));
    /* Initial recovery sees old summed delay0 and the reset transient last-use
     * stamp, despite low comfort and the consumed-before-save source object.
     * Dirty6 then observes negative RAW comfort before attribute8 clamps it. */
    CHECK(loaded->stamina==40&&loaded->comfort==0&&loaded->stamina_recovery.summed_delay>2);
    CHECK(loaded->stamina_recovery.last_consumption==0);
    PePassiveRecovery hp_clock=loaded->passive_recovery;
    PeStaminaRecovery stamina_clock=loaded->stamina_recovery;
    double seconds=clock.seconds;uint64_t frame=clock.frame;
    float hp=loaded->hp,energy=loaded->stamina,comfort=loaded->comfort;
    for(unsigned i=0;i<16;i++)CHECK(sandbox_health_frame(&clock,loaded,.25,true,true));
    for(unsigned i=0;i<16;i++)CHECK(sandbox_health_frame(&clock,loaded,.25,false,false));
    CHECK(clock.seconds==seconds&&clock.frame==frame+32);
    CHECK(loaded->hp==hp&&loaded->stamina==energy&&loaded->comfort==comfort);
    CHECK(!memcmp(&hp_clock,&loaded->passive_recovery,sizeof hp_clock));
    CHECK(!memcmp(&stamina_clock,&loaded->stamina_recovery,sizeof stamina_clock));
    CHECK(sandbox_health_frame(&clock,loaded,.125,true,false));
    CHECK(clock.seconds==seconds+.125&&loaded->stamina==energy);
    unlink(path);pe_game_destroy(loaded);free(loaded);
}
static void staged_rollback(PeGame *g){
    CHECK(fresh(g));PeAttributeClock clock={.seconds=100};
    CHECK(sandbox_health_frame(&clock,g,0,true,false));
    clock.frame=7; /* Next frame is due: HP would increase before stamina fails. */
    PeAttributeClock old_clock=clock;PePassiveRecovery old_hp=g->passive_recovery;
    PeStaminaRecovery old_stamina=g->stamina_recovery;float hp=g->hp,energy=g->stamina;
    g->comfort=NAN;
    CHECK(!sandbox_health_frame(&clock,g,.1,true,false));
    CHECK(!memcmp(&clock,&old_clock,sizeof clock)&&!memcmp(&old_hp,&g->passive_recovery,sizeof old_hp));
    CHECK(!memcmp(&old_stamina,&g->stamina_recovery,sizeof old_stamina)&&g->hp==hp&&g->stamina==energy&&isnan(g->comfort));
    g->comfort=100;g->stamina_recovery.summed_delay=NAN;old_stamina=g->stamina_recovery;
    CHECK(!sandbox_health_frame(&clock,g,.1,true,false));
    CHECK(!memcmp(&clock,&old_clock,sizeof clock)&&!memcmp(&old_hp,&g->passive_recovery,sizeof old_hp));
    CHECK(!memcmp(&old_stamina,&g->stamina_recovery,sizeof old_stamina)&&g->hp==hp&&g->stamina==energy&&g->comfort==100);
    g->stamina_recovery.summed_delay=0;old_stamina=g->stamina_recovery;
    CHECK(!sandbox_health_frame(&clock,g,NAN,true,false));
    CHECK(!memcmp(&clock,&old_clock,sizeof clock)&&!memcmp(&old_stamina,&g->stamina_recovery,sizeof old_stamina));
    CHECK(sandbox_health_frame(&clock,g,.1,true,false));
    CHECK(g->hp==hp+5&&g->stamina==40); /* A rejected frame did not consume ticks. */
}
int main(void){
    PeGame *g=calloc(1,sizeof *g);CHECK(g);if(!g)return 1;
    fixed_step_and_active_zero(g);loaded_order_pause(g);staged_rollback(g);
    pe_game_destroy(g);free(g);
    if(failures)return 1;
    puts("sandbox stamina host: fixed-step isolation, zero-energy action locks, source load/order, pause/focus and staged rollback passed");
    return 0;
}
