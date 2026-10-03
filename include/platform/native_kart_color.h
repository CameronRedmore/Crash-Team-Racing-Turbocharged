#ifndef NATIVE_KART_COLOR_H
#define NATIVE_KART_COLOR_H

#include <macros.h>

// Experimental > Kart Hue: rotates the paint colour of the human players' karts.
//
// Kart paint lives in the 16-colour palettes (CLUTs) of each driver model's
// texture layouts. When a player's driver model is drawn, the palettes it uses
// are rewritten in VRAM, hue-rotating only the entries inside that character's
// paint band so skin, tyres and metal keep their colours. The retail palette is
// remembered, so the slider can return to the original and VRAM reloads are
// detected.
#if defined(__vita__)
#define NATIVE_KART_COLOR_SUPPORTED 0
#else
#define NATIVE_KART_COLOR_SUPPORTED 1
#endif

#define NATIVE_KART_HUE_STEP_DEGREES 15
#define NATIVE_KART_HUE_STEPS        (360 / NATIVE_KART_HUE_STEP_DEGREES)

// 0 keeps the retail colours; n rotates the hue by n * NATIVE_KART_HUE_STEP_DEGREES.
extern int gNativeKartHue;

struct Instance;
struct TextureLayout;

void NativeKartColor_BeginFrame(void);
void NativeKartColor_OnLayout(const struct Instance *inst, const struct TextureLayout *layout);

#endif
