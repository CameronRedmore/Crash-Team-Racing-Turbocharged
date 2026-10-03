"""Rewrite pointer-field declarations and accesses to the P32 API.
Usage: rewrite.py [--apply] ; writes /tmp/ctr64-tools/report.txt"""
import sys, collections, re
sys.path.insert(0,'/tmp/ctr64-tools')
from common import *
sys.setrecursionlimit(100000)
CK=ci.CursorKind
APPLY='--apply' in sys.argv
SKIP_FILES={'platform/native_checkpoint.c'}
UNITS=['main.c','tests/native_framerate_test.c','tests/native_precision_test.c','tests/native_physics_test.c','tests/native_draw3d_test.c','platform/native_draw3d.c','platform/native_pgxp.c','platform/native_gte_core.c','platform/native_inline_c.c']

def rel(p): return os.path.relpath(os.path.realpath(p),ROOT)
def has_ptr(t):
    t=t.get_canonical(); k=t.kind
    if k==ci.TypeKind.POINTER: return True
    if k in (ci.TypeKind.CONSTANTARRAY,ci.TypeKind.INCOMPLETEARRAY): return has_ptr(t.element_type)
    return False
def arr_rank(t):
    t=t.get_canonical(); n=0
    while t.kind in (ci.TypeKind.CONSTANTARRAY,ci.TypeKind.INCOMPLETEARRAY): n+=1; t=t.element_type
    return n
def elem_type(t):
    c=t
    while c.get_canonical().kind in (ci.TypeKind.CONSTANTARRAY,ci.TypeKind.INCOMPLETEARRAY):
        c=c.element_type if c.kind in (ci.TypeKind.CONSTANTARRAY,ci.TypeKind.INCOMPLETEARRAY) else c.get_canonical().element_type
    return c
def key(cur):
    l=cur.location; return (rel(l.file.name),l.line,l.column)

edits=collections.defaultdict(set)   # file -> {(start,end,text,order)}
manual=[]                            # (file,line,reason,text)
files_text={}
def ftext(path):
    if path not in files_text: files_text[path]=open(path,'rb').read()
    return files_text[path]

CUR_TAG=[None]
SITE_CNT=collections.Counter()
def add(path,start,end,text,order):
    SITE_CNT[(path,CUR_TAG[0],start,end,text,order)]+=1
    edits[path].add((start,end,text,order,CUR_TAG[0],SITE_CNT[(path,CUR_TAG[0],start,end,text,order)]))

targets={}  # key -> FIELD_DECL
def collect_fields(c):
    for ch in c.get_children():
        if ch.kind==CK.FIELD_DECL and ch.location.file and in_project(ch.location.file.name) and convert_scope(ch.location.file.name) and has_ptr(ch.type):
            targets[key(ch)]=ch
        if ch.kind in (CK.STRUCT_DECL,CK.UNION_DECL): collect_fields(ch)

STMT_PARENTS={CK.COMPOUND_STMT,CK.IF_STMT,CK.FOR_STMT,CK.WHILE_STMT,CK.DO_STMT,CK.CASE_STMT,CK.DEFAULT_STMT,CK.LABEL_STMT,CK.SWITCH_STMT}

def decl_edit(f):
    path=os.path.realpath(f.location.file.name)
    ext=f.extent
    s,e=ext.start.offset,ext.end.offset
    src=ftext(path)
    text=src[s:e].decode('latin1')
    t=f.type
    if 'unnamed' in t.spelling or 'anonymous' in t.spelling:
        manual.append((rel(path),f.location.line,'anon type decl',text)); return
    if '(' in text.split(f.spelling)[0][-3:] or re.search(r'\(\s*\*',text):
        manual.append((rel(path),f.location.line,'fn-ptr decl',text)); return
    if ',' in text and not '[' in text:
        manual.append((rel(path),f.location.line,'multi decl',text)); return
    m=re.search(r'\b'+re.escape(f.spelling)+r'\b(.*)$',text,re.S)
    if not m: manual.append((rel(path),f.location.line,'name not found',text)); return
    suffix=m.group(1)
    et=elem_type(t).spelling
    new='P32(%s) %s%s'%(et,f.spelling,suffix)
    # preserve macro-qualified leading words? e.g. 'static' not in fields
    CUR_TAG[0]=('decl',s)
    add(path,s,e,new,0)

def classify(chain, node_idx):
    """chain: list of ancestors from root..node. returns (ctxkind, ctxnode_index) where ctx is parent after skipping parens/implicit casts"""
    i=node_idx
    child=chain[i]
    i-=1
    while i>=0 and chain[i].kind in (CK.UNEXPOSED_EXPR,CK.PAREN_EXPR):
        child=chain[i]; i-=1
    return child,(chain[i] if i>=0 else None),i

def tokens_between(par,a_end,b_start):
    out=[]
    for tk in par.get_tokens():
        o=tk.extent.start.offset
        if o>=a_end and o<b_start: out.append(tk)
    return out

def handle_ref(chain):
    mr=chain[-1]
    CUR_TAG[0]=(mr.extent.start.offset,mr.extent.end.offset)
    r=mr.referenced
    if r is None or key(r) not in targets: return
    f=targets[key(r)]
    if mr.location.file is None or not in_project(mr.location.file.name): return
    path=os.path.realpath(mr.location.file.name)
    if rel(path) in SKIP_FILES: return
    src=ftext(path)
    ext=mr.extent
    s,e=ext.start.offset,ext.end.offset
    if os.path.realpath(ext.start.file.name)!=path or src[e-len(f.spelling):e].decode('latin1')!=f.spelling:
        manual.append((rel(path),mr.location.line,'macro site',src[s:e].decode('latin1')[:80])); return
    L=mr; Lidx=len(chain)-1
    rank=arr_rank(f.type)
    # climb array subscripts
    for _ in range(rank):
        child,par,pi=classify(chain,Lidx)
        if par is not None and par.kind==CK.ARRAY_SUBSCRIPT_EXPR and list(par.get_children())[0].extent.start.offset==L.extent.start.offset and list(par.get_children())[0].extent.end.offset==L.extent.end.offset:
            L=par; Lidx=pi
        else:
            manual.append((rel(path),mr.location.line,'array decay',src[s:e].decode('latin1')[:80])); return
    ls,le=L.extent.start.offset,L.extent.end.offset
    ltext=src[ls:le].decode('latin1')
    T=L.type.spelling
    if 'unnamed' in T or 'anonymous' in T:
        manual.append((rel(path),mr.location.line,'anon type site',ltext[:80])); return
    child,par,pi=classify(chain,Lidx)
    kind='get'
    if par is not None:
        if par.kind==CK.BINARY_OPERATOR:
            ch=list(par.get_children())
            if len(ch)==2 and ch[0].extent.start.offset==child.extent.start.offset and ch[0].extent.end.offset==child.extent.end.offset:
                toks=tokens_between(par,child.extent.end.offset,ch[1].extent.start.offset)
                op=''.join(t.spelling for t in toks if t.spelling not in ('(',')'))
                if op=='=': kind=('set',par,ch[1])
                else: kind=('other_bin',op)
        elif par.kind==CK.COMPOUND_ASSIGNMENT_OPERATOR:
            ch=list(par.get_children())
            if ch[0].extent.start.offset==child.extent.start.offset:
                toks=tokens_between(par,child.extent.end.offset,ch[1].extent.start.offset)
                kind=('compound',par,ch[1],''.join(t.spelling for t in toks))
        elif par.kind==CK.UNARY_OPERATOR:
            toks=list(par.get_tokens())
            op=toks[0].spelling if toks else ''
            if op=='&': kind=('addr',par)
            elif op in ('++','--') or (toks and toks[-1].spelling in ('++','--')): kind=('incdec',par,op)
        elif par.kind==CK.CXX_UNARY_EXPR:
            toks=list(par.get_tokens())
            if toks and toks[0].spelling=='sizeof': kind='sizeof'
    if kind=='sizeof': return
    if kind=='get' or (isinstance(kind,tuple) and kind[0]=='other_bin'):
        add(path,ls,ls,'P32_GET(%s, '%T,1000000-(le-ls))
        add(path,le,le,')',0)
        return
    k=kind[0]
    if k=='set':
        _,p,rhs=kind
        unused = True
        if pi-1>=0 and chain[pi-1].kind not in STMT_PARENTS and not (chain[pi-1].kind==CK.BINARY_OPERATOR and False):
            # allow wrapping paren/unexposed
            j=pi-1
            while j>=0 and chain[j].kind in (CK.UNEXPOSED_EXPR,CK.PAREN_EXPR): j-=1
            if j>=0 and chain[j].kind not in STMT_PARENTS: unused=False
        if not unused:
            manual.append((rel(path),mr.location.line,'assignment value used',src[p.extent.start.offset:p.extent.end.offset].decode('latin1')[:100])); return
        ps,pe=p.extent.start.offset,p.extent.end.offset
        rs=rhs.extent.start.offset
        add(path,ps,ps,'P32_SET(',1000000-(pe-ps))
        add(path,le,rs,', ',0)
        add(path,pe,pe,')',0)
        return
    if k=='compound':
        _,p,rhs,op=kind
        ps,pe=p.extent.start.offset,p.extent.end.offset
        rs=rhs.extent.start.offset
        binop=op[:-1]
        add(path,ps,ps,'P32_SET(',1000000-(pe-ps))
        add(path,le,rs,', P32_GET(%s, %s) %s '%(T,ltext,binop),0)
        add(path,pe,pe,')',0)
        return
    manual.append((rel(path),mr.location.line,k,src[ls:le].decode('latin1')[:80]))

def walk(c,chain):
    chain.append(c)
    if c.kind==CK.MEMBER_REF_EXPR: handle_ref(chain)
    for ch in c.get_children():
        loc=ch.location
        walk(ch,chain)
    chain.pop()

seen_decl=set()
for u in UNITS:
    tu=parse(file=u)
    collect_fields(tu.cursor)
    SITE_CNT.clear()
    print('unit',u,len(targets),file=sys.stderr)
    walk(tu.cursor,[])
for k,f in targets.items():
    decl_edit(f)

rep=open('/tmp/ctr64-tools/report.txt','w')
cnt=collections.Counter(m[2] for m in manual)
rep.write(str(cnt)+'\n')
for m in sorted(set(manual)): rep.write('%s:%d [%s] %s\n'%m)
rep.close()
print('edit files',len(edits),'edits',sum(len(v) for v in edits.values()),file=sys.stderr)
print(cnt)
if APPLY:
    for path,es in edits.items():
        src=ftext(path)
        # Order: process offsets descending; at equal start offsets prefix with smaller order key applied last (so appears first)
        es=sorted(es,key=lambda x:(x[0],x[3]),reverse=True)
        out=bytearray(src)
        for s,e,t,o,_,_ in es:
            out[s:e]=t.encode('latin1')
        open(path,'wb').write(bytes(out))
