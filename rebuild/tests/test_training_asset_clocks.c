/* CPU-only regression of the actual tutorial loader policy. Link with
 * function/data sections and --gc-sections: no window, raylib or host runs. */
#include "../src/pe_training_view.c"
typedef struct {size_t count;PeActorClip clips[33];} ClockFixture;
size_t pe_actor_scene_view_clip_count(const PeActorSceneView *v){
    return v&&v->manifest?((const ClockFixture*)v->manifest)->count:0;
}
const PeActorClip *pe_actor_scene_view_clip_at(const PeActorSceneView *v,size_t index){
    const ClockFixture *f=v?(const ClockFixture*)v->manifest:NULL;
    return f&&index<f->count?&f->clips[index]:NULL;
}
static int errors;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"training asset clocks:%d %s\n",__LINE__,#c);errors++;}}while(0)
int main(void){
    ClockFixture body={.count=20},tool={0};
    for(size_t i=0;i<20;i++){
        snprintf(body.clips[i].name,sizeof body.clips[i].name,"Required%zu",i);
        body.clips[i].fps=30;body.clips[i].frames=31;body.clips[i].duration=1;
        body.clips[i].loop=true;body.clips[i].terminal_at_duration=true;
    }
    tool=body;PeActorSceneView a={.manifest=(PeActorSceneManifest*)&body},b={.manifest=(PeActorSceneManifest*)&tool};
    CHECK(matching_clocks(&a,&b,false)&&matching_clocks(&a,&b,true));
    body.count=21;body.clips[20]=body.clips[0];snprintf(body.clips[20].name,sizeof body.clips[20].name,"RunSwordAttack");
    CHECK(matching_clocks(&a,&b,true));CHECK(!matching_clocks(&a,&b,false));
    ClockFixture good=tool;
    for(unsigned field=0;field<7;field++){
        tool=good;
        if(field==0)tool.clips[19].name[0]='X';
        if(field==1)tool.clips[19].fps=60;
        if(field==2)tool.clips[19].frames=32;
        if(field==3)tool.clips[19].duration=.9;
        if(field==4)tool.clips[19].loop=false;
        if(field==5)tool.clips[19].terminal_at_duration=false;
        if(field==6)tool.count=19;
        CHECK(!matching_clocks(&a,&b,true));
    }
    tool=good;PeActorClip swapped=tool.clips[0];tool.clips[0]=tool.clips[1];tool.clips[1]=swapped;
    CHECK(!matching_clocks(&a,&b,true));
    tool=good;body.clips[20].name[0]='X';CHECK(!matching_clocks(&a,&b,true));
    body.clips[20].name[0]='R';body.count=22;body.clips[21]=body.clips[20];CHECK(!matching_clocks(&a,&b,true));
    body.count=21;tool=body;CHECK(matching_clocks(&a,&b,false)&&matching_clocks(&a,&b,true));
    tool.clips[20].duration=.9;CHECK(!matching_clocks(&a,&b,true));
    static const char *sword[]={"AttackIdle","SwordWF","SwordRF","SwordSprint","SwordJump","SwordFall","SwordLand",
        "SwordRunJump","SwordRunFall","SwordRunLand","SwordWB","SwordRB"};
    tool=body;body.count=33;
    for(size_t i=21;i<33;i++){body.clips[i]=body.clips[0];snprintf(body.clips[i].name,sizeof body.clips[i].name,"%s",sword[i-21]);}
    CHECK(matching_clocks(&a,&b,true)&&!matching_clocks(&a,&b,false));
    tool.count=20;CHECK(matching_clocks(&a,&b,true));
    body.count=31;CHECK(matching_clocks(&a,&b,true));
    body.count=32;CHECK(!matching_clocks(&a,&b,true));
    body.count=33;body.clips[32].name[0]='X';CHECK(!matching_clocks(&a,&b,true));
    body.clips[32].name[0]='S';tool.clips[19].fps=60;CHECK(!matching_clocks(&a,&b,true));
    tool.clips[19].fps=30;
    body.count=20;tool.count=21;CHECK(!matching_clocks(&a,&b,true));
    body.count=tool.count=3;tool=body;CHECK(matching_clocks(&a,&b,false));
    body.count=4;body.clips[3]=body.clips[20];CHECK(!matching_clocks(&a,&b,false));
    body.count=tool.count=0;CHECK(!matching_clocks(&a,&b,true));
    CHECK(!matching_clocks(NULL,NULL,true));return errors?1:0;
}
