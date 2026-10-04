#include <common.h>
#include <platform/native_unlock.h>

#if defined(CTR_NATIVE)
enum NativeUnlockKind
{
	NATIVE_UNLOCK_CHARACTER,
	NATIVE_UNLOCK_TRACK,
	NATIVE_UNLOCK_ARENA,
	NATIVE_UNLOCK_SCRAPBOOK,
	NATIVE_UNLOCK_ADVENTURE_TRACK,
};

struct NativeUnlockReward
{
	int bit;
	s16 name;
	enum NativeUnlockKind kind;
};

static const struct NativeUnlockReward s_nativeUnlockRewards[] =
{
	{5, LNG_DR_N_TROPY, NATIVE_UNLOCK_CHARACTER},
	{6, LNG_PENTA_PENGUIN, NATIVE_UNLOCK_CHARACTER},
	{7, LNG_RIPPER_ROO, NATIVE_UNLOCK_CHARACTER},
	{8, LNG_PAPU_PAPU, NATIVE_UNLOCK_CHARACTER},
	{9, LNG_KOMODO_JOE, NATIVE_UNLOCK_CHARACTER},
	{10, LNG_PINSTRIPE, NATIVE_UNLOCK_CHARACTER},
	{11, LNG_FAKE_CRASH, NATIVE_UNLOCK_CHARACTER},
	{GAME_UNLOCK_BIT_OXIDE, LNG_N_OXIDE_FULL, NATIVE_UNLOCK_CHARACTER},
	{GAME_UNLOCK_BIT_TURBO_TRACK, LNG_TURBO_TRACK, NATIVE_UNLOCK_TRACK},
	{GAME_UNLOCK_BIT_PARKING_LOT, LNG_PARKING_LOT, NATIVE_UNLOCK_ARENA},
	{GAME_UNLOCK_BIT_NORTH_BOWL, LNG_THE_NORTH_BOWL, NATIVE_UNLOCK_ARENA},
	{GAME_UNLOCK_BIT_LAB_BASEMENT, LNG_LAB_BASEMENT, NATIVE_UNLOCK_ARENA},
	{GAME_UNLOCK_BIT_SCRAPBOOK, LNG_SCRAPBOOK, NATIVE_UNLOCK_SCRAPBOOK},
	{-1, LNG_SLIDE_COLISEUM, NATIVE_UNLOCK_ADVENTURE_TRACK},
};

enum { NATIVE_UNLOCK_REWARD_COUNT = sizeof(s_nativeUnlockRewards) / sizeof(s_nativeUnlockRewards[0]) };
static int s_nativeUnlockQueue[NATIVE_UNLOCK_REWARD_COUNT];
static int s_nativeUnlockHead;
static int s_nativeUnlockCount;
static double s_nativeUnlockSeconds;
static b32 s_nativeUnlockStarted;

static void NativeUnlock_QueueBit(int bit)
{
	for (int reward = 0; reward < NATIVE_UNLOCK_REWARD_COUNT; reward++)
	{
		if (s_nativeUnlockRewards[reward].bit != bit) continue;
		for (int i = 0; i < s_nativeUnlockCount; i++)
			if (s_nativeUnlockQueue[(s_nativeUnlockHead + i) % NATIVE_UNLOCK_REWARD_COUNT] == reward) return;
		if (s_nativeUnlockCount < NATIVE_UNLOCK_REWARD_COUNT)
		{
			s_nativeUnlockQueue[(s_nativeUnlockHead + s_nativeUnlockCount) % NATIVE_UNLOCK_REWARD_COUNT] = reward;
			s_nativeUnlockCount++;
		}
		return;
	}
}

void NativeUnlock_GrantBit(int bit)
{
	if ((sdata == NULL) || (bit < 0) || (bit >= GAME_PROGRESS_UNLOCK_WORD_COUNT * 32)) return;
	if (CHECK_ADV_BIT(sdata->gameProgress.unlocks, bit)) return;
	UNLOCK_ADV_BIT(sdata->gameProgress.unlocks, bit);
	NativeUnlock_QueueBit(bit);
}

void NativeUnlock_GrantMask(u32 mask)
{
	for (int bit = 0; bit < 32; bit++)
		if (mask & (1u << bit)) NativeUnlock_GrantBit(bit);
}

void NativeUnlock_NotifySlideColiseum(void)
{
	NativeUnlock_QueueBit(-1);
}

// A toast uses the shared UI ordering table and never consumes menu/race input.
// Its visible time pauses during loading, cutscenes, mask hints and active races.
void NativeUnlock_Draw(void)
{
	static const char *const titles[6] = {
		"UNLOCKED!", "DEBLOQUE!", "FREIGESCHALTET!", "SBLOCCATO!", "DESBLOQUEADO!", "ONTGRENDELD!",
	};
	static const char *const kinds[6][5] = {
		{"CHARACTER", "TRACK", "BATTLE ARENA", "SCRAPBOOK", "ADVENTURE TRACK"},
		{"PERSONNAGE", "CIRCUIT", "ARENE DE COMBAT", "ALBUM", "CIRCUIT AVENTURE"},
		{"CHARAKTER", "STRECKE", "KAMPFARENA", "ALBUM", "ABENTEUERSTRECKE"},
		{"PERSONAGGIO", "PISTA", "ARENA BATTAGLIA", "ALBUM", "PISTA AVVENTURA"},
		{"PERSONAJE", "CIRCUITO", "ARENA DE BATALLA", "ALBUM", "CIRCUITO AVENTURA"},
		{"PERSONAGE", "CIRCUIT", "GEVECHTSARENA", "ALBUM", "AVONTURENCIRCUIT"},
	};
	if ((sdata == NULL) || (sdata->gGT == NULL) || (s_nativeUnlockCount == 0)) return;
	struct GameTracker *gGT = sdata->gGT;
	if ((sdata->Loading.stage != LOAD_IDLE) || (sdata->lngStrings == NULL) ||
	    (gGT->backBuffer == NULL) || (gGT->boolDemoMode != 0) ||
	    (sdata->boolPlayVideoSTR != 0) || (sdata->AkuAkuHintState != 0) ||
	    ((gGT->gameMode1 & GAME_CUTSCENE) != 0) ||
	    ((gGT->gameMode1 & (MAIN_MENU | ADVENTURE_ARENA | END_OF_RACE)) == 0)) return;

	if (!s_nativeUnlockStarted)
	{
		OtherFX_Play(MM_CHEAT_SUCCESS_SFX, 1);
		s_nativeUnlockStarted = true;
	}
	const struct NativeUnlockReward *reward = &s_nativeUnlockRewards[s_nativeUnlockQueue[s_nativeUnlockHead]];
	// Match the native menus: configured language files 2..7 are EN, FR, DE, IT, ES, NL.
	int language = (cfg_language >= 2 && cfg_language <= 7) ? cfg_language - 2 : 0;
	char *name = sdata->lngStrings[reward->name];
	int nameFont = DecalFont_GetLineWidth(name, FONT_BIG) <= 440 ? FONT_BIG : FONT_SMALL;
	int width = DecalFont_GetLineWidth(name, nameFont) + 32;
	if (width < 280) width = 280;
	if (width > 480) width = 480;
	RECT box = {(s16)(256 - width / 2), 8, (s16)width, 58};
	Color background = {.self = 0x302018};
	Color border = {.self = 0x4080ff};
	DecalFont_DrawLine((char *)titles[language], 256, 12, FONT_SMALL, JUSTIFY_CENTER | ORANGE);
	DecalFont_DrawLine(name, 256, 28, nameFont, JUSTIFY_CENTER | WHITE);
	DecalFont_DrawLine((char *)kinds[language][reward->kind], 256, 50, FONT_SMALL, JUSTIFY_CENTER | PERIWINKLE);
	// OT insertion prepends: background renders first, behind the border and text.
	CTR_Box_DrawWireBox(&box, &border, gGT->backBuffer->otMem.uiOT, &gGT->backBuffer->primMem);
	CTR_Box_DrawSolidBox(&box, background, gGT->backBuffer->otMem.uiOT);

	// Frame-rate independent, including changes to the rate while a toast is visible.
	s_nativeUnlockSeconds += 1.0 / CTR_FRAMES_PER_SECOND;
	if (s_nativeUnlockSeconds + 0.000001 >= 5.0)
	{
		s_nativeUnlockHead = (s_nativeUnlockHead + 1) % NATIVE_UNLOCK_REWARD_COUNT;
		s_nativeUnlockCount--;
		s_nativeUnlockSeconds = 0;
		s_nativeUnlockStarted = false;
	}
}
#endif
