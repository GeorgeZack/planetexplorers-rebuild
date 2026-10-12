#!/usr/bin/env python3
"""Recover disconnected Wentfly/Knocked/GetUp requests, not ragdoll simulation."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
from export_content import ROOT, DEFAULT_DATABASE, connect_readonly, describe_source, numeric_rows
from export_hit_reaction import PLAYER, BEAT, guid, f
from export_human_phy import read_objects
from export_starter_outfit import body, uncomment
ACTION = 'Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Beat.cs'
BASE = 'Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/PEAction.cs'
MANAGER = 'Assets/Scripts/PeEntity/Cmpt/Motion/MotionMgrCmpt.cs'
VIEW = 'Assets/Scripts/PeEntity/Cmpt/BiologyViewCmpt.cs'
RAGDOLL = 'Assets/Scripts/Assist/PERagdollController.cs'
SOURCES = (PLAYER, BEAT, BEAT+'.meta', ACTION, BASE, MANAGER, VIEW, RAGDOLL)
METHODS = (
    ('Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/PEAction.cs', 'public\\s+enum\\s+PEActionType', 'cbce9c7ae392fa5889f6af6fa98bff526e5d669d2eb38d6c516b4e43dacfb72e'),
    ('Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/PEAction.cs', 'public\\s+enum\\s+PEActionMask', '8bd60ed668797604e6f8a4e042c849fb2eaa0b14b1a98c58ef428c16500f4af7'),
    ('Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/PEAction.cs', 'public\\s+static\\s+void\\s+LoadActionRelation', '5c5a06c1ee1e16aca4107baba07c840d183707e34558099c16167a896b85da74'),
    ('Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Beat.cs', 'public\\s+class\\s+Action_Wentfly', '1e19c2bc04b9fdfeb8e4268afa48c64cf51c2ec7936b2fdecfa36a9f9db3ed96'),
    ('Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Beat.cs', 'public\\s+class\\s+Action_Knocked', 'e1b800bbcb3e47873736dd273231563bb6a4dbf80b8b613a91b2a85c329c0df7'),
    ('Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Beat.cs', 'public\\s+class\\s+Action_GetUp', 'f23ce483d9eaa8546094ec2fae3ce9ab62459a4580d203f8ed55c266ed08870d'),
    ('Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/PEAction.cs', 'public\\s+abstract\\s+class\\s+PEAction', 'c443537080d22ef14706226c3e35a351a17389c010ca95a06bb180cf938ae5f2'),
    ('Assets/Scripts/PeEntity/Cmpt/Motion/Motion_Beat.cs', 'public\\s+override\\s+void\\s+Start', '26487b70b6f579e662db5d2266abe616c1d3f728006afbe1248df65f9e044bf1'),
    ('Assets/Scripts/PeEntity/Cmpt/Motion/Motion_Beat.cs', 'public\\s+void\\s+OnMsg', 'da2592018cd8f26703dad2dce118fc8d6666e9a21baa2e7af61e53424942d3b1'),
    ('Assets/Scripts/PeEntity/Cmpt/Motion/Motion_Beat.cs', 'void\\s+ApplyMoveEffect', 'f16ea238ae2415af3396ffa506fe629797df06e855d89f4052213644da1945e9'),
    ('Assets/Scripts/PeEntity/Cmpt/Motion/MotionMgrCmpt.cs', 'class\\s+RunningAction', '9578ae9ef45f7372e68b12e71fed364e4fe6b707cb87c3557c95382f16033034'),
    ('Assets/Scripts/PeEntity/Cmpt/Motion/MotionMgrCmpt.cs', 'public\\s+bool\\s+DoAction\\s*\\(', 'f58a6d9b88dc13c74170eeb3c230d55da2a7258d8f45ed591d6bdddaba54bd85'),
    ('Assets/Scripts/PeEntity/Cmpt/Motion/MotionMgrCmpt.cs', 'public\\s+void\\s+DoActionImmediately', 'b05053ac901f0c4da27ec9c37bc71a6a2b2535a33f9d18b4fb9b933d4898fa00'),
    ('Assets/Scripts/PeEntity/Cmpt/Motion/MotionMgrCmpt.cs', 'public\\s+bool\\s+EndAction\\s*\\(', '2fe01d7142c112249396f199a723341b7a7275d38424ec4d064499c650fef473'),
    ('Assets/Scripts/PeEntity/Cmpt/Motion/MotionMgrCmpt.cs', 'public\\s+bool\\s+EndImmediately', 'd0357e0e7a7c2579ed61efda714e1df20d47bbdfc63f56df8657179e1a7c866a'),
    ('Assets/Scripts/PeEntity/Cmpt/Motion/MotionMgrCmpt.cs', 'public\\s+void\\s+FreezePhyState\\s*\\(', 'a405225c6ce6e89df7d3fc52a8f0a7a1d5389249dd7afb21b0b2992383db8520'),
    ('Assets/Scripts/PeEntity/Cmpt/Motion/MotionMgrCmpt.cs', 'void\\s+UpdatePhy', '5d1044c12ecc150cea9991046bfc03748a4e1bcccb974a34070b2f17a5c1059c'),
    ('Assets/Scripts/PeEntity/Cmpt/BiologyViewCmpt.cs', 'public\\s+bool\\s+IsRagdoll', '24ec8452792c039143602e80b1ae403708c03a241a77ca9102f9558537acd84c'),
    ('Assets/Scripts/PeEntity/Cmpt/BiologyViewCmpt.cs', 'public\\s+bool\\s+IsReadyGetUp', 'd62dd5920dab17b40fac8a7f7282f1f052b1f1c46c952b5b3a1ebe3811696a3c'),
    ('Assets/Scripts/PeEntity/Cmpt/BiologyViewCmpt.cs', 'public\\s+Transform\\s+GetRagdollTransform', 'a1a3de73400319d53ee641025880f011defe38d4d9d02f484599469333adaf60'),
    ('Assets/Scripts/PeEntity/Cmpt/BiologyViewCmpt.cs', 'public\\s+void\\s+ActivateRagdoll\\s*\\(', 'eff41649d7198d53f8cd86ff47384746519512e2f55f8c08c62064cf46f6a277'),
    ('Assets/Scripts/PeEntity/Cmpt/BiologyViewCmpt.cs', 'public\\s+void\\s+DeactivateRagdoll', 'cc5b680136607e36cdb7e5d5ca2a75beb401875efb3faebd6905c88b2a71a1b4'),
    ('Assets/Scripts/PeEntity/Cmpt/BiologyViewCmpt.cs', 'public\\s+void\\s+ActivateInjured', 'fa159f021a2644e6a5fe5e19e37e58d7d943e62c969298cf0fd8bad484d60ce3'),
    ('Assets/Scripts/PeEntity/Cmpt/BiologyViewCmpt.cs', 'void\\s+DoActivateInjured', 'b97a7fefd7e671e23291d1d75e1a32d5923e479e81d23abd4702fc688e46bdb8'),
    ('Assets/Scripts/PeEntity/Cmpt/BiologyViewCmpt.cs', 'void\\s+IRagdollHandler.OnGetupFinished', '552a085d20efb2985a765b7c2451f993f4893d2b10a7c769178d77b183fedbc8'),
    ('Assets/Scripts/Assist/PERagdollController.cs', 'public\\s+void\\s+Activate\\s*\\(', '5b3f788dff1523d03b549cc2eba57a7b30837775357c61c4489c8dd2e82aae46'),
    ('Assets/Scripts/Assist/PERagdollController.cs', 'public\\s+void\\s+Deactivate', 'e4a76b36b7150bb0d70b0f5f01f1ce7382245357af4b6f5156e5fa47859d6bc4'),
    ('Assets/Scripts/Assist/PERagdollController.cs', 'IEnumerator\\s+AddForce', 'cf96e711975588d82ec26b2ed43f2695c90816d0a388f03c52ccf8891261e336'),
    ('Assets/Scripts/Assist/PERagdollController.cs', 'public\\s+bool\\s+IsReadyGetUp', 'd49d487a0a663aa586b39d88206abb04f4d539bdc0a1e5daa462d15f4bc98c72'),
    ('Assets/Scripts/Assist/PERagdollController.cs', 'protected\\s+override\\s+bool\\s+IsGetupReady', '2c2052a8c2e2d3cb0efbe87e44c003e3a3e949396f2363300d5041e7813ca119'),
    ('Assets/Scripts/Assist/PERagdollController.cs', 'protected\\s+override\\s+void\\s+OnFallBegin', '4aef09649d3178ee5392237154338cb312b9d2f4ebb5d8e53a0bab54911e329e'),
    ('Assets/Scripts/Assist/PERagdollController.cs', 'protected\\s+override\\s+void\\s+OnGetupFinished', '4b0d3c32b04dfae640f2b856b02ed8a756e9ca9ca7aa5b451c67e4c55224dc6b'),
)
RELATION_DIGESTS = {7: '57db55c1b37c2741f62440efd01ee54cd628ea9d1d2a75583294743d97a1f4e5', 8: '08c8e8b049a82172a07183f040c14af94946e57f62d964f88648a5788f8d3b78', 10: '615f7c92c569542f573955c6a01566c152ce2c3e62a241f04199ac3e6ee1f922'}
KINDS = ((7,'Wentfly'),(8,'Knocked'),(10,'GetUp'))

def require(value,message):
    if not value: raise ValueError(message)

def digest(value):
    return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode()).hexdigest()

def validate_sources(root):
    for path,pattern,expected in METHODS:
        text=uncomment((root/path).read_text(encoding='utf-8-sig',errors='replace'))
        require(hashlib.sha256(body(text,pattern).encode()).hexdigest()==expected,
                'Ragdoll action source changed: '+path+' '+pattern)
    return [describe_source(root/path) for path in SOURCES]

def read_relations(database):
    db=connect_readonly(database)
    try:
        rows,_=numeric_rows(db,'ActionRelation','ActionType');result=[]
        for action,name in KINDS:
            row=dict(rows[action]);require(row['Name']==name and digest(row)==RELATION_DIGESTS[action],
                                          'Ragdoll action relation changed: '+name)
            fields=list(row)[5:]
            require(len(fields)==56,'Action mask count changed')
            required=sum(1<<i for i,field in enumerate(fields) if int(row[field])==1)
            forbidden=sum(1<<i for i,field in enumerate(fields) if int(row[field])==2)
            def actions(field): return sum(1<<int(v) for v in row[field].split(',') if int(v))
            result.append(dict(action=action,name=name,row=row,required=required,forbidden=forbidden,
                               pause=actions('PauseAction'),end=actions('EndAction'),immediate=actions('EndImmediately')))
        return result
    finally: db.close()

def qualify(player,beat_guid):
    require([k for k,v in player.items() if v.get('script_guid')==beat_guid]==[11400038],
            'Player Motion_Beat identity changed')
    data=player[11400038]['data']
    require(data['m_Enabled']==1,'Player Motion_Beat disabled')
    require(data['m_Wentfly']=={'m_PauseActions':[]} and data['m_GetUp']=={'m_PauseActions':[]}
            and data['m_Knocked']=={'m_PauseActions':[],'m_GetUpTime':2.0},'Player ragdoll action defaults changed')
    return dict(component=11400038,knocked_get_up_time=data['m_Knocked']['m_GetUpTime'])

def build_outputs(root=ROOT,database=DEFAULT_DATABASE):
    sources=validate_sources(root);binding=qualify(read_objects(root/PLAYER),guid(root,BEAT));relations=read_relations(database)
    header='/* Generated by export_ragdoll_actions.py; do not edit. */\n#ifndef PE_RAGDOLL_ACTION_SOURCE_H\n#define PE_RAGDOLL_ACTION_SOURCE_H\n#include <stdint.h>\n'
    header+='typedef struct {unsigned action;uint64_t required,forbidden,pause,end,immediate;} PeRagdollSourceRelation;\n'
    header+='static const PeRagdollSourceRelation pe_ragdoll_source_relations[]={\n'
    for r in relations:
        header+='    {%d,%s}, /* %s */\n'%(r['action'],','.join('UINT64_C(%d)'%r[k] for k in ('required','forbidden','pause','end','immediate')),r['name'])
    header+='};\nstatic const float pe_ragdoll_source_knocked_get_up_time=%s;\n#endif\n'%f(binding['knocked_get_up_time'])
    metadata=dict(schema=1,source=describe_source(database),source_files=sources,binding=binding,relations=relations,
        method_guards=[dict(path=p,pattern=r,sha256=h) for p,r,h in METHODS],
        scope='Disconnected admitted-action lifecycle, ordered view/motor/freeze requests, readiness predicate and deferred force scaling only. No ragdoll simulation, PhysX, AnimFollow, get-up pose or host integration.',
        quirks=['Knocked player prefab delay is 2s, overriding the class default 3s.',
                'Knocked clears its mask and freeze, requests immediate GetUp, but Update still returns false.',
                'GetUp immediate dispatch bypasses normal admission and immediately ends Wentfly and Knocked.',
                'Normal GetUp admission requires both Knocked and Lie masks; immediate dispatch bypasses them.',
                'Wentfly missing view dereferences null in Update; C reports a source fault rather than inventing cleanup.',
                'Knocked reaction decision routes to Wentfly; it does not start Action_Knocked.',
                'View_Ragdoll_Getup_Finished action-ending branch in Motion_Beat is commented out.',
                'Freeze requests are keyed by action class; action freeze disables model physics, system freeze alone disables ragdoll physics.',
                'BiologyViewCmpt.IsRagdoll reads the inherited controller active, not PERagdollController.IsRagdoll().'],
        view_contract='Deactivate(false) sets get-up readiness; it does not immediately clear ragdoll state. Deactivate(true) stops falling/gettingUp, disables animFollow and invokes OnGetupFinished. ActivateInjured sets defence-trigger admission, cancels an older delayed request, and applies or invokes its replacement.',
        deferred_force='AddForce waits for the next FixedUpdate; with a valid hit transform and Rigidbody it calls AddForceAtPosition(hitForce * 100, hitPoint) with default Force mode. It is not a velocity impulse.',
        readiness='PERagdollController.IsReadyGetUp is falling && root Rigidbody velocity magnitude < settledSpeed. The inherited threshold, active lifecycle and pose are caller inputs, not fabricated constants.')
    return {'pe_ragdoll_action_source.h':header.encode(),'pe_ragdoll_action_source.json':(json.dumps(metadata,indent=2)+'\n').encode()}

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');p.add_argument('--output',type=Path,default=ROOT/'rebuild/generated');a=p.parse_args(argv)
    try:
        for name,data in build_outputs().items():
            if a.check: require((a.output/name).is_file() and (a.output/name).read_bytes()==data,'Stale or missing '+name)
            else: a.output.mkdir(parents=True,exist_ok=True);(a.output/name).write_bytes(data)
    except (OSError,ValueError,KeyError,ImportError,IndexError) as error:
        print(error,file=sys.stderr);return 1
    return 0
if __name__=='__main__': raise SystemExit(main())
