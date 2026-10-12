#!/usr/bin/env python3
"""Decode ECMA-335 CIL method bodies from file bytes; never loads the assembly.

A reading aid for compiled-only original classes. Token names come from a
caller-supplied table (for example an REA inspect-managed-members inventory);
unknown tokens print numerically. Decoding is not a qualification on its own:
exporters still pin artifact and method-body hashes.
"""
import argparse
import json
from pathlib import Path
import struct
import sys

PLAIN = {0x00:'nop',0x01:'break',0x14:'ldnull',0x15:'ldc.i4.m1',0x25:'dup',0x26:'pop',0x2A:'ret',
         0x50:'ldind.ref',0x51:'stind.ref',0x58:'add',0x59:'sub',0x5A:'mul',0x5B:'div',0x5C:'div.un',
         0x5D:'rem',0x5E:'rem.un',0x5F:'and',0x60:'or',0x61:'xor',0x62:'shl',0x63:'shr',0x64:'shr.un',
         0x65:'neg',0x66:'not',0x67:'conv.i1',0x68:'conv.i2',0x69:'conv.i4',0x6A:'conv.i8',0x6B:'conv.r4',
         0x6C:'conv.r8',0x6D:'conv.u4',0x6E:'conv.u8',0x76:'conv.r.un',0x7A:'throw',0x8E:'ldlen',
         0x9A:'ldelem.ref',0xC3:'ckfinite',0xD1:'conv.u2',0xD2:'conv.u1',0xD3:'conv.i',0xD4:'conv.ovf.i',
         0xD5:'conv.ovf.u',0xD6:'add.ovf',0xD7:'add.ovf.un',0xD8:'mul.ovf',0xD9:'mul.ovf.un',0xDA:'sub.ovf',
         0xDB:'sub.ovf.un',0xDC:'endfinally',0xDF:'stind.i',0xE0:'conv.u'}
for _i in range(4):
    PLAIN[0x02+_i]='ldarg.%d'%_i; PLAIN[0x06+_i]='ldloc.%d'%_i; PLAIN[0x0A+_i]='stloc.%d'%_i
for _i in range(9): PLAIN[0x16+_i]='ldc.i4.%d'%_i
for _i,_n in enumerate('i1 u1 i2 u2 i4 u4 i8 i r4 r8'.split()):
    PLAIN[0x46+_i]='ldind.'+_n; PLAIN[0x90+_i]='ldelem.'+_n
for _i,_n in enumerate('i1 i2 i4 i8 r4 r8'.split()): PLAIN[0x52+_i]='stind.'+_n
for _i,_n in enumerate('i i1 i2 i4 i8 r4 r8 ref'.split()): PLAIN[0x9B+_i]='stelem.'+_n
for _i,_n in enumerate('i1 i2 i4 i8 u1 u2 u4 u8 i u'.split()): PLAIN[0x82+_i]='conv.ovf.%s.un'%_n
for _i,_n in enumerate('i1 u1 i2 u2 i4 u4 i8 u8'.split()): PLAIN[0xB3+_i]='conv.ovf.'+_n
BYTE = {0x0E:'ldarg.s',0x0F:'ldarga.s',0x10:'starg.s',0x11:'ldloc.s',0x12:'ldloca.s',0x13:'stloc.s',0x1F:'ldc.i4.s'}
BRANCH = 'br brfalse brtrue beq bge bgt ble blt bne.un bge.un bgt.un ble.un blt.un'.split()
TOKEN = {0x27:'jmp',0x28:'call',0x29:'calli',0x6F:'callvirt',0x70:'cpobj',0x71:'ldobj',0x72:'ldstr',
         0x73:'newobj',0x74:'castclass',0x75:'isinst',0x79:'unbox',0x7B:'ldfld',0x7C:'ldflda',0x7D:'stfld',
         0x7E:'ldsfld',0x7F:'ldsflda',0x80:'stsfld',0x81:'stobj',0x8C:'box',0x8D:'newarr',0x8F:'ldelema',
         0xA3:'ldelem',0xA4:'stelem',0xA5:'unbox.any',0xC2:'refanyval',0xC6:'mkrefany',0xD0:'ldtoken'}
PREFIXED = {0x00:'arglist',0x01:'ceq',0x02:'cgt',0x03:'cgt.un',0x04:'clt',0x05:'clt.un',0x0F:'localloc',
            0x11:'endfilter',0x13:'volatile.',0x14:'tail.',0x17:'cpblk',0x18:'initblk',0x1A:'rethrow',
            0x1D:'refanytype',0x1E:'readonly.'}
PREFIXED_TOKEN = {0x06:'ldftn',0x07:'ldvirtftn',0x15:'initobj',0x16:'constrained.',0x1C:'sizeof'}
PREFIXED_VAR = {0x09:'ldarg',0x0A:'ldarga',0x0B:'starg',0x0C:'ldloc',0x0D:'ldloca',0x0E:'stloc'}


def decode(code):
    """Return [(offset, mnemonic, operand)]; raise ValueError on any undecodable byte."""
    out=[]; i=0
    def take(fmt):
        nonlocal i
        size=struct.calcsize(fmt)
        if i+size>len(code): raise ValueError('Truncated CIL operand')
        value=struct.unpack_from(fmt,code,i)[0]; i+=size
        return value
    while i<len(code):
        start=i; op=code[i]; i+=1
        if op in PLAIN: item=(PLAIN[op],None)
        elif op in BYTE: item=(BYTE[op],take('<b'))
        elif 0x2B<=op<=0x37: delta=take('<b'); item=(BRANCH[op-0x2B]+'.s',i+delta)
        elif 0x38<=op<=0x44: delta=take('<i'); item=(BRANCH[op-0x38],i+delta)
        elif op==0x20: item=('ldc.i4',take('<i'))
        elif op==0x21: item=('ldc.i8',take('<q'))
        elif op==0x22: item=('ldc.r4',take('<f'))
        elif op==0x23: item=('ldc.r8',take('<d'))
        elif op==0x45:
            count=take('<I'); deltas=[take('<i') for _ in range(count)]
            item=('switch',tuple(i+d for d in deltas))
        elif op==0xDD: delta=take('<i'); item=('leave',i+delta)
        elif op==0xDE: delta=take('<b'); item=('leave.s',i+delta)
        elif op in TOKEN: item=(TOKEN[op],take('<I'))
        elif op==0xFE:
            if i>=len(code): raise ValueError('Truncated CIL prefix')
            second=code[i]; i+=1
            if second in PREFIXED: item=(PREFIXED[second],None)
            elif second in PREFIXED_TOKEN: item=(PREFIXED_TOKEN[second],take('<I'))
            elif second in PREFIXED_VAR: item=(PREFIXED_VAR[second],take('<H'))
            elif second==0x12: item=('unaligned.',take('<B'))
            else: raise ValueError('Unknown CIL opcode fe %02x'%second)
        else: raise ValueError('Unknown CIL opcode %02x'%op)
        out.append((start,)+item)
    return out


def is_token(mnemonic):
    return mnemonic in TOKEN.values() or mnemonic in PREFIXED_TOKEN.values()


def is_branch(mnemonic):
    return mnemonic.split('.s')[0] in BRANCH or mnemonic.startswith('leave')


def body_bytes(raw,body):
    """Slice a method's CIL using an REA body record, checking its header."""
    offset=body['file_offset']; size=body['il_size']
    if not 0<=offset<len(raw): raise ValueError('Method body outside file')
    if body['header_format']=='tiny':
        if raw[offset]&3!=2 or raw[offset]>>2!=size: raise ValueError('Tiny header differs')
        header=1
    else:
        if offset+12>len(raw): raise ValueError('Truncated fat header')
        flags,_,stored=struct.unpack_from('<HHI',raw,offset)
        if flags&3!=3 or stored!=size: raise ValueError('Fat header differs')
        header=(flags>>12)*4
    code=raw[offset+header:offset+header+size]
    if len(code)!=size: raise ValueError('Truncated method body')
    return code


def listing(code,names=None):
    names=names or {}; lines=[]
    for offset,mnemonic,operand in decode(code):
        if operand is None: text=mnemonic
        elif is_token(mnemonic): text='%s %s'%(mnemonic,names.get(operand,'0x%08x'%operand))
        elif mnemonic=='switch': text='switch '+','.join('IL_%04x'%t for t in operand)
        elif is_branch(mnemonic): text='%s IL_%04x'%(mnemonic,operand)
        elif mnemonic=='ldc.r4': text='ldc.r4 %r'%operand
        else: text='%s %s'%(mnemonic,operand)
        lines.append('IL_%04x: %s'%(offset,text))
    return lines


def inventory_names(inventory):
    """Token names from an REA inspect-managed-members result (envelope or bare)."""
    data=inventory.get('normalized_result') or inventory; names={}
    for t in data['types']: names[int(t['token'],16)]=t['full_name']
    for f in data['fields']: names[int(f['token'],16)]='%s::%s'%(f.get('declaring_type'),f['name'])
    for m in data['methods']: names[int(m['token'],16)]='%s::%s'%(m['declaring_type'],m['name'])
    for r in data['member_refs']: names.setdefault(int(r['token'],16),'[%s]::%s'%(r['parent_token'],r['name']))
    return data,names


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('inventory',type=Path,help='saved REA inspect-managed-members JSON')
    parser.add_argument('assembly',type=Path)
    parser.add_argument('names',nargs='+',help='declaring type, or Type::Method')
    args=parser.parse_args(argv)
    try:
        data,names=inventory_names(json.loads(args.inventory.read_text())); raw=args.assembly.read_bytes()
        for method in data['methods']:
            if method['declaring_type'] not in args.names and '%s::%s'%(method['declaring_type'],method['name']) not in args.names:
                continue
            body=method['body']
            print('\n%s %s::%s il=%s sha256=%s'%(method['token'],method['declaring_type'],method['name'],body.get('il_size'),body.get('il_sha256')))
            if body.get('status')=='present': print('\n'.join('  '+line for line in listing(body_bytes(raw,body),names)))
    except (OSError,ValueError,KeyError) as error:
        print(error,file=sys.stderr); return 1
    return 0


if __name__=='__main__': raise SystemExit(main())
