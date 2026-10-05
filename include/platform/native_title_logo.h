#ifndef NATIVE_TITLE_LOGO_H
#define NATIVE_TITLE_LOGO_H

#include <macros.h>

// The TURBOCHARGED title under the CTR logo, as real 3D lettering: a bevelled,
// extruded solid ray marched from a distance field of the font
// (assets/fonts/FuzzyBubbles-Bold.ttf) into an offscreen texture, then drawn
// as one straight-alpha quad in the UI ordering table. Works under both
// renderers; the target is only re-rendered when the pose or the output size
// changes. When unavailable, callers keep the 2D plaque.
#if defined(__vita__) || defined(__EMSCRIPTEN__)
#define NATIVE_TITLE_LOGO_SUPPORTED 0
#else
#define NATIVE_TITLE_LOGO_SUPPORTED 1
#endif

struct PrimMem;

struct NativeTitleLogoPose
{
	// Centre of the lettering in UI pixels.
	float centerX;
	float centerY;
	// Width of the lettering in 4:3 UI pixels; widescreen squeezes it.
	float width;
	// Radians. Pitch tips the top of the word towards the viewer, roll
	// turns it anticlockwise on screen.
	float pitch;
	float roll;
};

#if NATIVE_TITLE_LOGO_SUPPORTED
// Loads the font and builds the distance field and shader on first call, so
// call it ahead of the first draw (it takes a few frames' worth of CPU once).
// Returns 0 if the lettering is unavailable.
int NativeTitleLogo_Prepare(void);
// Inserts the lettering into ot. Returns 0 (and draws nothing) if unavailable.
int NativeTitleLogo_Draw(struct PrimMem *primMem, u32 *ot, const struct NativeTitleLogoPose *pose);
void NativeTitleLogo_ReleaseGpu(void);
#else
static inline int NativeTitleLogo_Prepare(void)
{
	return 0;
}
static inline int NativeTitleLogo_Draw(struct PrimMem *primMem, u32 *ot, const struct NativeTitleLogoPose *pose)
{
	(void)primMem;
	(void)ot;
	(void)pose;
	return 0;
}
static inline void NativeTitleLogo_ReleaseGpu(void)
{
}
#endif

#endif
