import os
os.chdir('/home/cameron/Games/CTR Turbocharged/src/worktrees/64bit')
def sub(f,a,b,count=1):
    t=open(f).read(); assert t.count(a)>=1,(f,a); open(f,'w').write(t.replace(a,b,count))
sub('include/macros.h','#define AugReview 805','#include <ctr_ptr32.h>\n\n#define AugReview 805')
sub('platform/native_memory.c','global_variable char s_mempackMemory','''#if defined(CTR_NATIVE_64BIT)
// Origin for CtrPtr32 handles. Nothing may ever be allocated at offset 0.
char gCtrPtr32Anchor[64];

void CtrPtr32_RangeError(uintptr_t p)
{
	fprintf(stderr, "CtrPtr32: pointer %p is outside +-2 GiB of the image\\n", (void *)p);
	abort();
}
#endif

global_variable char s_mempackMemory''')
rep=[('include/namespace_Load.h','void (*callbackFuncPtr)(struct LoadQueueSlot *);','P32_FNPTR(void, callbackFuncPtr, (struct LoadQueueSlot *));'),
('include/namespace_RectMenu.h','void (*funcPtr)(struct RectMenu *m);','P32_FNPTR(void, funcPtr, (struct RectMenu *m));'),
('include/ovr_230.h','void (*handler)(void);','P32_FNPTR(void, handler, (void));'),
('include/regionsEXE.h','void (*LInB)(struct Instance *inst);','P32_FNPTR(void, LInB, (struct Instance *inst));'),
('include/regionsEXE.h','int (*LInC)(struct Instance *i, struct Thread *t, struct ScratchpadStruct *sps);','P32_FNPTR(int, LInC, (struct Instance *i, struct Thread *t, struct ScratchpadStruct *sps));'),
('include/regionsEXE.h','void (*opcodeFunc[0xb])(struct SongSeq *);','P32_FNPTR(void, opcodeFunc[0xb], (struct SongSeq *));'),
('include/regionsEXE.h','void (*callbackCdReadSuccess)(struct LoadQueueSlot *);','P32_FNPTR(void, callbackCdReadSuccess, (struct LoadQueueSlot *));')]
for f,a,b in rep: sub(f,a,b)
