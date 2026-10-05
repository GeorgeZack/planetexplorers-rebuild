#ifndef PE_ADVENTURE_TEST_HELPERS_H
#define PE_ADVENTURE_TEST_HELPERS_H
#include "pe_game.h"
#include <stdio.h>
#include <stdlib.h>
/* Existing post-welcome fixtures use the same public ACK transaction as UI. */
static inline bool pe_test_ack_welcome(PeGame *game){
    while(game->welcome_page<PE_ADVENTURE_WELCOME_PAGES)
        if(!pe_game_welcome_advance(game,game->welcome_page))return false;
    return game->welcome_page==PE_ADVENTURE_WELCOME_PAGES;
}
/* Produce older inline-world fixtures. V9 appends medicine/new stock; v8 has
 * its fixed seventeen stocks, never the current catalog capacity. */
static inline bool pe_test_save_pre_welcome(const char *path,unsigned version){
    if(version<4||version>7)return false;
    FILE *file=fopen(path,"rb");if(!file)return false;
    if(fseek(file,0,SEEK_END)){fclose(file);return false;}
    long size=ftell(file);if(size<20||size>800000){fclose(file);return false;}
    rewind(file);unsigned char *data=malloc((size_t)size);if(!data){fclose(file);return false;}
    bool ok=fread(data,1,(size_t)size,file)==(size_t)size;fclose(file);
    if(ok&&data[4]>=6&&data[4]<=9&&data[5]==0&&data[6]==0&&data[7]==0){
        if(data[4]>=9)size-=8+16*(PE_TOWN_STOCK_CAPACITY-PE_TOWN_V8_STOCK_CAPACITY);
        if(data[4]>=8){
            long footer=size-(12+4+16*PE_TOWN_V8_STOCK_CAPACITY);
            if(footer<20||data[footer]!=PE_WORLD_X||data[footer+1]||data[footer+2]||data[footer+3]||
               data[footer+4]!=PE_WORLD_Y||data[footer+5]||data[footer+6]||data[footer+7]||
               (data[footer+8]!=PE_WORLD_Z&&data[footer+8]!=128)||data[footer+9]||data[footer+10]||data[footer+11]){free(data);return false;}
            size=footer;
        }
        if(data[4]>=7&&version<7){
            if(data[size-4]!=PE_WORLD_Z||data[size-3]||data[size-2]||data[size-1]){free(data);return false;}
            size-=4;
        }
        if(version<6)size-=4;
        uint32_t sum=2166136261u;
        for(long i=16;i<size;i++){sum^=data[i];sum*=16777619u;}
        uint32_t fields[]={version,(uint32_t)size-16,sum};
        for(unsigned field=0;field<3;field++)for(unsigned byte=0;byte<4;byte++)
            data[4+field*4+byte]=(unsigned char)(fields[field]>>(byte*8));
        file=fopen(path,"wb");if(!file)ok=false;
        else{ok=fwrite(data,1,(size_t)size,file)==(size_t)size;if(fclose(file))ok=false;}
    }else ok=false;
    free(data);return ok;
}
#endif
