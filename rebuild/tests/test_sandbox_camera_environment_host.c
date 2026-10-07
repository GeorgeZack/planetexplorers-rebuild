/* Actual frontend camera dispatch and collision snapshot; no window/GPU. */
#include "../src/pe_sandbox_view.c"
#include <assert.h>
static void update(PeSandboxCollision *c,PeWorld *w,const PeSandboxBox *boxes,size_t n,const PeTreeCapsule *trees,size_t nt){
    char error[256];assert(pe_sandbox_collision_update_with_trees(c,w,NULL,0,boxes,n,trees,nt,error,sizeof error));
}
int main(void){
    PeGame *g=calloc(1,sizeof *g);PeSandboxCollision *c=pe_sandbox_collision_create();assert(g&&c);
    g->world.revision=1;g->mode=PE_GAME_ADVENTURE;g->player=(PeVec3){30,10,30};
    update(c,&g->world,NULL,0,NULL,0);
    PeSandboxCamera camera;PeSandboxCameraRecovery recovery;pe_sandbox_camera_init(&camera);pe_sandbox_camera_recovery_init(&recovery);
    PeSandboxCameraInput in={.dt=1.0f/60,.third_person=true,.input_allowed=true};
    PeVec3 anchor={30,11.58f,30};float open=0;
    assert(pe_sandbox_camera_activity(anchor,sandbox_camera_ray,c,&open)&&fabsf(open-50)<.0001f);
    assert(sandbox_camera_frame(&camera,&recovery,g,c,in,NULL)&&camera.distance==6);
    /* Live tree caps obstruct movement/perception, never camera activity/pose. */
    PeTreeCapsule tree=pe_tree_collision_source((PeVec3){30,9,32});assert(pe_tree_collision_valid(&tree));
    update(c,&g->world,NULL,0,&tree,1);float with_tree=0;
    assert(pe_sandbox_camera_activity(anchor,sandbox_camera_ray,c,&with_tree)&&with_tree==open);
    assert(pe_sandbox_collision_ray_perception(c,anchor,(PeVec3){0,0,1},6).hit);
    float clamp=-1;Camera3D pose=game_camera(g,c,true,false,6,&clamp);assert(clamp<0&&pose.position.z==36);
    /* A close wall affects both actual camera pose and the activity evaluator. */
    PeSandboxBox walls[]={{{25,5,33},{35,20,33.5f}},{{28,5,25},{28.5f,20,32}},{{31.5f,5,25},{32,20,32}}};
    update(c,&g->world,walls,3,&tree,1);float narrow=0;
    assert(pe_sandbox_camera_activity(anchor,sandbox_camera_ray,c,&narrow)&&narrow<open);
    pose=game_camera(g,c,true,false,camera.distance,&clamp);assert(clamp>=0&&clamp<camera.distance&&pose.position.z<33);
    PeSandboxCamera before=camera;PeSandboxCameraRecovery old=recovery;
    for(unsigned i=0;i<6;i++)(void)game_camera(g,c,true,false,camera.distance,NULL);
    assert(!memcmp(&camera,&before,sizeof camera)&&!memcmp(&recovery,&old,sizeof recovery));
    assert(sandbox_camera_feedback_frame(&camera,&recovery,g->mode,in,false,clamp));
    assert(camera.distance==clamp&&camera.velocity==0&&recovery.clamp_pending);
    /* Pause/first person/overview neither consume nor generate clamp feedback. */
    before=camera;old=recovery;PeSandboxCameraInput frozen=in;frozen.paused=true;
    assert(sandbox_camera_frame(&camera,&recovery,g,c,frozen,NULL));
    assert(sandbox_camera_feedback_frame(&camera,&recovery,g->mode,frozen,false,.1f));
    assert(!memcmp(&camera,&before,sizeof camera)&&!memcmp(&recovery,&old,sizeof recovery));
    frozen=in;frozen.third_person=false;
    assert(sandbox_camera_frame(&camera,&recovery,g,c,frozen,NULL));
    assert(sandbox_camera_feedback_frame(&camera,&recovery,g->mode,frozen,false,.1f));
    assert(sandbox_camera_feedback_frame(&camera,&recovery,g->mode,in,true,.1f));
    assert(!memcmp(&camera,&before,sizeof camera)&&!memcmp(&recovery,&old,sizeof recovery));
    /* Clearing geometry does not immediately spring back after the clamp. */
    update(c,&g->world,NULL,0,NULL,0);
    assert(sandbox_camera_frame(&camera,&recovery,g,c,in,NULL)&&recovery.hold_seconds==2&&!recovery.clamp_pending);
    float held=camera.distance;
    for(int i=0;i<30;i++)assert(sandbox_camera_frame(&camera,&recovery,g,c,in,NULL));
    assert(recovery.hold_seconds>1&&camera.distance==held);
    in.wheel_notches=-1;assert(sandbox_camera_frame(&camera,&recovery,g,c,in,NULL));
    assert(recovery.hold_seconds==0&&camera.desired_distance>6&&camera.distance>held);
    /* Invalid backend cannot silently publish open-space recovery. */
    PeSandboxCollision *unready=pe_sandbox_collision_create();assert(unready);
    before=camera;old=recovery;assert(!sandbox_camera_frame(&camera,&recovery,g,unready,in,NULL));
    assert(!memcmp(&camera,&before,sizeof camera)&&!memcmp(&recovery,&old,sizeof recovery));
    pe_sandbox_collision_free(unready);
    /* Legacy still follows the previously verified scalar-only policy. */
    g->mode=PE_GAME_LEGACY;pe_sandbox_camera_init(&camera);before=camera;pe_sandbox_camera_recovery_init(&recovery);
    assert(pe_sandbox_camera_step(&before,in)&&sandbox_camera_frame(&camera,&recovery,g,NULL,in,NULL));
    assert(!memcmp(&camera,&before,sizeof camera)&&!recovery.clamp_pending&&recovery.hold_seconds==0);
    /* Integrated source GeometryClamp and coherent reflected pointer pose. */
    g->mode=PE_GAME_ADVENTURE;pe_sandbox_camera_init(&camera);pe_sandbox_camera_recovery_init(&recovery);
    PeCameraGeometryState geometry={0};Camera3D rendered={0};in=(PeSandboxCameraInput){.dt=1.f/60,.third_person=true,.input_allowed=true};
    update(c,&g->world,walls,3,&tree,1);
    assert(sandbox_camera_pose_frame(g,c,in,false,1280,720,&camera,&recovery,&geometry,&rendered));
    assert(camera.distance<6&&camera.velocity==0&&recovery.clamp_pending&&rendered.position.z<33);
    assert(rendered.position.y>=g->player.y+1.58f); /* no fixed .5m framing rise */
    PeCameraProjection projection={pv(rendered.position),pv(Vector3Subtract(rendered.target,rendered.position)),pv(rendered.up),1280,720,rendered.fovy,PE_CAMERA_RENDER_NEAR};
    PeCameraRay cursor;assert(pe_camera_projection_ray(&projection,1000,230,&cursor));
    PeVec3 point={cursor.origin.x+cursor.direction.x*4,cursor.origin.y+cursor.direction.y*4,cursor.origin.z+cursor.direction.z*4};float px,py;
    assert(pe_camera_projection_screen(&projection,point,&px,&py)&&fabsf(px-1000)<.01f&&fabsf(py-230)<.01f);
    before=camera;old=recovery;PeCameraGeometryState old_geometry=geometry;
    in.paused=true;
    assert(sandbox_camera_pose_frame(g,c,in,false,1280,720,&camera,&recovery,&geometry,&rendered));
    assert(!memcmp(&camera,&before,sizeof camera)&&!memcmp(&recovery,&old,sizeof recovery)&&geometry.rise==old_geometry.rise);
    in.paused=false;in.third_person=false;
    assert(sandbox_camera_pose_frame(g,c,in,false,1280,720,&camera,&recovery,&geometry,&rendered)&&rendered.position.z==g->player.z&&geometry.rise==old_geometry.rise);
    unready=pe_sandbox_collision_create();in.third_person=true;Camera3D old_rendered=rendered;
    assert(!sandbox_camera_pose_frame(g,unready,in,false,1280,720,&camera,&recovery,&geometry,&rendered));
    assert(!memcmp(&rendered,&old_rendered,sizeof rendered)&&!memcmp(&camera,&before,sizeof camera)&&geometry.rise==old_geometry.rise);
    pe_sandbox_collision_free(unready);
    pe_sandbox_collision_free(c);pe_game_destroy(g);free(g);
    puts("Sandbox camera environment: actual host activity, tree exclusion, final-pose feedback, hold/wheel and freeze passed (no graphics)");return 0;
}
