#ifndef CTR_PTR32_H
#define CTR_PTR32_H

#include <stdint.h>

// Retail-shaped structs store 4-byte pointers. On 32-bit builds P32() is just
// the pointer type. On 64-bit builds it is a 32-bit handle holding a signed
// offset from gCtrPtr32Anchor, so struct layouts, file-patched pointers and
// hard-coded offsets keep working without needing low memory (MAP_32BIT,
// -no-pie, __ptr32), which are not portable to macOS, ARM64 or Windows.
//
// Every object a handle can name must live in the executable image (statics,
// string literals, functions, the mempack arena) so it is within +-2 GiB of the
// anchor. Handle 0 is NULL.
//
//   P32(T)             declares a pointer field of type T
//   P32_FNPTR(ret, name, (args))  declares a function pointer field
//   P32_GET(T, lv)     reads lv as a pointer of type T
//   P32_SET(lv, v)     stores pointer v into lv
//   P32_ENC / P32_DEC  convert a raw pointer to/from a handle stored in an int
//
// T is the full pointer type, e.g. P32_GET(struct Model *, inst->model).

#if defined(CTR_NATIVE_64BIT)

typedef struct CtrPtr32
{
	uint32_t h;
} CtrPtr32;

extern char gCtrPtr32Anchor[64];
void CtrPtr32_RangeError(uintptr_t p);

static inline uint32_t ctr_p32_enc(uintptr_t p)
{
	if (p == 0)
	{
		return 0;
	}
	intptr_t d = (intptr_t)(p - (uintptr_t)&gCtrPtr32Anchor[0]);
	if (d != (intptr_t)(int32_t)d)
	{
		CtrPtr32_RangeError(p);
	}
	return (uint32_t)(int32_t)d;
}

static inline uintptr_t ctr_p32_dec(uint32_t h)
{
	return (h == 0) ? 0 : (uintptr_t)&gCtrPtr32Anchor[0] + (uintptr_t)(intptr_t)(int32_t)h;
}

#define P32(T)         CtrPtr32
#define P32_GET(T, lv) ((T)ctr_p32_dec((lv).h))
#define P32_SET(lv, v) ((lv).h = ctr_p32_enc((uintptr_t)(v)))
#define P32_FNPTR(ret, name, args) CtrPtr32 name
#define P32_ENC(v)     ctr_p32_enc((uintptr_t)(v))
#define P32_DEC(T, h)  ((T)ctr_p32_dec((uint32_t)(h)))

#else

#define P32(T)         T
#define P32_GET(T, lv) (lv)
#define P32_SET(lv, v) ((lv) = (v))
#define P32_FNPTR(ret, name, args) ret (*name) args
#define P32_ENC(v)     ((uint32_t)(uintptr_t)(v))
#define P32_DEC(T, h)  ((T)(uintptr_t)(h))

#endif

#endif
