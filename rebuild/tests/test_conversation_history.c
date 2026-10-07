#include "pe_conversation_history.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int pe_test_conversation_history(void){
    PeConversationHistory h={0},before;bool added=false;
    assert(pe_conversation_history_valid(&h));
    assert(pe_conversation_history_add(&h,"Tips","First\npage",&added)&&added&&h.count==1);
    assert(pe_conversation_history_add(&h,"Different speaker","First\npage",&added)&&!added&&h.count==1);
    assert(!strcmp(h.entries[0].speaker,"Tips")&&!strcmp(h.entries[0].content,"First\npage"));
    assert(pe_conversation_history_add(&h,"Tips","Firstpage",NULL)&&h.count==2); /* display normalization is not dedupe */
    for(unsigned i=2;i<22;i++){
        char text[32];snprintf(text,sizeof text,"Page %u",i);
        assert(pe_conversation_history_add(&h,"Naoko",text,&added)&&added);
    }
    assert(h.count==20&&!strcmp(h.entries[0].content,"Page 2")&&!strcmp(h.entries[19].content,"Page 21"));
    assert(pe_conversation_history_add(&h,h.entries[0].speaker,"Aliased speaker",NULL));
    assert(!strcmp(h.entries[19].speaker,"Naoko")&&!strcmp(h.entries[19].content,"Aliased speaker"));
    assert(pe_conversation_history_add(&h,"Tips","First\npage",&added)&&added); /* oldest content was evicted */
    before=h;added=true;
    const char *bad[]={"\xc0\xaf","\xe0\x80\xaf","\xed\xa0\x80","\xf4\x90\x80\x80","\x80","\xe2\x82"};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;i++){
        assert(!pe_conversation_history_add(&h,"Name",bad[i],&added)&&added&&!memcmp(&h,&before,sizeof h));
        assert(!pe_conversation_history_add(&h,bad[i],"Text",&added)&&!memcmp(&h,&before,sizeof h));
    }
    char long_name[PE_CONVERSATION_SPEAKER_CAPACITY+1],long_text[PE_CONVERSATION_CONTENT_CAPACITY+1];
    memset(long_name,'N',sizeof long_name);long_name[sizeof long_name-1]=0;
    memset(long_text,'X',sizeof long_text);long_text[sizeof long_text-1]=0;
    assert(!pe_conversation_history_add(&h,long_name,"Text",&added)&&!memcmp(&h,&before,sizeof h));
    assert(!pe_conversation_history_add(&h,"Name",long_text,&added)&&!memcmp(&h,&before,sizeof h));
    long_name[sizeof long_name-2]=0;long_text[sizeof long_text-2]=0;
    assert(pe_conversation_history_add(&h,long_name,long_text,NULL)&&pe_conversation_history_valid(&h));
    assert(pe_conversation_history_add(&h,"名字","UTF-8: café 🌱",NULL));
    pe_conversation_history_clear(&h);assert(pe_conversation_history_valid(&h)&&!h.count);
    assert(pe_conversation_history_add(&h,"","",NULL)&&h.count==1); /* source string values may be empty */
    assert(pe_conversation_history_add(&h,"Other","",&added)&&!added&&h.count==1);
    h.entries[19].content[4000]='x';before=h;
    assert(!pe_conversation_history_valid(&h)&&!pe_conversation_history_add(&h,"X","Y",NULL)&&!memcmp(&h,&before,sizeof h));
    pe_conversation_history_clear(&h);h.count=21;assert(!pe_conversation_history_valid(&h));
    pe_conversation_history_clear(&h);h.count=2;assert(!pe_conversation_history_valid(&h)); /* duplicate empty content */
    pe_conversation_history_clear(&h);h.count=1;memset(h.entries[0].content,'x',sizeof h.entries[0].content);assert(!pe_conversation_history_valid(&h));
    assert(!pe_conversation_history_valid(NULL)&&!pe_conversation_history_add(NULL,"X","Y",NULL));
    pe_conversation_history_clear(NULL);
    puts("conversation history: source FIFO/content dedupe, UTF-8 bounds, alias and atomic failure checks pass");return 0;
}
#ifdef PE_CONVERSATION_HISTORY_TEST_MAIN
int main(void){return pe_test_conversation_history();}
#endif
