#include "pe_conversation_history.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static bool utf8_string(const char *text,size_t capacity,size_t *length){
    if(!text)return false;
    const unsigned char *p=(const unsigned char *)text;
    size_t i=0;
    while(i<capacity&&p[i]){
        unsigned c=p[i++],need=0;uint32_t code=0,minimum=0;
        if(c<0x80)continue;
        if(c>=0xc2&&c<=0xdf){need=1;code=c&31;minimum=0x80;}
        else if(c>=0xe0&&c<=0xef){need=2;code=c&15;minimum=0x800;}
        else if(c>=0xf0&&c<=0xf4){need=3;code=c&7;minimum=0x10000;}
        else return false;
        if(need>capacity-i)return false;
        while(need--){unsigned next=p[i++];if((next&0xc0)!=0x80)return false;code=(code<<6)|(next&63);}
        if(code<minimum||code>0x10ffff||(code>=0xd800&&code<=0xdfff))return false;
    }
    if(i>=capacity)return false;
    if(length)*length=i;
    return true;
}
static bool canonical(const char *text,size_t capacity){
    size_t n;if(!utf8_string(text,capacity,&n))return false;
    for(size_t i=n+1;i<capacity;i++)if(text[i])return false;
    return true;
}
bool pe_conversation_history_valid(const PeConversationHistory *h){
    if(!h||h->count>PE_CONVERSATION_HISTORY_CAPACITY)return false;
    for(unsigned i=0;i<PE_CONVERSATION_HISTORY_CAPACITY;i++){
        const PeConversationEntry *e=&h->entries[i];
        if(!canonical(e->speaker,sizeof e->speaker)||!canonical(e->content,sizeof e->content))return false;
        if(i>=h->count){if(e->speaker[0]||e->content[0])return false;}
        else for(unsigned j=0;j<i;j++)if(!strcmp(e->content,h->entries[j].content))return false;
    }
    return true;
}
bool pe_conversation_history_add(PeConversationHistory *h,const char *speaker,const char *content,bool *added){
    size_t sn,cn;
    if(!pe_conversation_history_valid(h)||!utf8_string(speaker,PE_CONVERSATION_SPEAKER_CAPACITY,&sn)||
       !utf8_string(content,PE_CONVERSATION_CONTENT_CAPACITY,&cn))return false;
    for(unsigned i=0;i<h->count;i++)if(!strcmp(content,h->entries[i].content)){
        if(added)*added=false;
        return true;
    }
    /* Copy first: the caller may point at an entry which eviction moves. */
    PeConversationEntry entry={0};memcpy(entry.speaker,speaker,sn);memcpy(entry.content,content,cn);
    if(h->count==PE_CONVERSATION_HISTORY_CAPACITY){
        memmove(h->entries,h->entries+1,(PE_CONVERSATION_HISTORY_CAPACITY-1)*sizeof *h->entries);
        h->count--;
    }
    h->entries[h->count++]=entry;
    if(added)*added=true;
    return true;
}
void pe_conversation_history_clear(PeConversationHistory *h){if(h)memset(h,0,sizeof *h);}
