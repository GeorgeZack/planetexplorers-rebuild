#include "pe_ragdoll_action.h"
#include "../generated/pe_ragdoll_action_source.h"
#include <math.h>

static int index_of(PeRagdollKind kind){
    for(unsigned i=0;i<3;i++)if(pe_ragdoll_source_relations[i].action==(unsigned)kind)return (int)i;
    return -1;
}
static bool finite3(PeVec3 v){return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z);}
static bool valid(const PeRagdollAction *a){
    return a&&index_of(a->kind)>=0&&isfinite(a->remaining);
}
bool pe_ragdoll_action_relation(PeRagdollKind kind,PeRagdollRelation *out){
    int i=index_of(kind);if(i<0||!out)return false;
    const PeRagdollSourceRelation *r=&pe_ragdoll_source_relations[i];
    *out=(PeRagdollRelation){r->required,r->forbidden,r->pause,r->end,r->immediate};return true;
}
bool pe_ragdoll_action_source_admissible(PeRagdollKind kind,uint64_t masks){
    PeRagdollRelation r;
    return pe_ragdoll_action_relation(kind,&r)&&!(masks>>56)&&
           (masks&r.required_masks)==r.required_masks&&!(masks&r.forbidden_masks);
}
static void op(PeRagdollUpdate *u,PeRagdollOpType type,bool value,float delay){
    u->ops[u->op_count++]=(PeRagdollOp){type,value,delay};
}
static void mask(PeRagdollAction *a,PeRagdollUpdate *u,bool value){
    a->mask=value;op(u,PE_RAGDOLL_SET_MASK,value,0);
}
static void freeze(PeRagdollAction *a,PeRagdollUpdate *u,bool value){
    a->freeze_requested=value;op(u,PE_RAGDOLL_FREEZE,value,0);
}
static void motor(PeRagdollUpdate *u,bool present){
    if(present){op(u,PE_RAGDOLL_CLEAR_DESIRED_MOVEMENT,false,0);op(u,PE_RAGDOLL_CANCEL_MOVE_REQUEST,false,0);}
}
static void cleanup(PeRagdollAction *a,bool view,PeRagdollUpdate *u){
    if(a->kind==PE_RAGDOLL_KNOCKED){
        freeze(a,u,false);mask(a,u,false);
        if(view)op(u,PE_RAGDOLL_INJURED,true,1);
    }else{
        mask(a,u,false);freeze(a,u,false);
        if(a->kind==PE_RAGDOLL_GET_UP&&view){
            op(u,PE_RAGDOLL_DEACTIVATE,true,0);op(u,PE_RAGDOLL_INJURED,true,1.5f);
        }
    }
}
static void complete(PeRagdollAction *a,PeRagdollUpdate *u){
    a->running=false;u->manager_complete=true;
    u->continue_actions=pe_ragdoll_source_relations[index_of(a->kind)].pause;
}
bool pe_ragdoll_action_begin(PeRagdollAction *a,PeRagdollStart in,PeRagdollUpdate *out){
    if(!out||!valid(a)||!finite3(in.direction)||!finite3(in.hit_point)||!isfinite(in.force)||in.force<0)return false;
    PeRagdollAction next=*a;PeRagdollUpdate result={0};
    if(next.running&&next.kind!=PE_RAGDOLL_GET_UP){*out=(PeRagdollUpdate){.repeated_ignored=true};return true;}
    result.started=!next.running;result.reset=next.running;next.running=true;
    if(next.kind==PE_RAGDOLL_WENTFLY){
        if(in.view_present){
            mask(&next,&result,true);
            if(!in.parameter_present)result.source_fault=PE_RAGDOLL_FAULT_PARAMETER;
            else{
                PeVec3 force={in.direction.x*in.force,in.direction.y*in.force,in.direction.z*in.force};
                float magnitude=sqrtf(force.x*force.x+force.y*force.y+force.z*force.z);
                if(!finite3(force)||!isfinite(magnitude))return false;
                freeze(&next,&result,true);
                if(in.entity_present&&!in.entity_view_present)result.source_fault=PE_RAGDOLL_FAULT_ENTITY_VIEW;
                else{
                    if(in.entity_present&&in.bone_present){
                        op(&result,PE_RAGDOLL_ACTIVATE,true,0);
                        result.hit_force=force;result.hit_point=in.hit_point;
                        if(magnitude>9.99999975e-06f)result.hit_normal=(PeVec3){-force.x/magnitude,-force.y/magnitude,-force.z/magnitude};
                    }
                    motor(&result,in.motor_present);
                }
            }
        }
    }else if(next.kind==PE_RAGDOLL_KNOCKED){
        if(in.view_present){
            if(!in.ragdoll_active)op(&result,PE_RAGDOLL_ACTIVATE,false,0);
            op(&result,PE_RAGDOLL_INJURED,false,0);
        }
        mask(&next,&result,true);freeze(&next,&result,true);
        next.auto_get_up=true;next.remaining=pe_ragdoll_source_knocked_get_up_time;
        motor(&result,in.motor_present);
    }else{
        if(in.view_present){op(&result,PE_RAGDOLL_DEACTIVATE,false,0);op(&result,PE_RAGDOLL_INJURED,false,0);}
        freeze(&next,&result,true);mask(&next,&result,true);
    }
    *a=next;*out=result;return true;
}
bool pe_ragdoll_action_step(PeRagdollAction *a,PeRagdollTick in,PeRagdollUpdate *out){
    if(!out||!valid(a)||!isfinite(in.delta_time)||in.delta_time<0)return false;
    PeRagdollAction next=*a;PeRagdollUpdate result={0};
    if(next.running){
        if(next.kind==PE_RAGDOLL_WENTFLY){
            if(!in.view_present)result.source_fault=PE_RAGDOLL_FAULT_UPDATE_VIEW;
            else if(!in.ragdoll_active){cleanup(&next,true,&result);complete(&next,&result);}
            else if(in.ready_get_up){
                cleanup(&next,true,&result);op(&result,PE_RAGDOLL_REQUEST_GET_UP,true,0);complete(&next,&result);
            }
        }else if(next.kind==PE_RAGDOLL_KNOCKED){
            if(next.auto_get_up){
                next.remaining-=in.delta_time;
                if(!isfinite(next.remaining))return false;
                if(next.remaining<=0){
                    next.auto_get_up=false;cleanup(&next,in.view_present,&result);
                    op(&result,PE_RAGDOLL_REQUEST_GET_UP,true,0);
                    /* Update returns false. GetUp's relation must remove this
                     * running action even though it just cleared its mask. */
                }
            }
        }else if(!in.view_present||!in.ragdoll_active){
            cleanup(&next,in.view_present,&result);complete(&next,&result);
        }
    }
    *a=next;*out=result;return true;
}
bool pe_ragdoll_action_end(PeRagdollAction *a,bool view,bool immediately,PeRagdollUpdate *out){
    if(!out||!valid(a))return false;
    PeRagdollAction next=*a;PeRagdollUpdate result={0};
    if(next.running){
        cleanup(&next,view,&result);
        if(!immediately&&next.kind==PE_RAGDOLL_WENTFLY)op(&result,PE_RAGDOLL_REQUEST_GET_UP,true,0);
        else complete(&next,&result);
    }
    *a=next;*out=result;return true;
}
bool pe_ragdoll_ready_get_up(bool falling,float speed,float settled,bool *out){
    if(!out||!isfinite(speed)||speed<0||!isfinite(settled)||settled<0)return false;
    *out=falling&&speed<settled;return true;
}
bool pe_ragdoll_deferred_force(PeVec3 force,PeVec3 *out){
    if(!out||!finite3(force))return false;
    PeVec3 next={force.x*100,force.y*100,force.z*100};
    if(!finite3(next))return false;
    *out=next;return true;
}
bool pe_ragdoll_freeze_set(PeRagdollFreeze *s,PeRagdollKind kind,bool value,PeRagdollPhysics *out){
    int i=index_of(kind);if(!s||!out||i<0||s->classes>7)return false;
    PeRagdollFreeze next=*s;unsigned bit=1u<<(unsigned)i;
    if(value)next.classes|=bit;else next.classes&=~bit;
    *s=next;*out=(PeRagdollPhysics){!(next.classes||next.other_classes||next.system||next.network),!next.system};return true;
}
