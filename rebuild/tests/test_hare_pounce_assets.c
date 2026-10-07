/* Explicit local asset check: missing staged assets are failures, never skips. */
#include "pe_actor.h"
#include "pe_hitbox.h"
#include "pe_hare_pounce.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"pounce assets:%d %s\n",__LINE__,#x);failures++;}}while(0)
int main(int argc,char**argv){
 if(argc!=4){fprintf(stderr,"usage: test_hare_pounce_assets OLD_PEAC STAGED_PEAC HARE_PEHB\n");return 2;}
 char error[256];PeActor*base=pe_actor_load(argv[1],error,sizeof error),*actor=pe_actor_load(argv[2],error,sizeof error);CHECK(base&&actor);if(!base||!actor){fprintf(stderr,"%s\n",error);pe_actor_free(base);pe_actor_free(actor);return 1;}
 PeActorStats a=pe_actor_stats(base),b=pe_actor_stats(actor);CHECK(a.clips==8&&b.clips==9&&a.vertices==b.vertices&&pe_actor_skeleton_hash(base)==pe_actor_skeleton_hash(actor));
 size_t clip=pe_actor_find_clip(actor,"attack5");CHECK(clip==8);const PeActorClip*d=pe_actor_clip(actor,clip);CHECK(d&&d->frames==31&&!d->loop&&d->duration==0.9666671752929688);
 PeActorVertex*vertices=malloc(b.vertices*sizeof *vertices);CHECK(vertices);
 if(vertices)for(unsigned frame=0;frame<31;frame++)CHECK(pe_actor_pose(actor,clip,frame/30.,vertices,b.vertices));
 for(unsigned c=0;c<8;c++){
  CHECK(!strcmp(pe_actor_clip(base,c)->name,pe_actor_clip(actor,c)->name));
  PeActorVertex first,second;CHECK(pe_actor_pose_vertex(base,c,.3,0,&first)&&pe_actor_pose_vertex(actor,c,.3,0,&second)&&!memcmp(&first,&second,sizeof first));
 }
 PeHitboxes*boxes=pe_hitboxes_load(argv[3],actor,error,sizeof error);CHECK(boxes);unsigned attack=0,groups=0;
 if(boxes){const PeHarePounceDef*s=pe_hare_pounce_def();float times[]={nextafterf(s->window_start,0),s->window_start,s->window_end,nextafterf(s->window_end,INFINITY)};
  for(size_t i=0;i<pe_hitboxes_count(boxes);i++){
   const PeHitboxDefinition*box=pe_hitboxes_get(boxes,i);if(box->kind!=PE_HITBOX_ATTACK||box->joint==PE_HITBOX_DISABLED)continue;attack++;groups|=box->group;
   for(unsigned t=0;t<4;t++){PeHitboxCapsule out;CHECK(pe_hitbox_pose(boxes,actor,i,clip,times[t]*d->duration,(PeVec3){0},0,1,&out));CHECK(isfinite(out.radius)&&out.radius>0);CHECK(pe_hare_pounce_window(times[t])==(t==1||t==2));}
  }CHECK(attack==4&&groups==(PE_HITBOX_RIGHT|PE_HITBOX_LEFT));
 }
 free(vertices);pe_hitboxes_free(boxes);pe_actor_free(base);pe_actor_free(actor);printf("Pounce actor/parser/window/capsule: %d failures\n",failures);return !!failures;
}
