"""Turn pointer<->u32 round trips through uintptr_t into handle conversions:
(u32)(uintptr_t)p -> P32_ENC(p), (T *)(uintptr_t)w -> P32_DEC(T *, w).
Lines matching SKIP are left alone (hashes, pointer differences, values that
are never turned back into pointers)."""
import re, sys
sys.path.insert(0, __file__.rsplit('/', 1)[0])
from alias import match_close

SKIP = re.compile(r'hostStackAnchor|debugText|hashIndex|NativeCheckpoint_GetSize|RenderBucket_MipsSub\(|0xff000000')

def operand_end(s, i):
	if s[i] == '&': i += 1
	if s[i] == '(':
		i = match_close(s, i, '(', ')') + 1
	else:
		m = re.match(r'\w+', s[i:]); i += m.end()
	while True:
		if s.startswith('->', i):
			i += 2; i += re.match(r'\w+', s[i:]).end()
		elif s[i] == '.' :
			i += 1; i += re.match(r'\w+', s[i:]).end()
		elif s[i] in '[(':
			i = match_close(s, i, s[i], ']' if s[i] == '[' else ')') + 1
		else:
			return i

ENC = re.compile(r'\((u32|s32|int)\)\(uintptr_t\)')
DEC = re.compile(r'\(((?:const )?(?:struct )?\w+(?: const)?) \*\)\(uintptr_t\)')

def fix_line(line):
	if SKIP.search(line): return line
	for _ in range(20):
		m = ENC.search(line) or DEC.search(line)
		if not m: return line
		e = operand_end(line, m.end())
		op = line[m.end():e]
		if m.re is ENC:
			rep = ('P32_ENC(%s)' if m.group(1) == 'u32' else '(%s)P32_ENC(%%s)' % m.group(1)) % op
		else:
			rep = 'P32_DEC(%s *, %s)' % (m.group(1), op)
		line = line[:m.start()] + rep + line[e:]
	return line

for f in sys.argv[1:]:
	t = open(f).read().split('\n')
	t2 = [fix_line(l) for l in t]
	if t2 != t:
		open(f, 'w').write('\n'.join(t2))
		print(f, sum(a != b for a, b in zip(t, t2)))
