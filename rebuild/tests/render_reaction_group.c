#define _POSIX_C_SOURCE 200809L
#include "pe_actor_scene_view.h"
#include "pe_sandbox_actor.h"
#include "pe_hitbox.h"
#include <assert.h>
#include <math.h>
#include <rlgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Fixture-only explicit selection. No action owner, damage, save or input. */
static void path(char out[4096],const char *root,const char *folder,const char *file){
    int n=snprintf(out,4096,"%s/%s/%s",root,folder,file);assert(n>0&&n<4096);
}
static void capsule(PeHitboxCapsule c){
    assert(isfinite(c.a.x)&&isfinite(c.a.y)&&isfinite(c.a.z)&&
           isfinite(c.b.x)&&isfinite(c.b.y)&&isfinite(c.b.z)&&isfinite(c.radius)&&c.radius>0);
    DrawCapsuleWires((Vector3){c.a.x,c.a.y,c.a.z},(Vector3){c.b.x,c.b.y,c.b.z},c.radius,4,6,(Color){255,200,40,180});
}
static void recovery_sheet(PeSandboxActor *host,const char *out,bool walk){
    PeGame *game=calloc(1,sizeof *game);assert(game);
    game->hp=100;game->grounded=true;game->equipped_instance_id=1;
    game->inventory.slots[0]=(PeSlot){.item_id=33,.instance_id=1,.count=1};
    pe_sandbox_actor_reset_motion(host,game);pe_sandbox_actor_facing_mode(host,true);assert(pe_sandbox_actor_face_yaw(host,0));
    PeCombat combat={.active=true,.gender=PE_COMBAT_FEMALE,.view=PE_COMBAT_THIRD_PERSON,.clip_time=.69,.event_index=4};
    assert(pe_sandbox_actor_set_melee(host,1,true,&combat));
    combat.active=false;combat.clip_time=.71;combat.event_index=5;
    assert(pe_sandbox_actor_set_melee(host,1,true,&combat));
    RenderTexture2D tiles[4]={0};
    for(unsigned column=0;column<4;column++){
        if(column){if(walk)game->player.z+=.075f;pe_sandbox_actor_update(host,.05f,game);}
        assert(pe_sandbox_actor_set_melee(host,1,true,&combat));
        PeSandboxBodyPose pose;assert(pe_sandbox_actor_body_pose(host,game,&pose));
        bool attack=false;for(size_t b=0;b<pose.blend_count;b++)if(!strcmp(pose.blend[b].clip,"Attack6")&&pose.blend[b].weight>0)attack=true;
        assert(attack==(column<3));
        tiles[column]=LoadRenderTexture(300,600);assert(tiles[column].id);
        BeginTextureMode(tiles[column]);ClearBackground((Color){65,84,102,255});
        Camera3D camera={.position={1.8f,1.6f,3.7f},.target={0,.9f,0},.up={0,1,0},.fovy=38,.projection=CAMERA_PERSPECTIVE};
        BeginMode3D(camera);Matrix projection=rlGetMatrixProjection();projection.m0=-projection.m0;rlSetMatrixProjection(projection);rlSetCullFace(RL_CULL_FACE_FRONT);
        pe_sandbox_actor_draw(host,game);assert(pe_sandbox_actor_ok(host));EndMode3D();rlSetCullFace(RL_CULL_FACE_BACK);
        DrawText(TextFormat("EndAction + %ums",column*50),8,8,20,WHITE);EndTextureMode();
    }
    for(unsigned pass=0;pass<2;pass++){
        BeginDrawing();ClearBackground((Color){24,31,40,255});
        DrawText(walk?"Native Knife recovery to walking":"Native Knife recovery to idle",20,12,26,WHITE);
        DrawText("Fixture-only inactive action; 150ms pose blend, no damage or movement request.",20,45,18,LIGHTGRAY);
        for(unsigned column=0;column<4;column++)DrawTextureRec(tiles[column].texture,(Rectangle){0,0,300,-600},(Vector2){column*300.f,80},WHITE);
        DrawRectangle(1180,740,16,16,(Color){walk?201:200,77,153,255});EndDrawing();
    }
    Image image=LoadImageFromScreen();assert(image.data);
    Color token=GetImageColor(image,1184,744);assert(token.r==(walk?201:200)&&token.g==77&&token.b==153);
    char filename[4096];snprintf(filename,sizeof filename,"%s/knife-recovery-%s.png",out,walk?"walking":"idle");
    assert(!FileExists(filename)&&ExportImage(image,filename));UnloadImage(image);
    for(unsigned column=0;column<4;column++)UnloadRenderTexture(tiles[column]);
    pe_sandbox_actor_set_action(host,NULL);free(game);
}
int main(int argc,char **argv){
    const char *display=getenv("DISPLAY"),*owned=getenv("PE_UI_PRIVATE_DISPLAY");
    if((argc!=3&&argc!=4)||(argc==4&&strcmp(argv[3],"knife"))||!display||!owned||strcmp(display,owned)||getenv("WAYLAND_DISPLAY")||display[0]!=':'||atoi(display+1)<100||atoi(display+1)>999)return 2;
    bool knife=argc==4;
    char filename[4096],error[256];
    InitWindow(1200,760,"Staged reaction animation fixture");assert(IsWindowReady());
    PeSandboxActor *host=pe_sandbox_actor_load(argv[2],error,sizeof error);
    if(!host){fprintf(stderr,"Sandbox actor: %s\n",error);return 1;}
    assert(pe_sandbox_actor_reaction_ready(host));
    path(filename,argv[2],"player-reaction-defence","player-defence.peac");
    PeActor *defence=pe_actor_load(filename,error,sizeof error);assert(defence);
    assert(pe_sandbox_actor_reaction_defence_ready(host,defence));
    path(filename,argv[2],"player-reaction-defence","player-defence.pehb");
    PeHitboxes *boxes=pe_hitboxes_load(filename,defence,error,sizeof error);assert(boxes&&pe_hitboxes_count(boxes)==16);
    recovery_sheet(host,argv[1],false);recovery_sheet(host,argv[1],true);
    pe_sandbox_actor_unload(host);
    puts("Sandbox optional reaction group: real GPU load and matching defence PASS");
    const char *folders[]={"player-reaction","player-reaction-wood-sword-drawn","player-reaction-wood-sword-stowed","player-reaction-wood-shield"};
    if(knife){folders[1]="player-reaction-knife-drawn";folders[2]="player-reaction-knife-stowed";}
    PeActorSceneView views[4]={0};
    for(unsigned i=0;i<4;i++){
        path(filename,argv[2],folders[i],"recording.peas");
        if(!pe_actor_scene_view_load(&views[i],filename,error,sizeof error)){fprintf(stderr,"%s: %s\n",filename,error);return 1;}
        assert(pe_actor_scene_view_clip_count(&views[i])==25);
    }
    PeHitboxes *blade=NULL;
    if(knife){
        path(filename,argv[2],folders[1],"knife-drawn.pehb");
        blade=pe_hitboxes_load(filename,views[1].parts[0].actor,error,sizeof error);
        assert(blade&&pe_hitboxes_count(blade)==1);
    }
    snprintf(filename,sizeof filename,"%s/poses.csv",argv[1]);FILE *log=fopen(filename,"wx");assert(log);
    fputs("clip,duration,phase,seconds,capsules\n",log);
    RenderTexture2D tile=LoadRenderTexture(300,320);assert(tile.id);
    const double phases[]={0,.25,.5,1};
    for(size_t clip=0;clip<25;clip++){
        const PeActorClip *clock=pe_actor_scene_view_clip_at(&views[0],clip);
        const PeActorClip *dc=pe_actor_clip(defence,clip);
        assert(clock&&dc&&!strcmp(clock->name,dc->name)&&clock->duration==dc->duration&&clock->frames==dc->frames&&clock->loop==dc->loop);
        for(unsigned v=1;v<4;v++){
            const PeActorClip *other=pe_actor_scene_view_clip_at(&views[v],clip);
            assert(other&&!strcmp(clock->name,other->name)&&clock->duration==other->duration&&clock->fps==other->fps&&clock->frames==other->frames&&clock->loop==other->loop&&clock->terminal_at_duration==other->terminal_at_duration);
        }
        /* Buffer swap can leave the prior back buffer visible to readback. */
        for(unsigned pass=0;pass<2;pass++){
        BeginDrawing();ClearBackground((Color){24,31,40,255});
        DrawText(TextFormat("%02u  %s   %.6fs",(unsigned)clip,clock->name,clock->duration),20,12,24,WHITE);
        DrawText(knife?"Knife33, fixed root. Top: drawn knife + cyan blade capsule. Bottom: stowed knife + defence wires.":"Isolated leaves, fixed root. Top: drawn sword + shield. Bottom: stowed sword + defence wires.",20,44,16,LIGHTGRAY);
        for(unsigned row=0;row<2;row++)for(unsigned col=0;col<4;col++){
            double seconds=phases[col]*clock->duration;
            BeginTextureMode(tile);ClearBackground((Color){65,84,102,255});
            Camera3D camera={.position={row?3.7f:1.8f,1.7f,row?1.7f:3.7f},.target={0,.85f,0},.up={0,1,0},.fovy=38,.projection=CAMERA_PERSPECTIVE};
            BeginMode3D(camera);
            Matrix projection=rlGetMatrixProjection();projection.m0=-projection.m0;rlSetMatrixProjection(projection);rlSetCullFace(RL_CULL_FACE_FRONT);
            assert(pe_actor_scene_view_draw_clip(&views[0],clip,seconds,(Vector3){0},0,1,WHITE));
            assert(pe_actor_scene_view_draw_clip(&views[row?2:1],clip,seconds,(Vector3){0},0,1,WHITE));
            if(!row&&!knife)assert(pe_actor_scene_view_draw_clip(&views[3],clip,seconds,(Vector3){0},0,1,WHITE));
            rlSetCullFace(RL_CULL_FACE_BACK);
            if(!row&&blade){
                PeHitboxCapsule c;
                assert(pe_hitbox_pose(blade,views[1].parts[0].actor,0,clip,seconds,(PeVec3){0},0,1,&c));
                assert(isfinite(c.radius)&&c.radius>0);
                DrawCapsuleWires((Vector3){c.a.x,c.a.y,c.a.z},(Vector3){c.b.x,c.b.y,c.b.z},c.radius,4,6,SKYBLUE);
            }
            unsigned count=0;
            for(size_t b=0;b<pe_hitboxes_count(boxes);b++){
                const PeHitboxDefinition *d=pe_hitboxes_get(boxes,b);
                if(d->kind==PE_HITBOX_DISABLED)continue;
                PeHitboxCapsule c;assert(pe_hitbox_pose(boxes,defence,b,clip,seconds,(PeVec3){0},0,1,&c));
                if(row)capsule(c);
                count++;
            }
            assert(count==16);EndMode3D();
            DrawText(TextFormat("t=%.4f  (%u%%)",seconds,(unsigned)(phases[col]*100)),8,8,16,WHITE);
            EndTextureMode();
            DrawTextureRec(tile.texture,(Rectangle){0,0,300,-320},(Vector2){col*300.f,80+row*320.f},WHITE);
            if(pass&&!row)fprintf(log,"%s,%.9g,%.2f,%.9g,%u\n",clock->name,clock->duration,phases[col],seconds,count);
        }
        DrawRectangle(1180,740,16,16,(Color){(unsigned char)(clip+1),77,153,255});
        EndDrawing();
        }
        Image image=LoadImageFromScreen();assert(image.data);
        Color token=GetImageColor(image,1184,744);
        assert(token.r==clip+1&&token.g==77&&token.b==153);
        snprintf(filename,sizeof filename,"%s/%02u-%s.png",argv[1],(unsigned)clip,clock->name);
        assert(!FileExists(filename)&&ExportImage(image,filename));UnloadImage(image);
        printf("Rendered %s at four source-clock phases, both attachments and16 defence capsules\n",clock->name);
    }
    assert(fclose(log)==0);UnloadRenderTexture(tile);
    for(unsigned i=0;i<4;i++)pe_actor_scene_view_unload(&views[i]);
    pe_hitboxes_free(blade);pe_hitboxes_free(boxes);pe_actor_free(defence);CloseWindow();
    puts(knife?"Knife33 reaction fixture:25 clips /200 panels; blade geometry only, no live admission PASS":"Reaction fixture:25 clips /200 panels; no live action admission PASS");return 0;
}
