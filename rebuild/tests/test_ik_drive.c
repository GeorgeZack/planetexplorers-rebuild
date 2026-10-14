#include "pe_ik_drive.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
static unsigned failures;
#define CHECK(v) do{if(!(v)){fprintf(stderr,"ik_drive:%d %s\n",__LINE__,#v);failures++;}}while(0)
static const PeFbbikRig *rig;
static unsigned char file[65536];static size_t used;
static void put(const void *p,size_t n){memcpy(file+used,p,n);used+=n;}
static void put_u32(uint32_t v){unsigned char b[4]={(unsigned char)v,(unsigned char)(v>>8),(unsigned char)(v>>16),(unsigned char)(v>>24)};put(b,4);}
static void put_f32(float f){uint32_t v;memcpy(&v,&f,4);put_u32(v);}
static bool save(const char *path){FILE *f=fopen(path,"wb");if(!f)return false;bool ok=fwrite(file,1,used,f)==used;return fclose(f)==0&&ok;}
static const char *root="build/test-ik-drive",*empty="build/test-ik-drive/none";
static const char *pose_path="build/test-ik-drive/player-ik-pose/player-ik-pose.peik";
static const char *driver_path="build/test-ik-drive/player-ik-drivers/player-ik-drivers.peid";
static const char *actor_path="build/test-ik-drive/actor.peac";
enum {SPINE3=10,L_UPPER_ARM=18,L_HAND=24};
/* The prefab rest pose has straight limbs, which the solver always bends a little. Rest therefore
 * holds one solve of it and Straight the unsolved rest pose. */
static void build_pose(void){
    PeFbbikTransform rest[PE_FBBIK_MAX_BONES]={0};PeFbbik state;PeFbbikInput in;PeFbbikOutput settled;
    CHECK(pe_fbbik_female_rest(rest)&&pe_fbbik_init(&state,rig,rest)&&pe_fbbik_input(&state,rest,&in)&&pe_fbbik_solve(&state,&in,&settled));
    used=0;put("PEIK",4);put_u32(1);put_u32(rig->bone_count);put_u32(2);
    for(unsigned b=0;b<((rig->bone_count+3u)&~3u);b++){unsigned char p=b<rig->bone_count?(unsigned char)rig->parent[b]:0;put(&p,1);}
    for(int clip=0;clip<2;clip++){
        char name[32]={0};strcpy(name,clip?"Straight":"Rest");put(name,32);put_f32(2);put_u32(2);put_f32(.5f);put_u32(1);
        const PeFbbikTransform *pose=clip?rest:settled.local;
        for(int f=0;f<2;f++)for(unsigned b=0;b<rig->bone_count;b++){
            float v[7]={pose[b].position.x,pose[b].position.y,pose[b].position.z,pose[b].rotation.x,pose[b].rotation.y,pose[b].rotation.z,pose[b].rotation.w};
            for(int j=0;j<7;j++)put_f32(v[j]);
        }
    }
    CHECK(save(pose_path));
}
/* Two joints, three vertices and one clip, as in the actor suite. */
static PeActor *build_actor(float first_x){
    unsigned char *b=file;memset(b,0,1024);memcpy(b,"PEAC",4);used=4;put_u32(1);put_u32(3);put_u32(3);put_u32(2);put_u32(1);
    for(int k=0;k<3;k++){put_f32(k?1:first_x);put_f32((float)k);put_f32(0);put_f32(1);put_f32(1);put_f32(0);put_f32(0);put_f32(0);
        put_f32(.25f);put_f32(.75f);put_f32(0);put_f32(0);unsigned char joints[8]={0,0,1,0,0,0,0,0};put(joints,8);}
    for(uint32_t k=0;k<3;k++)put_u32(k);
    const float unit[12]={1,0,0,0,0,1,0,0,0,0,1,0};
    for(int k=0;k<2;k++)for(int j=0;j<12;j++)put_f32(unit[j]);
    char name[32]={0};strcpy(name,"Rest");put(name,32);put_f32(2);put_u32(2);put_f32(.5f);put_u32(1);
    for(int k=0;k<4;k++)for(int j=0;j<12;j++)put_f32(unit[j]);
    CHECK(save(actor_path));char error[128];PeActor *a=pe_actor_load(actor_path,error,sizeof error);CHECK(a);return a;
}
static size_t entry_at;
static void build_drivers(uint64_t skeleton,unsigned char second){
    used=0;put("PEID",4);put_u32(1);put_u32(rig->bone_count);
    for(unsigned b=0;b<rig->bone_count;b++)put_u32(pe_fbbik_female_bone_id(b));
    put_u32(2);entry_at=used;
    for(uint32_t part=0;part<2;part++){
        char group[48]={0};strcpy(group,"player-test");put(group,48);put_u32(part);put_u32((uint32_t)skeleton);put_u32((uint32_t)(skeleton>>32));put_u32(2);
        unsigned char drivers[4]={L_HAND,part?second:SPINE3,0,0};put(drivers,4);
    }
}
static void refused(void){
    char error[128]="";CHECK(save(driver_path));PeIkDrive *d=pe_ik_drive_load(root,error,sizeof error);CHECK(!d&&error[0]);pe_ik_drive_free(d);
}
static bool same(const float a[12],const float b[12],float tolerance){for(int k=0;k<12;k++)if(fabsf(a[k]-b[k])>tolerance)return false;return true;}
int pe_test_ik_drive(void){
    failures=0;rig=pe_fbbik_female_rig();
    mkdir(root,0777);mkdir("build/test-ik-drive/player-ik-pose",0777);mkdir("build/test-ik-drive/player-ik-drivers",0777);
    remove(pose_path);remove(driver_path);
    char error[128]="x";CHECK(!pe_ik_drive_present(root)&&!pe_ik_drive_present(empty)&&!pe_ik_drive_present(NULL));
    CHECK(!pe_ik_drive_load(root,error,sizeof error)&&error[0]);
    build_pose();CHECK(pe_ik_drive_present(root));
    /* A pose without its driver table is a partial installation. */
    CHECK(!pe_ik_drive_load(root,error,sizeof error)&&error[0]);
    PeActor *actor=build_actor(1),*other=build_actor(2);if(!actor||!other){pe_actor_free(actor);pe_actor_free(other);return 1;}
    uint64_t skeleton=pe_actor_skeleton_hash(actor);
    build_drivers(skeleton,SPINE3);CHECK(save(driver_path));
    PeIkDrive *d=pe_ik_drive_load(root,error,sizeof error);CHECK(d&&!error[0]);
    if(d){
        const unsigned char *drivers=pe_ik_drive_drivers(d,"player-test",1,actor);
        CHECK(drivers&&drivers[0]==L_HAND&&drivers[1]==SPINE3);
        CHECK(!pe_ik_drive_drivers(d,"player-test",2,actor)&&!pe_ik_drive_drivers(d,"player-other",0,actor)&&!pe_ik_drive_drivers(d,"player-test",0,other));
        CHECK(!pe_ik_drive_drivers(NULL,"player-test",0,actor)&&!pe_ik_drive_drivers(d,NULL,0,actor)&&!pe_ik_drive_drivers(d,"player-test",0,NULL));
        static const float identity[12]={1,0,0,0,0,1,0,0,0,0,1,0};
        PeIkDriveFrame frame,kept;PeIkDriveSample rest={"Rest",.1,1},mix[3]={{"Rest",0,1},{"Straight",.2,9},{"Absent",0,0}};
        /* An already solved pose is within reach, so the solve leaves it nearly alone. */
        CHECK(pe_ik_drive_solve(d,&rest,1,true,&frame));
        float worst=0;for(unsigned b=0;b<rig->bone_count;b++)for(int k=0;k<12;k++)worst=fmaxf(worst,fabsf(frame.delta[b][k]-identity[k]));
        if(getenv("PE_TEST_VERBOSE"))fprintf(stderr,"ik_drive settled residual %g\n",worst);
        CHECK(worst<1e-4f);
        /* Mostly the straight pose: the limbs bend and every motion stays rigid. */
        CHECK(pe_ik_drive_solve(d,mix,3,true,&frame));unsigned moved=0;
        for(unsigned b=0;b<rig->bone_count;b++){
            if(!same(frame.delta[b],identity,1e-4f))moved++;
            for(int i=0;i<3;i++)for(int j=0;j<3;j++){float dot=0;for(int k=0;k<3;k++)dot+=frame.delta[b][i*4+k]*frame.delta[b][j*4+k];CHECK(fabsf(dot-(i==j))<1e-4f);}
        }
        CHECK(moved>3);
        /* With the arm mapping off, the arm keeps its animated place under the solved chest. */
        PeIkDriveFrame held;CHECK(pe_ik_drive_solve(d,mix,2,false,&held));
        CHECK(same(held.delta[L_UPPER_ARM],held.delta[SPINE3],1e-4f)&&same(held.delta[L_HAND],held.delta[SPINE3],1e-4f));
        CHECK(same(held.delta[SPINE3],frame.delta[SPINE3],1e-5f));
        /* Effector offsets: none is the plain solve; a body drop lowers the chest and leaves the planted feet. */
        PeIkDriveFrame plain,dropped;PeVec3 offsets[PE_FBBIK_EFFECTORS]={{0}};
        CHECK(pe_ik_drive_solve(d,&rest,1,true,&plain)&&pe_ik_drive_solve_offsets(d,&rest,1,true,NULL,&dropped)&&!memcmp(&plain,&dropped,sizeof plain));
        CHECK(pe_ik_drive_solve_offsets(d,&rest,1,true,offsets,&dropped)&&!memcmp(&plain,&dropped,sizeof plain));
        offsets[0]=(PeVec3){0,-.1f,0};CHECK(pe_ik_drive_solve_offsets(d,&rest,1,true,offsets,&dropped));
        CHECK(dropped.delta[SPINE3][7]<-.02f&&fabsf(dropped.delta[13][7])<5e-3f&&fabsf(dropped.delta[16][7])<5e-3f);
        offsets[0].y=NAN;memcpy(&kept,&dropped,sizeof kept);CHECK(!pe_ik_drive_solve_offsets(d,&rest,1,true,offsets,&dropped)&&!memcmp(&kept,&dropped,sizeof kept));
        /* The grounder's transform writes: none is the offset solve; a lowered pelvis with the feet
         * requested back up leaves them planted and drops the chest; a foot rotation turns that foot only. */
        PeGrounderPose level={{0,0,0},{{0,0,0,1},{0,0,0,1}}},lowered=level,tilted=level;PeIkDriveFrame grounded;
        offsets[0]=(PeVec3){0,0,0};
        CHECK(pe_ik_drive_solve_grounded(d,&rest,1,true,offsets,&level,&grounded));
        for(unsigned b=0;b<rig->bone_count;b++)CHECK(same(grounded.delta[b],plain.delta[b],1e-5f));
        lowered.pelvis_offset.y=-.1f;offsets[7].y=offsets[8].y=.1f;
        CHECK(pe_ik_drive_solve_grounded(d,&rest,1,true,offsets,&lowered,&grounded));
        CHECK(grounded.delta[SPINE3][7]<-.05f&&fabsf(grounded.delta[13][7])<5e-3f&&fabsf(grounded.delta[16][7])<5e-3f);
        offsets[7].y=offsets[8].y=0;tilted.foot_rotation[0]=(PeFbbikQuat){sinf(.1745329f),0,0,cosf(.1745329f)};   /* 20 degrees about x */
        CHECK(pe_ik_drive_solve_grounded(d,&rest,1,true,offsets,&tilted,&grounded));
        unsigned left_foot=rig->chain_bone[3][2],right_foot=rig->chain_bone[4][2];
        float turn=acosf(fminf(1,(grounded.delta[left_foot][0]+grounded.delta[left_foot][5]+grounded.delta[left_foot][10]-1)/2))*57.29578f;
        CHECK(fabsf(turn-20)<.05f&&same(grounded.delta[right_foot],plain.delta[right_foot],1e-5f)&&same(grounded.delta[SPINE3],plain.delta[SPINE3],1e-5f));
        tilted.pelvis_offset.y=NAN;memcpy(&kept,&grounded,sizeof kept);
        CHECK(!pe_ik_drive_solve_grounded(d,&rest,1,true,offsets,&tilted,&grounded)&&!memcmp(&kept,&grounded,sizeof kept));
        /* Foot effector bones of the blended pose, in model space: left at +x of right for this rig's facing or the reverse, never equal. */
        PeVec3 left={9,9,9},right={9,9,9};CHECK(pe_ik_drive_feet(d,&rest,1,&left,&right));
        CHECK(fabsf(left.x-right.x)>.05f&&fabsf(left.y-right.y)<.02f&&left.y<.3f&&fabsf(left.x+right.x)<.05f);
        PeVec3 untouched=left;mix[2].weight=1;CHECK(!pe_ik_drive_feet(d,mix,3,&left,&right)&&!memcmp(&untouched,&left,sizeof left));mix[2].weight=0;
        CHECK(!pe_ik_drive_feet(NULL,&rest,1,&left,&right)&&!pe_ik_drive_feet(d,&rest,1,NULL,&right)&&!pe_ik_drive_feet(d,&rest,1,&left,NULL));
        /* The draw transform turns model x toward world -z at +90 degrees; the two helpers are inverses. */
        PeVec3 turned=pe_ik_drive_world_vector((PeVec3){1,2,0},90);CHECK(fabsf(turned.x)<1e-6f&&turned.y==2&&fabsf(turned.z+1)<1e-6f);
        PeVec3 round=pe_ik_drive_model_vector(pe_ik_drive_world_vector((PeVec3){.3f,-.2f,.7f},37),37);
        CHECK(fabsf(round.x-.3f)<1e-6f&&round.y==-.2f&&fabsf(round.z-.7f)<1e-6f);
        /* Unknown or invalid samples leave the output alone. */
        memcpy(&kept,&frame,sizeof kept);mix[2].weight=1;CHECK(!pe_ik_drive_solve(d,mix,3,true,&frame));
        mix[2]=(PeIkDriveSample){NULL,0,1};CHECK(!pe_ik_drive_solve(d,mix,3,true,&frame));
        mix[2]=(PeIkDriveSample){"Rest",NAN,1};CHECK(!pe_ik_drive_solve(d,mix,3,true,&frame));
        mix[2]=(PeIkDriveSample){"Rest",0,-1};CHECK(!pe_ik_drive_solve(d,mix,3,true,&frame));
        mix[0].weight=mix[1].weight=0;mix[2].weight=0;CHECK(!pe_ik_drive_solve(d,mix,3,true,&frame));
        CHECK(!pe_ik_drive_solve(d,mix,0,true,&frame)&&!pe_ik_drive_solve(d,NULL,1,true,&frame)&&!pe_ik_drive_solve(NULL,&rest,1,true,&frame)&&!pe_ik_drive_solve(d,&rest,1,true,NULL));
        CHECK(!memcmp(&kept,&frame,sizeof kept));
        pe_ik_drive_free(d);
    }
    /* Malformed tables fail closed. */
    size_t full=used;
    for(size_t length=0;length<full;length+=7){used=length;refused();}
    used=full+4;memset(file+full,0,4);refused();used=full;
    build_drivers(skeleton,(unsigned char)rig->bone_count);refused();                       /* driver outside the rig */
    build_drivers(skeleton,SPINE3);file[4]=2;refused();                                     /* version */
    build_drivers(skeleton,SPINE3);file[12]^=1;refused();                                   /* another rig's bone */
    build_drivers(skeleton,SPINE3);file[entry_at+68+48]=0;refused();                     /* duplicate group and part */
    build_drivers(skeleton,SPINE3);file[entry_at+6]='/';refused();                          /* group name */
    build_drivers(skeleton,SPINE3);file[entry_at+48+12]=0;refused();                        /* no joints */
    build_drivers(skeleton,SPINE3);file[entry_at+48+16+3]=1;refused();                      /* padding */
    pe_actor_free(actor);pe_actor_free(other);pe_ik_drive_free(NULL);
    remove(pose_path);remove(driver_path);remove(actor_path);
    return failures?1:0;
}
