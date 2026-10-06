"""Fields that point at arrays of retail pointer slots (P32(T **) fields).

    python3 p32_arrays.py FIELD... -- FILE...

Rewrites `P32_GET(T **, ...FIELD)` as `P32_GET(P32(T *) *, ...)` and wraps
direct element accesses: `P32_GET(P32(T *) *, x)[i]` becomes a P32_GET of the
element, or a P32_SET when it is assigned. Locals that hold the array still
need retyping (see alias.py); the compiler reports them."""
import re, sys
sys.path.insert(0, __file__.rsplit('/', 1)[0])
from alias import match_close, stmt_end

def rewrite(text, fields):
	fpat = '|'.join(map(re.escape, fields))
	out = []
	i = 0
	pat = re.compile(r'P32_GET\(((?:struct )?\w+(?: \w+)?) \*\*, ')
	while True:
		m = pat.search(text, i)
		if not m:
			out.append(text[i:])
			return ''.join(out)
		close = match_close(text, m.start() + len('P32_GET'), '(', ')')
		expr = text[m.end():close]
		if not re.search(r'(->|\.)(' + fpat + r')$', expr.strip()):
			out.append(text[i:m.end()])
			i = m.end()
			continue
		out.append(text[i:m.start()])
		T = m.group(1) + ' *'
		inner = rewrite(expr, fields)
		get = 'P32_GET(P32(%s) *, %s)' % (T, inner)
		j = close + 1
		if j < len(text) and text[j] == '[':
			e = match_close(text, j, '[', ']')
			site = get + rewrite(text[j:e + 1], fields)
			am = re.match(r'\s*=(?!=)', text[e + 1:])
			if am:
				s = stmt_end(text, e + 1 + am.end())
				out.append('P32_SET(%s,%s)' % (site, rewrite(text[e + 1 + am.end():s], fields)))
				i = s
			else:
				out.append('P32_GET(%s, %s)' % (T, site))
				i = e + 1
		else:
			out.append(get)
			i = j

if __name__ == '__main__':
	k = sys.argv.index('--')
	fields, files = sys.argv[1:k], sys.argv[k + 1:]
	for f in files:
		t = open(f).read()
		t2 = rewrite(t, fields)
		if t2 != t:
			open(f, 'w').write(t2)
			print(f)
