import sys
from collections import Counter
a=open(sys.argv[1]).read().splitlines()
b=open(sys.argv[2]).read().splitlines()
c=Counter(); first={}
n=min(len(a),len(b))
# drop a possibly truncated final line
for i,(x,y) in enumerate(zip(a[:n-1],b[:n-1])):
    for p,q in zip(x.split(),y.split()):
        if p!=q:
            k=p.split('=')[0]; c[k]+=1; first.setdefault(k,(i,p,q))
print(len(a),len(b),dict(c))
for k,v in sorted(first.items(), key=lambda t:t[1][0])[:8]: print(' ',k,v)
