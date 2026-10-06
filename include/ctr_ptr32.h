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
// Small integers in [-CTR_P32_SENTINEL_MAX, CTR_P32_SENTINEL_MAX] are stored
// as themselves, so sentinel "pointers" such as (void (*)(...))-2 survive a
// round trip. The origin sits in the middle of gCtrPtr32Anchor, so no real
// object is that close to it.
//
//   P32(T)             declares a pointer field of type T
//   P32_FNPTR(ret, name, (args))  declares a function pointer field
//   P32_GET(T, lv)     reads lv as a pointer of type T
//   P32_SET(lv, v)     stores pointer v into lv
//   P32_ENC / P32_DEC  convert a raw pointer to/from a handle stored in an int
//   P32_TRY_ENC(v, out) like P32_ENC, but returns 0 instead of aborting when v
//                      cannot be a handle
//
// T is the full pointer type, e.g. P32_GET(struct Model *, inst->model).

#if defined(CTR_NATIVE_64BIT)

typedef struct CtrPtr32
{
	uint32_t h;
} CtrPtr32;

#define CTR_P32_SENTINEL_MAX 16
#define CTR_P32_ORIGIN       ((uintptr_t)&gCtrPtr32Anchor.bytes[32])

// 8-byte aligned, so handle low bits match pointer low bits (alignment tests
// and low-bit tags work on handles).
typedef union CtrPtr32Anchor
{
	char bytes[64];
	uint64_t align[8];
} CtrPtr32Anchor;
extern CtrPtr32Anchor gCtrPtr32Anchor;
void CtrPtr32_RangeError(uintptr_t p);

static inline uint32_t ctr_p32_enc(uintptr_t p)
{
	if (p + CTR_P32_SENTINEL_MAX <= 2 * CTR_P32_SENTINEL_MAX)
	{
		return (uint32_t)p;
	}
	intptr_t d = (intptr_t)(p - CTR_P32_ORIGIN);
	if (d != (intptr_t)(int32_t)d)
	{
		CtrPtr32_RangeError(p);
	}
	return (uint32_t)(int32_t)d;
}

static inline int ctr_p32_try_enc(uintptr_t p, uint32_t *out)
{
	intptr_t d = (intptr_t)(p - CTR_P32_ORIGIN);
	if (p + CTR_P32_SENTINEL_MAX <= 2 * CTR_P32_SENTINEL_MAX)
	{
		*out = (uint32_t)p;
		return 1;
	}
	if (d != (intptr_t)(int32_t)d)
	{
		return 0;
	}
	*out = (uint32_t)(int32_t)d;
	return 1;
}

static inline uintptr_t ctr_p32_dec(uint32_t h)
{
	uintptr_t v = (uintptr_t)(intptr_t)(int32_t)h;
	return (h + CTR_P32_SENTINEL_MAX <= 2 * CTR_P32_SENTINEL_MAX) ? v : CTR_P32_ORIGIN + v;
}

#define P32(T)         CtrPtr32
#define P32_GET(T, lv) ((T)ctr_p32_dec((lv).h))
#define P32_SET(lv, v) ((lv).h = ctr_p32_enc((uintptr_t)(v)))
#define P32_FNPTR(ret, name, args) CtrPtr32 name
// Static initializers cannot hold handles (they are not link-time constants).
// P32_DEFER() zeroes the field and the value is stored at startup instead by a
// CTR_P32_STATIC_FIXUP block placed after the definition (see tools/ctr64).
#define P32_DEFER(e)   0
// Objects patched by startup fixups cannot live in read-only memory.
#define CTR_P32_MUTABLE
#if defined(_MSC_VER) && !defined(__clang__)
#define CTR_P32_STATIC_FIXUP(name)                                                                        \
	static void CtrP32Fixup_##name(void);                                                                  \
	static int CtrP32FixupRun_##name(void)                                                                 \
	{                                                                                                      \
		CtrP32Fixup_##name();                                                                              \
		return 0;                                                                                          \
	}                                                                                                      \
	__pragma(section(".CRT$XIU", read)) __declspec(allocate(".CRT$XIU")) static int (*CtrP32FixupPtr_##name)(void) = CtrP32FixupRun_##name; \
	static void CtrP32Fixup_##name(void)
#else
#define CTR_P32_STATIC_FIXUP(name) static void __attribute__((constructor)) CtrP32Fixup_##name(void)
#endif
#define P32_ENC(v)     ctr_p32_enc((uintptr_t)(v))
#define P32_DEC(T, h)  ((T)ctr_p32_dec((uint32_t)(h)))
#define P32_TRY_ENC(v, out) ctr_p32_try_enc((uintptr_t)(v), (out))

#else

#define P32(T)         T
#define P32_GET(T, lv) ((T)(lv))
#define P32_SET(lv, v) ((lv) = (v))
#define P32_FNPTR(ret, name, args) ret (*name) args
#define P32_DEFER(e)   (e)
#define CTR_P32_MUTABLE const
#define P32_ENC(v)     ((uint32_t)(uintptr_t)(v))
#define P32_DEC(T, h)  ((T)(uintptr_t)(h))
#define P32_TRY_ENC(v, out) (*(out) = (uint32_t)(uintptr_t)(v), 1)

#endif

#endif
