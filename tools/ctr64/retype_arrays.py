"""Retype locals that hold arrays of 4-byte pointer slots (ICONGROUP_GETICONS,
ST1_GETPOINTERS, ANIMTEX_GETARRAY, P32(T **) fields) as P32(T) * and rewrite
their element accesses. Reads the clang "incompatible pointer types ... from
'CtrPtr32 *'" errors from /tmp/b64.log. `*var` and `var++` walks are left for
hand fixing; the next build reports them."""
import re, sys
sys.path.insert(0, __file__.rsplit('/', 1)[0])
from alias import rewrite


sites = set()
for l in open('/tmp/b64.log'):
	m = re.match(r'.*64bit/(.*?):(\d+):\d+: error: incompatible pointer types (?:initializing|assigning to) .* \'CtrPtr32 \*\'', l)
	if m: sites.add((m.group(1), int(m.group(2))))

todo = {}
for f, n in sorted(sites):
	lines = open(f).read().split('\n')
	src = lines[n - 1]
	m = re.search(r'(?:((?:const )?(?:struct \w+|void|char)) \*\*)?(\w+) = ', src)
	if not m:
		print('skip', f, n, src.strip()); continue
	var = m.group(2)
	# enclosing function: nearest column-0 braces
	a = n - 1
	while not lines[a].startswith('{'): a -= 1
	b = n - 1
	while not lines[b].startswith('}'): b += 1
	if not any(x[:3] == (a, b, var) for x in todo.get(f, [])):
		todo.setdefault(f, []).append((a, b, var, m.group(1)))

for f, items in todo.items():
	lines = open(f).read().split('\n')
	for a, b, var, decltype in items:
		body = '\n'.join(lines[a:b + 1])
		if decltype is None:
			dm = re.search(r'((?:const )?(?:struct \w+|void|char)) \*\*' + var + r'\b', body)
			decltype = dm.group(1)
		T = decltype + ' *'
		body = re.sub(r'\b' + re.escape(decltype) + r' \*\*' + var + r'\b', 'P32(%s) *%s' % (T, var), body)
		body = rewrite(body, var, T)
		lines[a:b + 1] = body.split('\n')
	open(f, 'w').write('\n'.join(lines))
	print(f, len(items))
