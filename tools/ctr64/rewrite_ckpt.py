"""Second pass: convert remaining pointer-field accesses (macro-argument sites etc.).
Parses the 32-bit-shaped AST (no CTR_NATIVE_64BIT) where P32(T) is T, so
already-converted sites are recognised by their P32_GET/P32_SET macro parens."""
import sys, collections, re, ctypes
sys.path.insert(0,'/tmp/ctr64-tools')
import common as _c
from common import *
_orig=_c.args_for
def args_for(build='/tmp/b64',file='main.c'):
    return [x for x in _orig(build,file) if x!='-DCTR_NATIVE_64BIT']
_c.args_for=args_for
sys.setrecursionlimit(100000)
CK=ci.CursorKind
APPLY='--apply' in sys.argv
SKIP_FILES=set()
ONLY='platform/native_checkpoint.c'
UNITS=['main.c','tests/native_framerate_test.c','tests/native_precision_test.c','tests/native_physics_test.c','tests/native_draw3d_test.c','platform/native_draw3d.c','platform/native_pgxp.c','platform/native_gte_core.c','platform/native_inline_c.c']
_f=ci.conf.lib.clang_getSpellingLocation
_f.argtypes=[ci.SourceLocation, ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint)]
_f.restype=None
def spell(loc):
    fi=ctypes.c_void_p(); l=ctypes.c_uint(); c=ctypes.c_uint(); o=ctypes.c_uint()
    _f(loc,ctypes.byref(fi),ctypes.byref(l),ctypes.byref(c),ctypes.byref(o))
    return l.value,c.value,o.value,fi.value
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
def key(cur):
    l=cur.location; return (rel(l.file.name),l.line,l.column)
edits=collections.defaultdict(set); manual=[]; files_text={}
def ftext(path):
    if path not in files_text: files_text[path]=open(path,'rb').read()
    return files_text[path]
CUR_TAG=[None]; SITE_CNT=collections.Counter()
def add(path,start,end,text,order):
    # A macro that expands its argument several times visits the same spelled
    # site more than once; the set dedupes those so the site is wrapped once.
    edits[path].add((start,end,text,order,CUR_TAG[0]))
targets={}
def collect_fields(c):
    for ch in c.get_children():
        if ch.kind==CK.FIELD_DECL and ch.location.file and in_project(ch.location.file.name) and convert_scope(ch.location.file.name) and has_ptr(ch.type):
            targets[key(ch)]=ch
        if ch.kind in (CK.STRUCT_DECL,CK.UNION_DECL): collect_fields(ch)
STMT_PARENTS={CK.COMPOUND_STMT,CK.IF_STMT,CK.FOR_STMT,CK.WHILE_STMT,CK.DO_STMT,CK.CASE_STMT,CK.DEFAULT_STMT,CK.LABEL_STMT,CK.SWITCH_STMT}
def classify(chain,idx):
    i=idx; child=chain[i]; i-=1
    while i>=0 and chain[i].kind in (CK.UNEXPOSED_EXPR,CK.PAREN_EXPR):
        child=chain[i]; i-=1
    return child,(chain[i] if i>=0 else None),i
def paren_wrap_kind(chain,idx,mfile):
    """True if the expression at chain[idx] is already wrapped by a P32_GET/P32_SET paren."""
    i=idx-1
    while i>=0 and chain[i].kind==CK.UNEXPOSED_EXPR: i-=1
    if i>=0 and chain[i].kind==CK.PAREN_EXPR:
        sf=spell(chain[i].extent.start)[3]
        if sf!=mfile: return True
    return False
def handle_ref(chain):
    mr=chain[-1]
    r=mr.referenced
    if r is None or key(r) not in targets: return
    f=targets[key(r)]
    if mr.location.file is None or not in_project(mr.location.file.name): return
    path=os.path.realpath(mr.location.file.name)
    if rel(path) != ONLY: return
    src=ftext(path)
    s0=spell(mr.extent.start); e0=spell(mr.extent.end)
    mfile=s0[3]
    es=mr.extent.start.offset
    macro=(s0[2]!=es)
    def spelled_span(cur,lastlen,lasttok):
        a=spell(cur.extent.start); b=spell(cur.extent.end)
        E=b[2]
        if src[E-lastlen:E].decode('latin1')==lasttok: e=E
        elif src[E:E+lastlen].decode('latin1')==lasttok: e=E+lastlen
        else: e=-1
        return a[2], e, a[3]==mfile and b[3]==mfile
    L=mr; Lidx=len(chain)-1
    for _ in range(arr_rank(f.type)):
        child,par,pi=classify(chain,Lidx)
        if par is not None and par.kind==CK.ARRAY_SUBSCRIPT_EXPR:
            pc=list(par.get_children())
            if pc and spell(pc[0].extent.start)[2]==spell(L.extent.start)[2] and spell(pc[0].extent.end)[2]==spell(L.extent.end)[2]:
                L=par; Lidx=pi; continue
        manual.append((rel(path),mr.location.line,'array decay',f.spelling)); return
    # already wrapped?
    cchild,cpar,cpi=classify(chain,Lidx)
    if paren_wrap_kind(chain,Lidx,mfile) : return
    if cpar is not None and cpar.kind==CK.PAREN_EXPR and spell(cpar.extent.start)[3]!=mfile: return
    # compute spelled span of L
    last=1 if L is not mr else len(f.spelling)
    ls,le,ok=spelled_span(L,last,']' if L is not mr else f.spelling)
    if not ok: manual.append((rel(path),mr.location.line,'macro body site',f.spelling)); return
    if le<0:
        manual.append((rel(path),mr.location.line,'bad span',src[ls:ls+60].decode('latin1'))); return
    CUR_TAG[0]=(ls,le)
    T=L.type.spelling
    if 'unnamed' in T or 'anonymous' in T: manual.append((rel(path),mr.location.line,'anon type site',f.spelling)); return
    child,par,pi=classify(chain,Lidx)
    kind='get'
    if par is not None:
        if par.kind==CK.BINARY_OPERATOR:
            ch=list(par.get_children())
            if len(ch)==2 and spell(ch[0].extent.start)[2]==spell(child.extent.start)[2] and spell(ch[0].extent.end)[2]==spell(child.extent.end)[2]:
                toks=[t.spelling for t in par.get_tokens()]
                # operator: token after lhs; detect '=' by scanning source between lhs end and rhs start
                lend=spell(ch[0].extent.end)[2]; rstart=spell(ch[1].extent.start)[2]
                seg=src[lend:rstart].decode('latin1')
                m=re.search(r'[^\s\w\])]*[=][^=]',seg)
                opseg=seg
                if re.search(r'(^|[^=!<>+\-*/%&|^])=($|[^=])',re.sub(r'\w+|\s+|[\])]','',seg)+' ') and ('==' not in seg) :
                    kind=('set',par,ch[1])
        elif par.kind==CK.COMPOUND_ASSIGNMENT_OPERATOR:
            kind=('compound',par)
        elif par.kind==CK.UNARY_OPERATOR:
            toks=list(par.get_tokens()); op=toks[0].spelling if toks else ''
            if op=='&': kind=('addr',par)
            elif op in ('++','--') or (toks and toks[-1].spelling in ('++','--')): kind=('incdec',par)
        elif par.kind==CK.CXX_UNARY_EXPR:
            toks=list(par.get_tokens())
            if toks and toks[0].spelling=='sizeof': return
    if kind=='get':
        add(path,ls,ls,'P32_GET(%s, '%T,1000000-(le-ls)); add(path,le,le,')',0); return
    if kind[0]=='set':
        if macro: manual.append((rel(path),mr.location.line,'set in macro arg',f.spelling)); return
        # not in macro: delegate to first-pass logic equivalent
        manual.append((rel(path),mr.location.line,'set unconverted',f.spelling)); return
    manual.append((rel(path),mr.location.line,kind[0],src[ls:le].decode('latin1')[:80]))
def walk(c,chain):
    chain.append(c)
    if c.kind==CK.MEMBER_REF_EXPR: handle_ref(chain)
    for ch in c.get_children(): walk(ch,chain)
    chain.pop()
for u in UNITS:
    tu=_c.parse(file=u)
    collect_fields(tu.cursor)
    SITE_CNT.clear()
    walk(tu.cursor,[])
rep=open('/tmp/ctr64-tools/report_ckpt.txt','w')
cnt=collections.Counter(m[2] for m in manual)
rep.write(str(cnt)+'\n')
for m in sorted(set(manual)): rep.write('%s:%d [%s] %s\n'%m)
rep.close()
print('edits',sum(len(v) for v in edits.values()),'files',len(edits)); print(cnt)
if APPLY:
    for path,es in edits.items():
        src=ftext(path)
        es=sorted(es,key=lambda x:(x[0],x[3]),reverse=True)
        out=bytearray(src)
        for s,e,t,o,_ in es: out[s:e]=t.encode('latin1')
        open(path,'wb').write(bytes(out))
