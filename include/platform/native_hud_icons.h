#ifndef NATIVE_HUD_ICONS_H
#define NATIVE_HUD_ICONS_H

#include <macros.h>
struct Icon;
struct PrimMem;
extern int gNativeModernHudIconsEnabled;
// Untextured native geometry, in the same pixel box as the retail decal.
// Return zero when disabled or out of primitive memory, preserving the bitmap.
int NativeHudIcons_DrawButton(u8 character, const struct Icon *icon, float x, float y, s16 scale, struct PrimMem *primMem, u32 *ot);
int NativeHudIcons_DrawLight(const struct Icon *icon, float x, float y, s16 scale, int green, int lit, struct PrimMem *primMem, u32 *ot);
// Box corners are in the retail quad order, including rotation and widescreen.
int NativeHudIcons_DrawMenuArrow(const float x[4], const float y[4], const u32 colors[4], struct PrimMem *primMem, u32 *ot);
#endif
