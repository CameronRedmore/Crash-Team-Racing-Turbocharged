import sys
from collections import Counter
# Compare two --ab-trace files. Bucket fields (bN=count:thread:instFlags:matrix:modelIndex)
# are split so the known-benign stale Thread.modelIndex hash (bN.mi) is reported apart.
BUCKET_SUB = ['count', 'thread', 'inst', 'mat', 'mi']
a=open(sys.argv[1]).read().splitlines()
b=open(sys.argv[2]).read().splitlines()
c=Counter(); first={}
n=min(len(a),len(b))
# drop a possibly truncated final line
for i,(x,y) in enumerate(zip(a[:n-1],b[:n-1])):
    for p,q in zip(x.split(),y.split()):
        if p==q:
            continue
        k=p.split('=')[0]
        keys=[k]
        if k[:1]=='b' and k[1:].isdigit():
            pp=p.split('=',1)[1].split(':'); qq=q.split('=',1)[1].split(':')
            keys=[k+'.'+(BUCKET_SUB[j] if j<len(BUCKET_SUB) else str(j))
                  for j in range(max(len(pp),len(qq))) if pp[j:j+1]!=qq[j:j+1]]
        for kk in keys:
            c[kk]+=1; first.setdefault(kk,(i,p,q))
print(len(a),len(b),dict(c))
real=[k for k in c if not k.endswith('.mi')]
print('MATCH (ignoring .mi)' if not real else 'DIFF: '+' '.join(sorted(real)))
for k,v in sorted(first.items(), key=lambda t:t[1][0])[:8]: print(' ',k,v)
