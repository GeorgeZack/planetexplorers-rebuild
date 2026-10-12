#!/usr/bin/env python3
"""Committed static CIL excerpts: build, self-check and bind to installed bytes.

An excerpt keeps selected method bodies of pinned managed assemblies as raw CIL
with their hashes, taken from saved REA inspect-managed-members inventories.
Checks never load an assembly; they compare hashes and file bytes only.
"""
import hashlib
import json
from pathlib import Path

import cil_disasm

LIMITATIONS = ['Metadata tokens are build-local and only meaningful with the stated artifact SHA-256 and MVID.',
               'CIL is read from file bytes; no JIT, floating-point or engine behavior is observed.']


def key(method):
    return method['declaring_type']+'::'+method['name']


def digest(evidence):
    rows=sorted((name,m['token'],m['body']['il_sha256']) for name,part in evidence['assemblies'].items()
                for m in part['methods'])
    return hashlib.sha256(json.dumps(rows,separators=(',',':')).encode()).hexdigest()


def excerpt(inventories,managed,select,scope,limitations=()):
    """inventories: assembly name -> saved inventory path; select: name -> predicate."""
    assemblies={}
    for name,path in inventories.items():
        envelope=json.loads(Path(path).read_text()); data=envelope.get('normalized_result') or envelope
        raw=(managed/name).read_bytes(); heap=cil_disasm.streams(raw).get('#US'); methods=[]
        if data['coverage']['state']!='complete': raise ValueError('Incomplete inventory for '+name)
        if hashlib.sha256(raw).hexdigest()!=data['artifact']['sha256']: raise ValueError('Inventory is for another '+name)
        for method in data['methods']:
            if not select[name](method): continue
            body=method['body']; code=cil_disasm.body_bytes(raw,body)
            keep=dict(token=method['token'],declaring_type=method['declaring_type'],name=method['name'],
                signature=dict(raw_sha256=method['signature']['raw_sha256'],
                    return_type=method['signature']['return_type'],
                    parameter_types=method['signature']['parameter_types']),
                body={k:body[k] for k in ('header_format','file_offset','il_size','il_sha256','instruction_count')},
                raw_il_hex=code.hex())
            strings={'0x%08x'%t:cil_disasm.user_string(raw,t,heap) for _,m,t in cil_disasm.decode(code) if m=='ldstr'}
            if strings: keep['strings']=strings
            methods.append(keep)
        assemblies[name]=dict(sha256=data['artifact']['sha256'],byte_length=data['artifact']['byte_length'],
            mvid=data['module']['mvid'],rea_evidence_id=envelope.get('evidence_id'),methods=methods)
    return dict(schema=1,authority='shipped-artifact',confidence='observed',
        provider='rea-agents 6.0.0 inspect-managed-members (CLI); raw CIL sliced by cil_disasm.py',
        scope=scope,limitations=LIMITATIONS+list(limitations),assemblies=assemblies)


def load(path,assemblies,methods_digest,method_il):
    """Check an excerpt against pinned identities; return (evidence, {name: method})."""
    evidence=json.loads(Path(path).read_text()); found={}
    if set(evidence['assemblies'])!=set(assemblies): raise ValueError('Unexpected evidence assemblies')
    for name,(sha,mvid) in assemblies.items():
        part=evidence['assemblies'][name]
        if part['sha256']!=sha or part['mvid']!=mvid: raise ValueError('Unqualified evidence identity for '+name)
        for method in part['methods']:
            data=bytes.fromhex(method['raw_il_hex']); body=method['body']
            if len(data)!=body['il_size'] or hashlib.sha256(data).hexdigest()!=body['il_sha256']:
                raise ValueError('Method evidence differs from its hash: '+key(method))
            code=cil_disasm.decode(data)
            if len(code)!=body['instruction_count']: raise ValueError('Undecoded method evidence: '+key(method))
            if {'0x%08x'%t for _,m,t in code if m=='ldstr'}!=set(method.get('strings',{})):
                raise ValueError('String evidence differs: '+key(method))
            found.setdefault(key(method),[]).append(method)
    if digest(evidence)!=methods_digest: raise ValueError('Method evidence changed')
    selected={}
    for name,prefix in method_il.items():
        # Overloads share a name; the pinned body selects one.
        match=[m for m in found.get(name,()) if m['body']['il_sha256'].startswith(prefix)]
        if len(match)!=1: raise ValueError('Unqualified method body: '+name)
        selected[name]=match[0]
    return evidence,selected


def qualify_installed(evidence,assemblies,managed):
    """Bind every excerpted body and string literal to the installed bytes."""
    for name,(sha,_) in assemblies.items():
        raw=(managed/name).read_bytes(); heap=None
        if hashlib.sha256(raw).hexdigest()!=sha: raise ValueError('Unqualified installed '+name)
        for method in evidence['assemblies'][name]['methods']:
            if cil_disasm.body_bytes(raw,method['body'])!=bytes.fromhex(method['raw_il_hex']):
                raise ValueError('Method evidence differs from installed bytes: '+key(method))
            for token,text in method.get('strings',{}).items():
                heap=heap or cil_disasm.streams(raw)['#US']
                if cil_disasm.user_string(raw,int(token,16),heap)!=text:
                    raise ValueError('String evidence differs from installed bytes: '+key(method))


def strings(method):
    """ldstr literals of one excerpted method, in instruction order."""
    table=method.get('strings',{})
    return [table['0x%08x'%t] for _,m,t in cil_disasm.decode(bytes.fromhex(method['raw_il_hex'])) if m=='ldstr']
