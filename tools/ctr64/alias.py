"""Rewrite uses of a local alias into an array of P32 handles.

    python3 alias.py FILE FIRST_LINE LAST_LINE VAR 'struct T *'

Within the line range, `VAR[i] = v;` becomes `P32_SET(VAR[i], v);` and every
other `VAR[i]` becomes `P32_GET(struct T *, VAR[i])`. Retype the alias itself
(`P32(struct T *) *VAR`) by hand."""
import re, sys

def match_close(s, i, o, c):
	d = 0
	while i < len(s):
		if s[i] == o: d += 1
		elif s[i] == c:
			d -= 1
			if d == 0: return i
		i += 1
	raise ValueError('unbalanced')

def stmt_end(s, i):
	d = 0
	while i < len(s):
		ch = s[i]
		if ch in '([{': d += 1
		elif ch in ')]}': d -= 1
		elif ch == ';' and d == 0: return i
		i += 1
	raise ValueError('no ;')

def rewrite(text, var, T):
	out = []
	pat = re.compile(r'(?<![\w.>])' + re.escape(var) + r'\s*\[')
	i = 0
	while True:
		m = pat.search(text, i)
		if not m:
			out.append(text[i:])
			return ''.join(out)
		out.append(text[i:m.start()])
		close = match_close(text, m.end() - 1, '[', ']')
		idx = rewrite(text[m.end():close], var, T)
		site = '%s[%s]' % (var, idx)
		rest = text[close + 1:]
		am = re.match(r'\s*=(?!=)', rest)
		if am:
			e = stmt_end(text, close + 1 + am.end())
			rhs = rewrite(text[close + 1 + am.end():e], var, T)
			out.append('P32_SET(%s,%s)' % (site, rhs))
			i = e
		else:
			out.append('P32_GET(%s, %s)' % (T, site))
			i = close + 1

if __name__ == '__main__':
	path, a, b, var, T = sys.argv[1:6]
	a, b = int(a), int(b)
	lines = open(path).read().split('\n')
	mid = rewrite('\n'.join(lines[a - 1:b]), var, T)
	open(path, 'w').write('\n'.join(lines[:a - 1] + [mid] + lines[b:]))
