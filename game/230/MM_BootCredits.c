#include <common.h>
#include <math.h>

#if defined(CTR_NATIVE)
#include <platform/native_assets.h>
#include <platform/native_title_logo.h>
#endif

#if defined(CTR_NATIVE)
// Turbocharged credits: two title cards on black before the first title intro
// of a session, then a fade into the retail Crash + C-T-R animation. The menu
// level is already loaded underneath, so the cards hold back MM_Title_Init and
// the menu song until they finish. Like the preset menu, they can't be
// skipped until they have played to the end once (config key credits_seen);
// after that Start skips them.

struct MM_BootCreditsLine
{
	const char *text;
	s16 y;
	s16 font;
	s16 color;
	b32 reveal;  // letter by letter, each igniting white before settling
	float start; // seconds into the card
};

struct MM_BootCreditsCard
{
	float start;   // seconds into the sequence
	float fadeOut; // seconds into the card that the fade out begins
	const struct MM_BootCreditsLine *lines;
	int count;
};

enum
{
	MM_BOOT_CREDITS_PENDING,
	MM_BOOT_CREDITS_RUNNING,
	MM_BOOT_CREDITS_DONE,
};

#define MM_BOOT_CREDITS_LINE_FADE         1.0f
#define MM_BOOT_CREDITS_CARD_FADE         1.0f
#define MM_BOOT_CREDITS_LETTER_GAP        0.09f
#define MM_BOOT_CREDITS_LETTER_FADE       0.5f
#define MM_BOOT_CREDITS_IGNITE            0.7f
#define MM_BOOT_CREDITS_END               21.7f
#define MM_BOOT_CREDITS_SKIP_INPUT        BTN_START

// Retail fades the screen in at 0x88 per 30 FPS frame; this takes 24 frames.
#define MM_BOOT_CREDITS_TITLE_FADE_FRAMES 24

// Logo centre in 4:3 PS1 pixels; widescreen squeezes it towards the middle.
#define MM_BOOT_CREDITS_LOGO_X            160
#define MM_BOOT_CREDITS_PLAQUE_Y          188
#define MM_BOOT_CREDITS_CORNER_PAD        6

static const struct MM_BootCreditsLine s_mmBootCreditsNative[] = {
    {"SPECIAL THANKS TO", 56, FONT_SMALL, PERIWINKLE, false, 0.0f},         {"CTR-NATIVE", 70, FONT_BIG, ORANGE, true, 1.0f},
    {"FOR THE DECOMPILATION OF", 104, FONT_SMALL, WHITE, false, 2.8f},      {"CRASH TEAM RACING", 115, FONT_SMALL, WHITE, false, 3.1f},
    {"THE MAIN REASON ANY OF THESE", 138, FONT_SMALL, ORANGE, false, 5.0f}, {"CHANGES ARE POSSIBLE", 149, FONT_SMALL, ORANGE, false, 5.3f},
};

static const struct MM_BootCreditsLine s_mmBootCreditsHighOctane[] = {
    {"AND THANKS TO", 50, FONT_SMALL, PERIWINKLE, false, 0.0f},
    {"CTR HIGH OCTANE", 64, FONT_BIG, ORANGE, true, 1.0f},
    {"FOR SHOWING HOW EDITS", 98, FONT_SMALL, WHITE, false, 2.8f},
    {"COULD BE INTEGRATED", 109, FONT_SMALL, WHITE, false, 3.1f},
    {"AND HOW ENHANCEMENTS SUCH AS", 132, FONT_SMALL, ORANGE, false, 5.0f},
    {"WIDESCREEN AND HIGHER FPS", 143, FONT_SMALL, ORANGE, false, 5.3f},
    {"COULD BE MADE", 154, FONT_SMALL, ORANGE, false, 5.6f},
};

static const struct MM_BootCreditsCard s_mmBootCreditsCards[] = {
    {1.5f, 8.0f, s_mmBootCreditsNative, sizeof(s_mmBootCreditsNative) / sizeof(s_mmBootCreditsNative[0])},
    {11.1f, 9.0f, s_mmBootCreditsHighOctane, sizeof(s_mmBootCreditsHighOctane) / sizeof(s_mmBootCreditsHighOctane[0])},
};

extern void save_config();

int gNativeCreditsSeen = 0;

static int s_mmBootCreditsState = MM_BOOT_CREDITS_PENDING;
static float s_mmBootCreditsSeconds;
static b32 s_mmBootCreditsStoppedMusic;

static float MM_BootCredits_Ramp(float t, float start, float duration)
{
	float x = (t - start) / duration;

	if (x <= 0.0f)
	{
		return 0.0f;
	}
	if (x >= 1.0f)
	{
		return 1.0f;
	}
	return x * x * (3.0f - 2.0f * x);
}

// Writes the CREDITS_FADE slot, as the ending credits do, so each draw call
// can carry its own brightness. Text is drawn immediately, so one slot serves
// every line of the frame.
static void MM_BootCredits_SetFadeColor(int fromSlot, int toSlot, float mix, float level)
{
	const u8 *from = (const u8 *)P32_GET(u32 *, data.ptrColor[fromSlot]);
	const u8 *to = (const u8 *)P32_GET(u32 *, data.ptrColor[toSlot]);
	u8 *dst = (u8 *)&data.colors[CREDITS_FADE][0];

	for (int i = 0; i < 16; i++)
	{
		if ((i & 3) == 3)
		{
			dst[i] = to[i];
			continue;
		}
		const float c = (float)from[i] + ((float)to[i] - (float)from[i]) * mix;
		dst[i] = (u8)(c * level + 0.5f);
	}
}

static void MM_BootCredits_DrawLine(const struct MM_BootCreditsLine *line, float t, float cardLevel)
{
	char *text = (char *)line->text;

	if (!line->reveal)
	{
		const float level = MM_BootCredits_Ramp(t, line->start, MM_BOOT_CREDITS_LINE_FADE) * cardLevel;

		if (level <= 0.0f)
		{
			return;
		}
		MM_BootCredits_SetFadeColor(line->color, line->color, 1.0f, level);
		DecalFont_DrawLine(text, 0x100, line->y, line->font, JUSTIFY_CENTER | CREDITS_FADE);
		return;
	}

	const int len = (int)strlen(text);
	const int left = 0x100 - DecalFont_GetLineWidth(text, line->font) / 2;

	for (int i = 0; i < len; i++)
	{
		if (text[i] == ' ')
		{
			continue;
		}

		const float letterStart = line->start + MM_BOOT_CREDITS_LETTER_GAP * (float)i;
		const float level = MM_BootCredits_Ramp(t, letterStart, MM_BOOT_CREDITS_LETTER_FADE) * cardLevel;

		if (level <= 0.0f)
		{
			continue;
		}

		const float settle = MM_BootCredits_Ramp(t, letterStart + MM_BOOT_CREDITS_LETTER_FADE * 0.5f, MM_BOOT_CREDITS_IGNITE);
		MM_BootCredits_SetFadeColor(WHITE, line->color, settle, level);
		DecalFont_DrawLineStrlen(&text[i], 1, left + DecalFont_GetLineWidthStrlen(text, i, line->font), line->y, line->font, CREDITS_FADE);
	}
}

static void MM_BootCredits_Draw(struct GameTracker *gGT)
{
	const float t = s_mmBootCreditsSeconds;

	for (int c = 0; c < (int)(sizeof(s_mmBootCreditsCards) / sizeof(s_mmBootCreditsCards[0])); c++)
	{
		const struct MM_BootCreditsCard *card = &s_mmBootCreditsCards[c];
		const float cardT = t - card->start;

		if ((cardT < 0.0f) || (cardT > card->fadeOut + MM_BOOT_CREDITS_CARD_FADE))
		{
			continue;
		}

		const float cardLevel = 1.0f - MM_BootCredits_Ramp(cardT, card->fadeOut, MM_BOOT_CREDITS_CARD_FADE);

		for (int i = 0; i < card->count; i++)
		{
			MM_BootCredits_DrawLine(&card->lines[i], cardT, cardLevel);
		}
	}

	if (gNativeCreditsSeen)
	{
		// Fades in, then breathes gently.
		const float pulse = 0.55f + 0.2f * sinf(t * 2.5f);
		MM_BootCredits_SetFadeColor(WHITE, WHITE, 1.0f, MM_BootCredits_Ramp(t, 0.5f, MM_BOOT_CREDITS_LINE_FADE) * pulse);
		DecalFont_DrawLine((char *)"PRESS START TO SKIP", gGT->pushBuffer_UI.rect.w - MM_BOOT_CREDITS_CORNER_PAD,
		                   gGT->pushBuffer_UI.rect.h - MM_BOOT_CREDITS_CORNER_PAD - data.font_charPixHeight[FONT_SMALL], FONT_SMALL,
		                   JUSTIFY_RIGHT | CREDITS_FADE);
	}

	// OT insertion prepends: the backdrop renders first, behind the text.
	// The loading flag draws in the slot in front of the UI, so it still
	// lifts over the black.
	RECT backdrop = {0, 0, gGT->pushBuffer_UI.rect.w, gGT->pushBuffer_UI.rect.h};
	CTR_Box_DrawSolidBox(&backdrop, MakeColor(0, 0, 0), P32_GET(uint32_t *, P32_GET(struct DB *, gGT->backBuffer)->otMem.uiOT));
}

static void MM_BootCredits_Finish(struct GameTracker *gGT)
{
	s_mmBootCreditsState = MM_BOOT_CREDITS_DONE;

	// The title fades in from the black the cards ended on.
	gGT->pushBuffer_UI.fadeFromBlack_currentValue = 0;
	gGT->pushBuffer_UI.fadeFromBlack_desiredResult = 0x1000;
	gGT->pushBuffer_UI.fade_step = (s16)(0x1000 / FPS_DOUBLE(MM_BOOT_CREDITS_TITLE_FADE_FRAMES) + 1);

	if (s_mmBootCreditsStoppedMusic)
	{
		// Same start as the AUDIO_GARAGE_ENTRY state that loading set.
		Music_Adjust(0, 0, NULL, 0);
	}
}

static void MM_TitleLogo_Prepare(void);

// Returns true while the credits own the main menu, so MM_MenuProc_Main
// skips the title. Runs once per frame on the menu's update call.
static b32 MM_BootCredits_Update(struct RectMenu *mainMenu)
{
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);

	// Built once, behind the credits or the logo's fade in.
	MM_TitleLogo_Prepare();

	if (s_mmBootCreditsState == MM_BOOT_CREDITS_DONE)
	{
		return false;
	}

	if (s_mmBootCreditsState == MM_BOOT_CREDITS_PENDING)
	{
		// Only in front of the session's first title intro.
		if ((D230.titleMenuState != TITLE_MENU_STATE_INTRO) || (D230.titleIntroFrame != 0) || (P32_GET(struct Title *, D230.titleObj) != NULL) ||
		    ((gGT->gameMode1 & MAIN_MENU) == 0) || (gGT->boolDemoMode != 0))
		{
			s_mmBootCreditsState = MM_BOOT_CREDITS_DONE;
			return false;
		}

		s_mmBootCreditsState = MM_BOOT_CREDITS_RUNNING;
		s_mmBootCreditsSeconds = 0.0f;
		s_mmBootCreditsStoppedMusic = false;
	}

	if (mainMenu->funcState != RECTMENU_FUNC_STATE_UPDATE)
	{
		return true;
	}

	// Silence, then the menu song starts with the logo. Checked every frame
	// in case the post-load audio state starts it late.
	if (sdata->cseqBoolPlay != 0)
	{
		CseqMusic_StopAll();
		Music_End();
		s_mmBootCreditsStoppedMusic = true;
	}

	// The menu only collects player 1's taps; any pad (or the keyboard, in
	// whichever slot it sits) may skip.
	u32 tap = 0;
	for (int i = 0; i < 4; i++)
	{
		tap |= P32_GET(struct GamepadSystem *, sdata->gGamepads)->gamepad[i].buttonsTapped;
	}

	if (s_mmBootCreditsSeconds >= MM_BOOT_CREDITS_END)
	{
		if (!gNativeCreditsSeen)
		{
			gNativeCreditsSeen = 1;
			save_config();
		}
		RECTMENU_ClearInput();
		MM_BootCredits_Finish(gGT);
		return false;
	}

	if (gNativeCreditsSeen && ((tap & MM_BOOT_CREDITS_SKIP_INPUT) != 0))
	{
		RECTMENU_ClearInput();
		MM_BootCredits_Finish(gGT);
		return false;
	}

	MM_BootCredits_Draw(gGT);

	s_mmBootCreditsSeconds += 1.0f / (float)CTR_FRAMES_PER_SECOND;
	return true;
}

// TURBOCHARGED under the logo, as 3D lettering (native_title_logo.h) that
// springs up from below the screen once the logo has landed, to a tyre
// screech. Drawn whenever the TM is; the 2D plaque stands in where the
// lettering is unavailable.

// Lettering centre and width in 4:3 UI pixels; the width matches the logo.
#define MM_TITLE_LOGO_Y              197.0f
#define MM_TITLE_LOGO_WIDTH          288.0f
// The spring: starts this far below its rest line, so off screen, and rings
// out as offset * e^(-damping t) * cos(2 pi hz t).
#define MM_TITLE_LOGO_DROP           64.0f
#define MM_TITLE_LOGO_DAMPING        5.0f
#define MM_TITLE_LOGO_BOUNCE_HZ      1.3f
// Tilt with vertical speed (radians per UI pixel per second), and a wobble.
#define MM_TITLE_LOGO_TILT           0.0014f
#define MM_TITLE_LOGO_MAX_TILT       0.5f
#define MM_TITLE_LOGO_WOBBLE         0.08f
#define MM_TITLE_LOGO_WOBBLE_HZ      1.9f
#define MM_TITLE_LOGO_WOBBLE_DAMPING 3.5f
// After this the pose is exactly at rest, so the lettering stops re-rendering.
#define MM_TITLE_LOGO_SETTLED        2.0f

// The asphalt skid. Kart sounds aren't loaded on the menu, so the sample is
// read from KART.HWL and parked in free SPU memory just long enough to start
// the voice (see MM_TitleScreech_Play).
#define MM_TITLE_SCREECH_SFX         17
#define MM_TITLE_SCREECH_HWL         "SOUNDS/KART.HWL"
#define MM_TITLE_SCREECH_HOLD        0.3f
#define MM_TITLE_SCREECH_LENGTH      0.85f
// Distortion indices: 0x80 is the recorded pitch, 128 steps per octave.
#define MM_TITLE_SCREECH_PITCH_START 0xa8
#define MM_TITLE_SCREECH_PITCH_END   0x84
// Retail banks stay below the SPU reverb work area.
#define MM_TITLE_SCREECH_SPU_LIMIT   0x7e000

static float s_mmTitleLogoSeconds;
static u32 s_mmTitleLogoLastFrame;
static b32 s_mmTitleLogoShown;

static u8 *s_mmTitleScreechSample;
static int s_mmTitleScreechSampleSize;
static b32 s_mmTitleScreechSampleTried;
static u32 s_mmTitleScreechHandle;

// Copies the skid sample out of its bank: a bank is a sector of SPU indices
// followed by those samples back to back.
static void MM_TitleScreech_LoadSample(void)
{
	if (s_mmTitleScreechSampleTried || (sdata->boolAudioEnabled == 0) || (P32_GET(struct HowlHeader *, sdata->ptrHowlHeader) == NULL))
	{
		return;
	}
	s_mmTitleScreechSampleTried = true;

	const struct HowlHeader *header = P32_GET(struct HowlHeader *, sdata->ptrHowlHeader);
	if (header->numOtherFX <= MM_TITLE_SCREECH_SFX)
	{
		return;
	}
	const int spuIndex = P32_GET(struct OtherFX *, sdata->howl_metaOtherFX)[MM_TITLE_SCREECH_SFX].spuIndex;

	struct NativeAssetsByteBuffer hwl = {0};
	if (!NativeAssets_ReadBytes(MM_TITLE_SCREECH_HWL, NATIVE_ASSET_READ_DATA_FILE, &hwl))
	{
		return;
	}

	for (int bank = 0; bank < header->numBanks; bank++)
	{
		const size_t bankOffset = (size_t)P32_GET(u16 *, sdata->howl_bankOffsets)[bank] * 0x800;
		if ((hwl.size < 0) || (bankOffset + 0x800 > (size_t)hwl.size))
		{
			continue;
		}
		const s16 *spuIndices = SBHEADER_GETARR(&hwl.data[bankOffset]);
		const int numSamples = ((const struct SampleBlockHeader *)&hwl.data[bankOffset])->numSamples;
		size_t sampleOffset = bankOffset + 0x800;
		for (int i = 0; (i < numSamples) && (i < (0x800 - 2) / 2); i++)
		{
			const size_t size = (size_t)P32_GET(struct SpuAddrEntry *, sdata->howl_spuAddrs)[spuIndices[i]].spuSize * 8;
			if ((spuIndices[i] == spuIndex) && (sampleOffset + size <= (size_t)hwl.size))
			{
				s_mmTitleScreechSample = (u8 *)malloc(size);
				if (s_mmTitleScreechSample != NULL)
				{
					memcpy(s_mmTitleScreechSample, &hwl.data[sampleOffset], size);
					s_mmTitleScreechSampleSize = (int)size;
				}
				NativeAssets_FreeBytes(&hwl);
				return;
			}
			sampleOffset += size;
		}
	}
	NativeAssets_FreeBytes(&hwl);
}

static u32 MM_TitleScreech_Flags(float t)
{
	const float bend = MM_BootCredits_Ramp(t, 0.0f, MM_TITLE_SCREECH_LENGTH);
	const float level = 1.0f - MM_BootCredits_Ramp(t, MM_TITLE_SCREECH_HOLD, MM_TITLE_SCREECH_LENGTH - MM_TITLE_SCREECH_HOLD);
	const u32 pitch = (u32)(MM_TITLE_SCREECH_PITCH_START + (MM_TITLE_SCREECH_PITCH_END - MM_TITLE_SCREECH_PITCH_START) * bend + 0.5f);

	return HowlSfx_Pack(HOWL_SFX_LR_CENTER, pitch, (u32)(HOWL_SFX_VOLUME_MAX * level + 0.5f), 0);
}

static void MM_TitleScreech_Play(void)
{
	if ((sdata->boolAudioEnabled == 0) || (P32_GET(struct HowlHeader *, sdata->ptrHowlHeader) == NULL) ||
	    (P32_GET(struct HowlHeader *, sdata->ptrHowlHeader)->numOtherFX <= MM_TITLE_SCREECH_SFX))
	{
		return;
	}

	struct SpuAddrEntry *entry =
	    &P32_GET(struct SpuAddrEntry *, sdata->howl_spuAddrs)[P32_GET(struct OtherFX *, sdata->howl_metaOtherFX)[MM_TITLE_SCREECH_SFX].spuIndex];
	if (entry->spuAddr != 0)
	{
		// Already resident.
		s_mmTitleScreechHandle = OtherFX_Play_LowLevel(MM_TITLE_SCREECH_SFX, 1, MM_TitleScreech_Flags(0.0f));
		return;
	}
	if (s_mmTitleScreechSample == NULL)
	{
		return;
	}

	// Above every resident sample. The bank allocator only hands out space
	// that is free by this measure too, so the next bank load may overwrite
	// the copy, which is why the table entry is cleared again below.
	u32 top = sdata->audioAllocPtr;
	for (int i = 0; i < P32_GET(struct HowlHeader *, sdata->ptrHowlHeader)->numSpuAddrs; i++)
	{
		const struct SpuAddrEntry *other = &P32_GET(struct SpuAddrEntry *, sdata->howl_spuAddrs)[i];
		if ((other->spuAddr != 0) && ((u32)other->spuAddr + other->spuSize > top))
		{
			top = (u32)other->spuAddr + other->spuSize;
		}
	}
	if (top * 8 + (u32)s_mmTitleScreechSampleSize > MM_TITLE_SCREECH_SPU_LIMIT)
	{
		return;
	}

	SpuSetTransferStartAddr(top * 8);
	SpuWrite(s_mmTitleScreechSample, (u32)s_mmTitleScreechSampleSize);
	SpuIsTransferCompleted(SPU_TRANSFER_WAIT);

	// The channel takes the sample address now, so the voice keeps it after
	// the entry is cleared; OtherFX_Modify only changes pitch and volume.
	entry->spuAddr = (u16)top;
	s_mmTitleScreechHandle = OtherFX_Play_LowLevel(MM_TITLE_SCREECH_SFX, 1, MM_TitleScreech_Flags(0.0f));
	entry->spuAddr = 0;
}

static void MM_TitleScreech_Update(float t)
{
	if (s_mmTitleScreechHandle == 0)
	{
		return;
	}
	if (t >= MM_TITLE_SCREECH_LENGTH)
	{
		OtherFX_Stop1((int)s_mmTitleScreechHandle);
		s_mmTitleScreechHandle = 0;
		return;
	}
	OtherFX_Modify(s_mmTitleScreechHandle, MM_TitleScreech_Flags(t));
}

static void MM_TitleLogo_Prepare(void)
{
	NativeTitleLogo_Prepare();
	MM_TitleScreech_LoadSample();
}

static void MM_TitleLogo_GetPose(float t, struct NativeTitleLogoPose *pose)
{
	const int logoX = 0x100 + CTR_WIDESCREEN_SCALE_X(MM_BOOT_CREDITS_LOGO_X - 0x100);

	pose->centerX = (float)logoX;
	pose->centerY = MM_TITLE_LOGO_Y;
	pose->width = MM_TITLE_LOGO_WIDTH;
	pose->pitch = 0.0f;
	pose->roll = 0.0f;
	if (t >= MM_TITLE_LOGO_SETTLED)
	{
		return;
	}

	const float omega = 2.0f * 3.14159265f * MM_TITLE_LOGO_BOUNCE_HZ;
	const float decay = MM_TITLE_LOGO_DROP * expf(-MM_TITLE_LOGO_DAMPING * t);
	const float speed = -decay * (MM_TITLE_LOGO_DAMPING * cosf(omega * t) + omega * sinf(omega * t));
	float tilt = speed * MM_TITLE_LOGO_TILT;

	if (tilt > MM_TITLE_LOGO_MAX_TILT)
	{
		tilt = MM_TITLE_LOGO_MAX_TILT;
	}
	if (tilt < -MM_TITLE_LOGO_MAX_TILT)
	{
		tilt = -MM_TITLE_LOGO_MAX_TILT;
	}

	pose->centerY += decay * cosf(omega * t);
	// Rising leans the top back; dropping tips it forward.
	pose->pitch = tilt;
	pose->roll = MM_TITLE_LOGO_WOBBLE * expf(-MM_TITLE_LOGO_WOBBLE_DAMPING * t) * sinf(2.0f * 3.14159265f * MM_TITLE_LOGO_WOBBLE_HZ * t);
}

static void MM_BootCredits_DrawTitleExtras(struct GameTracker *gGT, b32 userIdShown)
{
	(void)userIdShown;

	// Start over whenever the title comes back after a frame away.
	const u32 frame = gGT->frameTimer_MainFrame_ResetDB;
	if (!s_mmTitleLogoShown || (frame != s_mmTitleLogoLastFrame + 1))
	{
		s_mmTitleLogoSeconds = 0.0f;
		MM_TitleScreech_Update(MM_TITLE_SCREECH_LENGTH);
		MM_TitleScreech_Play();
	}
	s_mmTitleLogoShown = true;
	s_mmTitleLogoLastFrame = frame;

	struct NativeTitleLogoPose pose;
	MM_TitleLogo_GetPose(s_mmTitleLogoSeconds, &pose);
	MM_TitleScreech_Update(s_mmTitleLogoSeconds);
	s_mmTitleLogoSeconds += 1.0f / (float)CTR_FRAMES_PER_SECOND;

	if (!NativeTitleLogo_Draw(&P32_GET(struct DB *, gGT->backBuffer)->primMem,
	                          &P32_GET(uint32_t *, P32_GET(struct DB *, gGT->backBuffer)->otMem.uiOT)[MM_TITLE_TM_OT_INDEX], &pose))
	{
		RECTMENU_DrawQuip((char *)"TURBOCHARGED", (s16)pose.centerX, MM_BOOT_CREDITS_PLAQUE_Y, 0, FONT_BIG, JUSTIFY_CENTER | ORANGE, 0);
	}
}
#endif
