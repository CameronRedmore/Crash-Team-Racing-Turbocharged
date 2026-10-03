import os
os.chdir('/home/cameron/Games/CTR Turbocharged/src/worktrees/64bit')
p='include/ctr_ptr32.h'; s=open(p).read()
s=s.replace('''#define P32_FNPTR(ret, name, args) CtrPtr32 name
''','''#define P32_FNPTR(ret, name, args) CtrPtr32 name
// Static initializers cannot hold handles (they are not link-time constants).
// P32_DEFER() zeroes the field and the value is stored at startup instead by a
// CTR_P32_STATIC_FIXUP block placed after the definition (see tools/ctr64).
#define P32_DEFER(e)   0
// Objects patched by startup fixups cannot live in read-only memory.
#define CTR_P32_MUTABLE
#if defined(_MSC_VER) && !defined(__clang__)
#define CTR_P32_STATIC_FIXUP(name)                                                                        \\
	static void CtrP32Fixup_##name(void);                                                                  \\
	static int CtrP32FixupRun_##name(void)                                                                 \\
	{                                                                                                      \\
		CtrP32Fixup_##name();                                                                              \\
		return 0;                                                                                          \\
	}                                                                                                      \\
	__pragma(section(".CRT$XIU", read)) __declspec(allocate(".CRT$XIU")) static int (*CtrP32FixupPtr_##name)(void) = CtrP32FixupRun_##name; \\
	static void CtrP32Fixup_##name(void)
#else
#define CTR_P32_STATIC_FIXUP(name) static void __attribute__((constructor)) CtrP32Fixup_##name(void)
#endif
''',1)
s=s.replace('''#define P32_FNPTR(ret, name, args) ret (*name) args
''','''#define P32_FNPTR(ret, name, args) ret (*name) args
#define P32_DEFER(e)   (e)
#define CTR_P32_MUTABLE const
''',1)
open(p,'w').write(s)
