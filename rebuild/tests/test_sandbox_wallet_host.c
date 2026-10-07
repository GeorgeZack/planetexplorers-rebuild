/* Exercise the actual display adapter without opening a graphics context. */
#include "../src/pe_sandbox_view.c"
#include <limits.h>
static unsigned failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"sandbox wallet host:%d: %s\n",__LINE__,#c);failures++;}}while(0)
int main(void){
    PeGame *game=calloc(1,sizeof *game);
    if(!game)return 1;
    CHECK(pe_game_init_mode(game,973,PE_GAME_ADVENTURE));
    game->game_seconds=10.5*PE_DAY_SECONDS;
    PeTownWallet initial[PE_TOWN_WALLET_CAPACITY];memcpy(initial,game->town_wallet,sizeof initial);
    CHECK(sandbox_wallet_frame(game,false,false));
    CHECK(sandbox_wallet_frame(game,true,true));
    CHECK(!memcmp(initial,game->town_wallet,sizeof initial));
    /* A dead player or unopened shop must not freeze NPC wallets. */
    game->hp=0;
    CHECK(sandbox_wallet_frame(game,true,false));
    for(size_t i=0;i<PE_TOWN_WALLET_CAPACITY;i++){
        const PeTownWalletSource *source=pe_town_wallet_source_at(i);
        CHECK(source&&game->town_wallet[i].added&&game->town_wallet[i].balance==initial[i].balance+source->daily_increase);
    }
    PeTownWallet credited[PE_TOWN_WALLET_CAPACITY];memcpy(credited,game->town_wallet,sizeof credited);
    for(unsigned i=0;i<6;i++)pe_game_step(game,(PeInput){0},PE_FIXED_DT);
    CHECK(!memcmp(credited,game->town_wallet,sizeof credited));
    CHECK(sandbox_wallet_frame(game,true,false));
    CHECK(!memcmp(credited,game->town_wallet,sizeof credited));
    /* Skipping over mornings is not a multi-day grant. A morning has to be
     * observed by the wallet update before another afternoon is eligible. */
    game->game_seconds=14.75*PE_DAY_SECONDS;CHECK(sandbox_wallet_frame(game,true,false));
    CHECK(!memcmp(credited,game->town_wallet,sizeof credited));
    game->game_seconds=15.25*PE_DAY_SECONDS;
    CHECK(sandbox_wallet_frame(game,true,true));CHECK(game->town_wallet[0].added);
    CHECK(sandbox_wallet_frame(game,true,false));CHECK(!game->town_wallet[0].added);
    game->game_seconds=15.5*PE_DAY_SECONDS;
    CHECK(sandbox_wallet_frame(game,true,false));
    CHECK(game->town_wallet[0].balance==credited[0].balance+pe_town_wallet_source_at(0)->daily_increase);
    /* Failure must not partially credit earlier vendors. */
    for(size_t i=0;i<PE_TOWN_WALLET_CAPACITY;i++)game->town_wallet[i].added=false;
    game->town_wallet[PE_TOWN_WALLET_CAPACITY-1].balance=INT32_MAX;
    memcpy(credited,game->town_wallet,sizeof credited);
    CHECK(!sandbox_wallet_frame(game,true,false));CHECK(!memcmp(credited,game->town_wallet,sizeof credited));
    CHECK(!sandbox_wallet_frame(NULL,true,false));
    pe_game_destroy(game);free(game);
    printf("Sandbox wallet host: %s (%u failures; no graphics)\n",failures?"FAIL":"PASS",failures);
    return failures?1:0;
}
