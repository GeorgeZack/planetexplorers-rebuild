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
/* Current category storage projects occupied records into the literal32 prefix. */
static inline int pe_test_save_prefix_index(const PeGame *game,int index){
    if(!game->inventory.categorized)return index;
    int result=0;
    for(int i=0;i<index;i++)if(game->inventory.slots[i].count)result++;
    return result<PE_INVENTORY_SLOTS?result:-1;
}
/* Offset of the retained v2 counter, after variable blocks/trees and v1 voxels.
 * Old-save outfit migration advances only this existing field in the prefix. */
static inline size_t pe_test_save_counter_offset(const PeGame *game){
    return 16u+52u+8u*PE_INVENTORY_SLOTS+12u+4u+16u*(size_t)game->block_count+
        4u+16u*(size_t)game->tree_count+12u+2u*PE_VOXEL_COUNT+16u;
}
/* Produce older inline-world fixtures. V23 appends8 bytes of hunger.
 * V16 appends4 bytes raw comfort.
 * V15 appends4 bytes of shield reference.
 * V14 appends36 bytes of worn instances.
 * V13 appends a four-byte medpack clock.
 * V12 retains the literal v11 payload.
 * V11 appends four-byte hunt credit;
 * V10 appends hunt/deployed instances;
 * V9 appends medicine/new stock; v8 has
 * its fixed seventeen stocks, never the current catalog capacity. */
static inline bool pe_test_save_pre_welcome(const char *path,unsigned version){
    if(version<4||version>7)return false;
    FILE *file=fopen(path,"rb");if(!file)return false;
    if(fseek(file,0,SEEK_END)){fclose(file);return false;}
    long size=ftell(file);if(size<20||size>800000){fclose(file);return false;}
    rewind(file);unsigned char *data=malloc((size_t)size);if(!data){fclose(file);return false;}
    bool ok=fread(data,1,(size_t)size,file)==(size_t)size;fclose(file);
    if(ok&&data[4]>=6&&data[4]<=PE_SAVE_VERSION&&version<=data[4]&&data[5]==0&&data[6]==0&&data[7]==0){
        if(data[4]>=23){if(size<=PE_SAVE_V23_APPEND+20){free(data);return false;}size-=PE_SAVE_V23_APPEND;}
        if(data[4]>=22){if(size<=PE_SAVE_V22_APPEND+20){free(data);return false;}size-=PE_SAVE_V22_APPEND;}
        if(data[4]>=21){if(size<=PE_SAVE_V21_APPEND+20){free(data);return false;}size-=PE_SAVE_V21_APPEND;}
        if(data[4]>=20){if(size<=PE_SAVE_V20_APPEND+20){free(data);return false;}size-=PE_SAVE_V20_APPEND;}
        if(data[4]>=19)size-=PE_SAVE_V19_APPEND;
        if(data[4]>=18)size-=PE_SAVE_V18_APPEND;
        if(data[4]>=17)size-=PE_SAVE_V17_APPEND;
        if(data[4]>=16)size-=PE_SAVE_V16_APPEND;
        if(data[4]>=15)size-=PE_SAVE_V15_APPEND;
        if(data[4]>=14)size-=PE_SAVE_V14_APPEND;
        if(data[4]>=13)size-=PE_SAVE_V13_APPEND;
        if(data[4]>=11)size-=PE_SAVE_V11_APPEND;
        if(data[4]>=10)size-=PE_SAVE_V10_APPEND;
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
