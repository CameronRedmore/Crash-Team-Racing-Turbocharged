"""Generate deferred static pointer fixups for P32 fields (64-bit build).
Run against the 64-bit configuration: python3 gen_fixups.py [--apply]"""
import sys, json, subprocess, collections, re
sys.path.insert(0,'/tmp/ctr64-tools')
from common import *
sys.setrecursionlimit(100000)
APPLY='--apply' in sys.argv
CK=ci.CursorKind; TK=ci.TypeKind
tu=parse()
rd=args_for()

_cache={}
def has_p32(t):
    t=t.get_canonical()
    k=t.spelling
    if k.endswith('CtrPtr32') and t.kind==TK.RECORD: return True
    if t.kind in (TK.CONSTANTARRAY,TK.INCOMPLETEARRAY): return has_p32(t.element_type)
    if t.kind==TK.RECORD:
        key=t.get_declaration().hash
        if key in _cache: return _cache[key]
        _cache[key]=False
        r=any(has_p32(f.type) for f in t.get_fields())
        _cache[key]=r
        return r
    return False

cands=[]
def walk(c):
    for ch in c.get_children():
        if ch.kind==CK.VAR_DECL and ch.is_definition() and ch.location.file and in_project(ch.location.file.name):
            kids=[k for k in ch.get_children()]
            if any(k.kind==CK.INIT_LIST_EXPR for k in kids) and has_p32(ch.type):
                cands.append(ch)
        elif ch.kind==CK.FUNCTION_DECL:
            pass
walk(tu.cursor)
print('candidate vars',len(cands),file=sys.stderr)

def jdump(name):
    cmd=['clang']+rd+['-Xclang','-ast-dump=json','-Xclang','-ast-dump-filter='+name,'-fsyntax-only',ROOT+'/main.c']
    r=subprocess.run(cmd,capture_output=True)
    raw=r.stdout.decode('utf8','replace'); dec=json.JSONDecoder(); i=0; objs=[]
    while i<len(raw):
        while i<len(raw) and raw[i].isspace(): i+=1
        if i>=len(raw): break
        o,j=dec.raw_decode(raw,i); objs.append(o); i=j
    return objs

def off(loc):
    if 'spellingLoc' in loc: return None
    return loc.get('offset')
def span(n,src=None):
    r=n.get('range',{})
    b=r.get('begin',{}); e=r.get('end',{})
    if 'spellingLoc' in b or 'spellingLoc' in e:
        if 'spellingLoc' in b and 'spellingLoc' in e and b['expansionLoc'].get('isMacroArgExpansion') and e['expansionLoc'].get('isMacroArgExpansion'):
            sb=b['spellingLoc']; se=e['spellingLoc']
            if 'offset' in sb and 'offset' in se and not sb.get('includedFrom') or True:
                return sb['offset'], se['offset']+se.get('tokLen',0)
        if src is not None and 'expansionLoc' in b and 'expansionLoc' in e:
            eb=b['expansionLoc']; ee=e['expansionLoc']
            if eb.get('offset') is not None and eb.get('offset')==ee.get('offset') and src[eb['offset']:eb['offset']+eb.get('tokLen',0)]==b'NULL':
                return eb['offset'], eb['offset']+4
        return None
    if 'offset' not in b or 'offset' not in e: return None
    return b['offset'], e['offset']+e.get('tokLen',0)

results=[]   # (file, var_end_offset, varname, [(path,text,s,e)] )
manual=[]
def top_expr(n):
    return n

def walk_init(node,ctype,path,out,src,pathfile):
    ct=ctype.get_canonical()
    ctype_sp=ct.spelling
    if ct.kind==TK.RECORD and ctype_sp.endswith('CtrPtr32'):
        inner=node.get('inner',[]) if node['kind']=='InitListExpr' else [node]
        e=inner[0] if inner else None
        if e is None or e['kind']=='ImplicitValueInitExpr': return
        c=e; ptrcast=False
        while c['kind'] in ('ImplicitCastExpr','CStyleCastExpr','ParenExpr','ConstantExpr') and c.get('inner'):
            if c.get('type',{}).get('qualType','').endswith('*'): ptrcast=True
            c=c['inner'][0]
        iszero = c['kind']=='IntegerLiteral' and c.get('value')=='0'
        if iszero and not ptrcast: return
        sp=span(e,src)
        if sp is None and iszero: return
        if sp is None: manual.append((pathfile,path,'macro leaf')); return
        out.append((path,src[sp[0]:sp[1]].decode('latin1'),sp[0],sp[1],iszero)); return
    if ct.kind==TK.RECORD:
        fields=list(ct.get_fields())
        inner=node.get('inner',[]) if node['kind']=='InitListExpr' else []
        if node['kind']!='InitListExpr': return
        if ct.get_declaration().kind==CK.UNION_DECL:
            fn=node.get('field',{}).get('name')
            for f in fields:
                if f.spelling==fn or (not fn):
                    if inner: walk_init(inner[0],f.type,path+('.'+f.spelling if f.spelling else ''),out,src,pathfile)
                    break
            return
        fi=0
        named=[f for f in fields if not (f.is_bitfield() and f.spelling=='')]
        for k,child in enumerate(inner):
            if k>=len(named): break
            f=named[k]
            walk_init(child,f.type,path+('.'+f.spelling if f.spelling else ''),out,src,pathfile)
        return
    if ct.kind in (TK.CONSTANTARRAY,TK.INCOMPLETEARRAY):
        if node['kind']!='InitListExpr': return
        et=ct.element_type
        elems=node['array_filler'][1:] if 'array_filler' in node else node.get('inner',[])
        for k,child in enumerate(elems):
            walk_init(child,et,path+'[%d]'%k,out,src,pathfile)
        return

byfile=collections.defaultdict(list)
for v in cands:
    f=os.path.realpath(v.location.file.name)
    src=open(f,'rb').read()
    objs=jdump(v.spelling)
    node=None
    for o in objs:
        if o.get('kind')=='VarDecl' and o.get('name')==v.spelling and o.get('inner') and o['loc'].get('line',0)==v.location.line or (o.get('kind')=='VarDecl' and o.get('name')==v.spelling and o.get('inner') and o.get('loc',{}).get('offset')==v.location.offset):
            node=o
    if node is None:
        # fallback: any with inner and matching offset/line in file
        for o in objs:
            if o.get('kind')=='VarDecl' and o.get('name')==v.spelling and o.get('inner'): node=o
    if node is None: manual.append((rel(f) if False else f,v.spelling,'no json')); continue
    init=[c for c in node['inner'] if c['kind']=='InitListExpr']
    if not init: manual.append((f,v.spelling,'no initlist')); continue
    out=[]
    walk_init(init[0],v.type,v.spelling,out,src,f)
    if out:
        end=v.extent.end.offset
        byfile[f].append((v.spelling,end,out))
    print(v.spelling,len(out),file=sys.stderr)
tot=sum(1 for l in byfile.values() for _,_,o in l for x in o if not x[4])
print('fixups',tot,'manual',len(manual),file=sys.stderr)
cnt=collections.Counter((m[0].split('64bit/')[-1],m[1].split('.')[0],m[2]) for m in manual)
for k,v in cnt.most_common(): print('MANUAL',v,k,file=sys.stderr)
if APPLY:
    for f,lst in byfile.items():
        src=open(f,'rb').read()
        eds=[]
        for name,end,out in lst:
            # find terminating ';' after end
            semi=src.index(b';',end)
            lines=['','#if defined(CTR_NATIVE_64BIT)','CTR_P32_STATIC_FIXUP(%s)'%name,'{']
            for path,text,s,e,iszero in out:
                eds.append((s,e,'P32_DEFER(%s)'%text))
                if not iszero: lines.append('\tP32_SET(%s, %s);'%(path,' '.join(text.split())))
            lines+=['}','#endif']
            if len(lines)>4: eds.append((semi+1,semi+1,'\n'.join(lines)))
        for s,e,t in sorted(eds,key=lambda x:(x[0],x[1]),reverse=True):
            src=src[:s]+t.encode('latin1')+src[e:]
        open(f,'wb').write(src)
