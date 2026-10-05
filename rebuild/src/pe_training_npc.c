#include "pe_training_npc.h"
#include <math.h>

/* npc.training_pos + PrototypeNPC dedicated assets; Plot470 turns Andy275.
 * Source model root capsules are audited in TRAINING_NPCS.md. */
static const PeTrainingNpc roster[PE_TRAINING_NPC_COUNT]={
    {9004,4,"Adisa","Npc/adisa_mostafa.unity3d","Model/Npc/Adisa_Mostafa:Adisa-Mostafa_1","AiPrefab/EntityData/Npc/Npc_stand",{27,1.8f,17.5f},0,.370000005f,1.75f,.875f,true},
    {9005,5,"Moric","Npc/moric_gahn.unity3d","Model/Npc/Moric_Gahn:Moric_Gahn","AiPrefab/EntityData/Npc/Npc_patrol",{32,1.8f,6.5f},0,.370000005f,1.75f,.875f,true},
    {9006,6,"Vera","Npc/vera_novikova.unity3d","Model/Npc/Vera_Novikova:Vera_Novikov","AiPrefab/EntityData/Npc/Npc_campfear",{58,2,12.5f},0,.370000005f,1.75f,.875f,true},
    {9009,9,"Allen","Npc/allen_carter.unity3d","Model/Npc/Allen_Carter:Allen Carter","AiPrefab/EntityData/Npc/Npc_doctor",{10,1.8f,3.5f},0,.370000005f,1.75f,.875f,true},
    {9020,20,"Anjum","Npc/anjum_nahas.unity3d","Model/Npc/Anjum_Nahas:Anjum_Nahas","AiPrefab/EntityData/Npc/Npc_patrol",{2.5f,1.8f,6},0,.370000005f,1.975000024f,.949999988f,true},
    {9031,31,"Viola","Npc/viola_strand.unity3d","Model/Npc/Viola_Strand:Viola_Strand","AiPrefab/EntityData/Npc/Npc_patrol",{-27,10,20.5f},0,.370000005f,1.75f,.875f,true},
    {9041,41,"Andy","Npc/andy.unity3d","Model/Npc/Andy:Andy","AiPrefab/EntityData/Npc/Npc_stand",{32,1.8f,12},275,.370000005f,1.75f,.875f,true},
    {9047,47,"Percy","Npc/percy_malik.unity3d","Model/Npc/Percy_Malik:Percy_Malik","AiPrefab/EntityData/Npc/Npc_com+repair",{27.5f,9.2f,2},0,.370000005f,1.75f,.875f,false},
    {9049,49,"Malus","Npc/malus_tepes.unity3d","Model/Npc/Malus_Tepes:Malus_Tepes","AiPrefab/EntityData/Npc/Npc_stand",{37,2.5f,5.5f},0,.370000005f,1.75f,.875f,false},
};
const PeTrainingNpc*pe_training_npc_at(size_t index){return index<PE_TRAINING_NPC_COUNT?&roster[index]:NULL;}
const PeTrainingNpc*pe_training_npc_find(int id){for(size_t i=0;i<PE_TRAINING_NPC_COUNT;i++)if(roster[i].id==id)return &roster[i];return NULL;}
bool pe_training_npc_ground(const PeTrainingNpc*n,const PeSceneCollision*room,PeVec3*out){
    if(!n||!room||!out||!isfinite(n->position.x)||!isfinite(n->position.y)||!isfinite(n->position.z)||
       !isfinite(n->radius)||!isfinite(n->height)||!isfinite(n->center_y)||n->radius<=0||n->radius>2||n->height<n->radius*2||n->height>5)return false;
    float offset=n->center_y-n->height*.5f;
    for(unsigned i=0;i<=12;i++){
        PeVec3 feet={n->position.x,n->position.y+offset+.25f*(float)i,n->position.z};
        if(pe_scene_capsule_overlap(room,feet,n->radius,n->height))continue;
        PeSceneHit ground=pe_scene_capsule_sweep(room,feet,n->radius,n->height,(PeVec3){0,-5,0});
        if(!ground.hit||ground.normal.y<.65f)continue;
        PeVec3 settled={feet.x,feet.y-fmaxf(0,ground.distance-.003f),feet.z};
        if(pe_scene_capsule_overlap(room,settled,n->radius,n->height))continue;
        settled.y-=offset;*out=settled;return true;
    }
    return false;
}
