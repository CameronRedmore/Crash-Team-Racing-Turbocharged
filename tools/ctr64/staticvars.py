import sys,re,collections
sys.path.insert(0,'/tmp/ctr64-tools')
from common import *
tu=parse()
errs=collections.defaultdict(list)
for l in open('/tmp/b64.log'):
    m=re.match(r'.*64bit/(.*?):(\d+):(\d+): error: (.*)',l)
    if m: errs[m.group(1)].append((int(m.group(2)),m.group(4)))
res=collections.Counter(); detail={}
def walk(c):
    for ch in c.get_children():
        if ch.kind==ci.CursorKind.VAR_DECL and ch.location.file and in_project(ch.location.file.name):
            f=os.path.relpath(os.path.realpath(ch.location.file.name),ROOT)
            if f in errs:
                a,b=ch.extent.start.line,ch.extent.end.line
                n=[e for (ln,e) in errs[f] if a<=ln<=b]
                if n: res[(f,ch.spelling,a,b)]=len(n)
        if ch.kind in (ci.CursorKind.TRANSLATION_UNIT,): walk(ch)
walk(tu.cursor)
tot=0
for k,v in sorted(res.items(),key=lambda x:-x[1]): print(v,k); tot+=v
print('total',tot,'of',sum(len(v) for v in errs.values()))
