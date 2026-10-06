#include <common.h>
#include <platform/native_input.h>
#include <platform/native_font.h>
#include <platform/native_kart_color.h>
#include <platform/native_engine.h>
#include <platform/native_aspect.h>
#include <platform/native_projection.h>

#if defined(CTR_NATIVE)
#include "platform/native_leaderboard.h"
#include "platform/native_minimap.h"
#if defined(__vita__)
#include "platform/native_adhoc.h"
#endif
#endif

#include "platform/native_user_id.h"

#ifdef CTR_NATIVE
int gNativeOnlineLeaderboardMode;

enum MMNativeLanguageConstants
{
	MM_NATIVE_LANGUAGE_COUNT = 6,
	MM_NATIVE_LANGUAGE_TIMEOUT_FRAMES = CTR_SECONDS_TO_FRAMES(30),
	MM_NATIVE_LANGUAGE_DRAWSTYLE_WIDESCREEN = 0x200,
};

static const s16 s_nativeLanguageFileIndex[MM_NATIVE_LANGUAGE_COUNT] =
{
	2, // English (PAL/UK)
	3, // French
	4, // German
	5, // Italian
	6, // Spanish
	7, // Dutch
};


enum MMNativeExtraDifficultyConstants
{
	MM_NATIVE_DIFFICULTY_EASY = 0,
	MM_NATIVE_DIFFICULTY_MEDIUM,
	MM_NATIVE_DIFFICULTY_HARD,
	MM_NATIVE_DIFFICULTY_SUPER_HARD,
	MM_NATIVE_DIFFICULTY_ULTRA_HARD,
	MM_NATIVE_DIFFICULTY_COUNT,
};

#if defined(__vita__)
enum MMNativeAdhocMenuStage
{
	MM_NATIVE_ADHOC_STAGE_MAIN = 0,
	MM_NATIVE_ADHOC_STAGE_MODE,
	MM_NATIVE_ADHOC_STAGE_ROLE,
	MM_NATIVE_ADHOC_STAGE_WAIT,
	MM_NATIVE_ADHOC_STAGE_GAME_FLOW,
	MM_NATIVE_ADHOC_MAIN_ROW = 6,
};

static const char *s_nativeAdhocHostText[MM_NATIVE_LANGUAGE_COUNT] =
{
	"HOST GAME",
	"CREER PARTIE",
	"SPIEL HOSTEN",
	"CREA PARTITA",
	"CREAR PARTIDA",
	"SPEL HOSTEN",
};

static const char *s_nativeAdhocJoinText[MM_NATIVE_LANGUAGE_COUNT] =
{
	"JOIN GAME",
	"REJOINDRE",
	"BEITRETEN",
	"UNISCITI",
	"UNIRSE",
	"DEELNEMEN",
};

static s16 s_nativeAdhocMenuStage = MM_NATIVE_ADHOC_STAGE_MAIN;
static s16 s_nativeAdhocSuppressInputFrames;
static s16 s_nativeAdhocGameMode = NATIVE_ADHOC_GAME_MODE_ARCADE;
#endif

static struct MenuRow s_nativeExtraDifficultyRows[MM_NATIVE_DIFFICULTY_COUNT + 1] =
{
	{LNG_EASY, 0, 1, 0, 0},
	{LNG_MEDIUM, 0, 2, 1, 1},
	{LNG_HARD, 1, 3, 2, 2},
	{NATIVE_MENU_STRING_SUPER_HARD, 2, 4, 3, 3},
	{NATIVE_MENU_STRING_ULTRA_HARD, 3, 4, 4, 4},
	{.stringIndex = RECTMENU_STRING_NONE},
};

static struct RectMenu s_nativeExtraDifficultyMenu =
{
	.stringIndexTitle = LNG_DIFFICULTY,
	.state = CENTER_ON_X | USE_SMALL_FONT | BIG_TEXT_IN_TITLE,
	.rows = P32_DEFER(s_nativeExtraDifficultyRows),
	.funcPtr = P32_DEFER(MM_MenuProc_Difficulty),
};
#if defined(CTR_NATIVE_64BIT)
CTR_P32_STATIC_FIXUP(s_nativeExtraDifficultyMenu)
{
	P32_SET(s_nativeExtraDifficultyMenu.rows, s_nativeExtraDifficultyRows);
	P32_SET(s_nativeExtraDifficultyMenu.funcPtr, MM_MenuProc_Difficulty);
}
#endif

static struct MenuRow s_nativeLanguageRows[MM_NATIVE_LANGUAGE_COUNT + 1] =
{
	{LNG_ENGLISH, 5, 1, 0, 0},
	{LNG_FRENCH, 0, 2, 1, 1},
	{LNG_GERMAN, 1, 3, 2, 2},
	{LNG_ITALIAN, 2, 4, 3, 3},
	{LNG_SPANISH, 3, 5, 4, 4},
	{LNG_DUTCH, 4, 0, 5, 5},
	{.stringIndex = RECTMENU_STRING_NONE},
};

#if defined(__vita__)
static void MM_NativeAdhocModeProc(struct RectMenu *menu);
static void MM_NativeAdhocRoleProc(struct RectMenu *menu);
static void MM_NativeAdhocWaitProc(struct RectMenu *menu);

static struct MenuRow s_nativeAdhocModeRows[] =
{
	{LNG_ARCADE, 0, 1, 0, 0},
	{LNG_VS, 0, 1, 1, 1},
	{.stringIndex = RECTMENU_STRING_NONE},
};

static struct MenuRow s_nativeAdhocRoleRows[] =
{
	{LNG_NA_241, 0, 1, 0, 0},
	{LNG_NA_242, 0, 1, 1, 1},
	{.stringIndex = RECTMENU_STRING_NONE},
};

static struct MenuRow s_nativeAdhocWaitRows[] =
{
	{LNG_CANCEL, 0, 0, 0, 0},
	{.stringIndex = RECTMENU_STRING_NONE},
};

static struct RectMenu s_nativeAdhocModeMenu = {
    .stringIndexTitle = NATIVE_MENU_STRING_ADHOC,
    .state = CENTER_ON_X | USE_SMALL_FONT | BIG_TEXT_IN_TITLE,
    .rows = P32_DEFER(s_nativeAdhocModeRows),
    .funcPtr = P32_DEFER(MM_NativeAdhocModeProc),
};
#if defined(CTR_NATIVE_64BIT)
CTR_P32_STATIC_FIXUP(s_nativeAdhocModeMenu)
{
	P32_SET(s_nativeAdhocModeMenu.rows, s_nativeAdhocModeRows);
	P32_SET(s_nativeAdhocModeMenu.funcPtr, MM_NativeAdhocModeProc);
}
#endif

static struct RectMenu s_nativeAdhocRoleMenu = {
    .stringIndexTitle = NATIVE_MENU_STRING_ADHOC,
    .state = CENTER_ON_X | USE_SMALL_FONT | BIG_TEXT_IN_TITLE,
    .rows = P32_DEFER(s_nativeAdhocRoleRows),
    .funcPtr = P32_DEFER(MM_NativeAdhocRoleProc),
};
#if defined(CTR_NATIVE_64BIT)
CTR_P32_STATIC_FIXUP(s_nativeAdhocRoleMenu)
{
	P32_SET(s_nativeAdhocRoleMenu.rows, s_nativeAdhocRoleRows);
	P32_SET(s_nativeAdhocRoleMenu.funcPtr, MM_NativeAdhocRoleProc);
}
#endif

static struct RectMenu s_nativeAdhocWaitMenu = {
    .stringIndexTitle = LNG_NA_241,
    .state = CENTER_ON_X | BIG_TEXT_IN_TITLE,
    .rows = P32_DEFER(s_nativeAdhocWaitRows),
    .funcPtr = P32_DEFER(MM_NativeAdhocWaitProc),
};
#if defined(CTR_NATIVE_64BIT)
CTR_P32_STATIC_FIXUP(s_nativeAdhocWaitMenu)
{
	P32_SET(s_nativeAdhocWaitMenu.rows, s_nativeAdhocWaitRows);
	P32_SET(s_nativeAdhocWaitMenu.funcPtr, MM_NativeAdhocWaitProc);
}
#endif
#endif

static struct MenuRow s_nativeMainMenuBasic[] =
{
	{LNG_ADVENTURE, 0, 1, 0, 0},
	{LNG_TIME_TRIAL, 0, 2, 1, 1},
	{LNG_ARCADE, 1, 3, 2, 2},
	{LNG_VS, 2, 4, 3, 3},
	{LNG_BATTLE, 3, 5, 4, 4},
	{NATIVE_MENU_STRING_BOSS_FIGHT, 4, 6, 5, 5},
#if defined(__vita__)
	{NATIVE_MENU_STRING_ADHOC, 5, 7, 6, 6},
	{LNG_OPTIONS, 6, 8, 7, 7},
	{NATIVE_MENU_STRING_UNLOCKS, 7, 9, 8, 8},
	{NATIVE_MENU_STRING_CREDITS, 8, 10, 9, 9},
	{NATIVE_MENU_STRING_EXIT_GAME, 9, 10, 10, 10},
#else
	{LNG_OPTIONS, 5, 7, 6, 6},
	{NATIVE_MENU_STRING_UNLOCKS, 6, 8, 7, 7},
	{NATIVE_MENU_STRING_CREDITS, 7, 9, 8, 8},
	{NATIVE_MENU_STRING_EXIT_GAME, 8, 9, 9, 9},
#endif
	{.stringIndex = RECTMENU_STRING_NONE},
};

static struct MenuRow s_nativeMainMenuWithScrapbook[] =
{
	{LNG_ADVENTURE, 0, 1, 0, 0},
	{LNG_TIME_TRIAL, 0, 2, 1, 1},
	{LNG_ARCADE, 1, 3, 2, 2},
	{LNG_VS, 2, 4, 3, 3},
	{LNG_BATTLE, 3, 5, 4, 4},
	{NATIVE_MENU_STRING_BOSS_FIGHT, 4, 6, 5, 5},
#if defined(__vita__)
	{NATIVE_MENU_STRING_ADHOC, 5, 7, 6, 6},
	{LNG_OPTIONS, 6, 8, 7, 7},
	{LNG_SCRAPBOOK, 7, 9, 8, 8},
	{NATIVE_MENU_STRING_UNLOCKS, 8, 10, 9, 9},
	{NATIVE_MENU_STRING_CREDITS, 9, 11, 10, 10},
	{NATIVE_MENU_STRING_EXIT_GAME, 10, 11, 11, 11},
#else
	{LNG_OPTIONS, 5, 7, 6, 6},
	{LNG_SCRAPBOOK, 6, 8, 7, 7},
	{NATIVE_MENU_STRING_UNLOCKS, 7, 9, 8, 8},
	{NATIVE_MENU_STRING_CREDITS, 8, 10, 9, 9},
	{NATIVE_MENU_STRING_EXIT_GAME, 9, 10, 10, 10},
#endif
	{.stringIndex = RECTMENU_STRING_NONE},
};

// The base rewards are always visible; Oxide requires Additional Unlocks.
struct MMNativeUnlockEntry
{
	s16 name;
	s16 unlockBit; // -1: Slide Coliseum Adventure access, counted from the active profile.
	const char *requirement[2];
};

static const struct MMNativeUnlockEntry s_nativeUnlockEntries[] =
{
	{LNG_RIPPER_ROO, 7, {"WIN THE RED GEM CUP", "IN ADVENTURE MODE"}},
	{LNG_PAPU_PAPU, 8, {"WIN THE GREEN GEM CUP", "IN ADVENTURE MODE"}},
	{LNG_KOMODO_JOE, 9, {"WIN THE BLUE GEM CUP", "IN ADVENTURE MODE"}},
	{LNG_PINSTRIPE, 10, {"WIN THE YELLOW GEM CUP", "IN ADVENTURE MODE"}},
	{LNG_FAKE_CRASH, 11, {"WIN THE PURPLE GEM CUP", "IN ADVENTURE MODE"}},
	{LNG_DR_N_TROPY, 5, {"BEAT EVERY N. TROPY GHOST", "ON THE ORIGINAL TIME TRIAL TRACKS"}},
	{LNG_N_OXIDE_FULL, GAME_UNLOCK_BIT_OXIDE, {"BEAT EVERY N. OXIDE GHOST", "ON THE ORIGINAL TIME TRIAL TRACKS"}},
	{LNG_PENTA_PENGUIN, 6, {"EARN GOLD OR PLATINUM RELICS", "ON ALL 18 ADVENTURE TRACKS"}},
	{LNG_SLIDE_COLISEUM, -1, {"COLLECT 10 RELICS TO ENTER IN ADVENTURE", "ALREADY AVAILABLE OUTSIDE ADVENTURE"}},
	{LNG_TURBO_TRACK, GAME_UNLOCK_BIT_TURBO_TRACK, {"COLLECT ALL 5 GEMS, THEN EARN", "A RELIC ON TURBO TRACK IN ADVENTURE"}},
	{LNG_PARKING_LOT, GAME_UNLOCK_BIT_PARKING_LOT, {"WIN ALL 4 ARCADE CUPS ON EASY", "IN SINGLE PLAYER"}},
	{LNG_THE_NORTH_BOWL, GAME_UNLOCK_BIT_NORTH_BOWL, {"WIN ALL 4 ARCADE CUPS ON MEDIUM", "IN SINGLE PLAYER"}},
	{LNG_LAB_BASEMENT, GAME_UNLOCK_BIT_LAB_BASEMENT, {"WIN ALL 4 ARCADE CUPS ON HARD", "IN SINGLE PLAYER"}},
	{LNG_SCRAPBOOK, GAME_UNLOCK_BIT_SCRAPBOOK, {"BEAT EVERY N. OXIDE GHOST", "ON THE ORIGINAL TIME TRIAL TRACKS"}},
};

// Title menus render on a 512 x 216 canvas, including in widescreen.
enum { MM_NATIVE_UNLOCK_VISIBLE_ROWS = 7, MM_NATIVE_BASE_UNLOCK_COUNT = 13 };
static int s_nativeUnlockFirst;
static void MM_NativeUnlocksMenuProc(struct RectMenu *menu);
static struct RectMenu s_nativeUnlocksMenu = {
    .stringIndexTitle = NATIVE_MENU_STRING_UNLOCKS,
    .state = RECTMENU_STATE_INVISIBLE_CALLBACK,
    .funcPtr = P32_DEFER(MM_NativeUnlocksMenuProc),
};
#if defined(CTR_NATIVE_64BIT)
CTR_P32_STATIC_FIXUP(s_nativeUnlocksMenu)
{
	P32_SET(s_nativeUnlocksMenu.funcPtr, MM_NativeUnlocksMenuProc);
}
#endif

static int MM_NativeUnlockCount(void)
{
	return gNativeAdditionalUnlocksEnabled
		? (int)(sizeof(s_nativeUnlockEntries) / sizeof(s_nativeUnlockEntries[0]))
		: MM_NATIVE_BASE_UNLOCK_COUNT;
}

static const struct MMNativeUnlockEntry *MM_NativeUnlockEntryAt(int index)
{
	// Oxide sits between N. Tropy and Penta when available; omit that row
	// while Additional Unlocks is disabled.
	if (!gNativeAdditionalUnlocksEnabled && index >= 6) index++;
	return &s_nativeUnlockEntries[index];
}

static b32 MM_NativeUnlockIsUnlocked(const struct MMNativeUnlockEntry *entry)
{
	if (entry->unlockBit >= 0)
		return CHECK_ADV_BIT(sdata->gameProgress.unlocks, entry->unlockBit) != 0;
	int relics = 0;
	for (int i = 0; i < ADV_REWARD_RELIC_TRACK_COUNT; i++)
		relics += CHECK_ADV_BIT(sdata->advProgress.rewards, ADV_REWARD_FIRST_SAPPHIRE_RELIC + i) != 0;
	return relics >= 10;
}

static void MM_NativeUnlocksInput(struct RectMenu *menu, u32 tap)
{
	int count = MM_NativeUnlockCount();
	if (menu->rowSelected < 0) menu->rowSelected = 0;
	if (menu->rowSelected >= count) menu->rowSelected = count - 1;
	if (tap & (BTN_TRIANGLE | BTN_SQUARE_one))
	{
		P32_SET(sdata->ptrDesiredMenu, &D230.menuMainMenu);
		OtherFX_Play(2, 1);
		return;
	}
	if (tap & (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT))
	{
		int step = (tap & (BTN_UP | BTN_LEFT)) ? -1 : 1;
		menu->rowSelected = (menu->rowSelected + step + count) % count;
		OtherFX_Play(0, 1);
	}
	// Keep the selection visible without moving between discrete pages.
	if (s_nativeUnlockFirst > menu->rowSelected) s_nativeUnlockFirst = menu->rowSelected;
	if (s_nativeUnlockFirst < menu->rowSelected - MM_NATIVE_UNLOCK_VISIBLE_ROWS + 1)
		s_nativeUnlockFirst = menu->rowSelected - MM_NATIVE_UNLOCK_VISIBLE_ROWS + 1;
	if (s_nativeUnlockFirst > count - MM_NATIVE_UNLOCK_VISIBLE_ROWS)
		s_nativeUnlockFirst = count - MM_NATIVE_UNLOCK_VISIBLE_ROWS;
}

static void MM_NativeUnlocksPanel(RECT box)
{
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);
	RECTMENU_DrawInnerRect(&box, 0, P32_GET(uint32_t *, P32_GET(struct DB *, gGT->backBuffer)->otMem.uiOT));
}

static void MM_NativeUnlocksMenuProc(struct RectMenu *menu)
{
	if (menu->funcState != RECTMENU_FUNC_STATE_UPDATE) return;
	MM_NativeUnlocksInput(menu, sdata->buttonTapPerPlayer[0]);
	RECTMENU_ClearInput();
	if (P32_GET(struct RectMenu *, sdata->ptrDesiredMenu) == &D230.menuMainMenu)
		return;

	int count = MM_NativeUnlockCount();
	DecalFont_DrawLine(RECTMENU_GetString(NATIVE_MENU_STRING_UNLOCKS), 256, 12, FONT_BIG, JUSTIFY_CENTER | ORANGE);
	DecalFont_DrawLine("REWARDS", 28, 36, FONT_SMALL, PERIWINKLE);
	char position[32];
	snprintf(position, sizeof(position), "%d / %d", menu->rowSelected + 1, count);
	DecalFont_DrawLine(position, 218, 36, FONT_SMALL, JUSTIFY_RIGHT | WHITE);
	for (int i = 0; i < MM_NATIVE_UNLOCK_VISIBLE_ROWS; i++)
	{
		int index = s_nativeUnlockFirst + i;
		const struct MMNativeUnlockEntry *item = MM_NativeUnlockEntryAt(index);
		int y = 54 + i * 16;
		b32 selected = index == menu->rowSelected;
		DecalFont_DrawMultiLine(RECTMENU_GetString(item->name), 28, y, 190, FONT_SMALL, selected ? ORANGE : WHITE);
		if (selected)
		{
			RECT highlight = {24, (s16)(y - 3), 198, 14};
			CTR_Box_DrawClearBox(&highlight, &sdata->menuRowHighlight_Normal, 1,
			                     P32_GET(uint32_t *, P32_GET(struct DB *, P32_GET(struct GameTracker *, sdata->gGT)->backBuffer)->otMem.uiOT));
		}
	}
	DecalFont_DrawLine(s_nativeUnlockFirst > 0 ? "ABOVE" : "", 28, 170, FONT_SMALL, PERIWINKLE);
	DecalFont_DrawLine(s_nativeUnlockFirst + MM_NATIVE_UNLOCK_VISIBLE_ROWS < count ? "BELOW" : "", 218, 170, FONT_SMALL, JUSTIFY_RIGHT | PERIWINKLE);

	const struct MMNativeUnlockEntry *entry = MM_NativeUnlockEntryAt(menu->rowSelected);
	b32 unlocked = MM_NativeUnlockIsUnlocked(entry);
	DecalFont_DrawMultiLine(RECTMENU_GetString(entry->name), 250, 36, 234, FONT_SMALL, ORANGE);
	DecalFont_DrawLine(unlocked ? "UNLOCKED" : "LOCKED", 250, 54, FONT_SMALL, unlocked ? ORANGE : WHITE);
	DecalFont_DrawLine((entry->name == LNG_PENTA_PENGUIN || entry->name == LNG_N_OXIDE_FULL || menu->rowSelected >= MM_NATIVE_BASE_UNLOCK_COUNT) ? "ADDITIONAL UNLOCK" : "HOW TO UNLOCK", 250, 78, FONT_SMALL, PERIWINKLE);
	if (!gNativeAdditionalUnlocksEnabled &&
		(entry->name == LNG_PENTA_PENGUIN || entry->name == LNG_N_OXIDE_FULL))
		DecalFont_DrawMultiLine("CHEAT ONLY OR ENABLE ADDITIONAL UNLOCKS", 250, 94, 234, FONT_SMALL, WHITE);
	else
	{
		char requirement[160];
		snprintf(requirement, sizeof(requirement), "%s %s", entry->requirement[0], entry->requirement[1]);
		DecalFont_DrawMultiLine(requirement, 250, 94, 234, FONT_SMALL, WHITE);
	}
	MM_NativeUnlocksPanel((RECT){16, 30, 214, 154});
	MM_NativeUnlocksPanel((RECT){238, 30, 258, 154});
	DecalFont_DrawLine("UP / DOWN: BROWSE", 28, 196, FONT_SMALL, WHITE);
	DecalFont_DrawLine("^ / [: BACK", 484, 196, FONT_SMALL, JUSTIFY_RIGHT | WHITE);
}

static struct MenuRow s_nativeTimeTrialRows[] =
{
#if CTR_NATIVE_HAS_LEADERBOARD
	{LNG_TIME_TRIAL, 4, 1, 0, 0},
	{LNG_RELIC_RACE, 0, 2, 1, 1},
	{NATIVE_MENU_STRING_GHOST_REPLAY, 1, 3, 2, 2},
	{LNG_HIGH_SCORE, 2, 4, 3, 3},
	{NATIVE_MENU_STRING_ONLINE_LEADERBOARD, 3, 0, 4, 4},
#else
	{LNG_TIME_TRIAL, 3, 1, 0, 0},
	{LNG_RELIC_RACE, 0, 2, 1, 1},
	{NATIVE_MENU_STRING_GHOST_REPLAY, 1, 3, 2, 2},
	{LNG_HIGH_SCORE, 2, 0, 3, 3},
#endif
	{.stringIndex = RECTMENU_STRING_NONE},
};

// Up/down links are set by MM_NativeOptionsConfigureRows, which also appends
// the pause-only row.
static struct MenuRow s_nativeOptionsRows[] = {
    {.stringIndex = NATIVE_MENU_STRING_DISPLAY},
#ifndef __vita__
    {.stringIndex = NATIVE_MENU_STRING_GRAPHICS},
#endif
    {.stringIndex = NATIVE_MENU_STRING_AUDIO},
    {.stringIndex = NATIVE_MENU_STRING_GAMEPLAY},
    {.stringIndex = NATIVE_MENU_STRING_UI},
    {.stringIndex = NATIVE_MENU_STRING_CONTROLS},
    {.stringIndex = LNG_LANGUAGE},
    {.stringIndex = NATIVE_MENU_STRING_CHEATS},
    {.stringIndex = NATIVE_MENU_STRING_PRESET},
#ifndef __vita__
    {.stringIndex = NATIVE_MENU_STRING_EXPERIMENTAL},
#endif
    {.stringIndex = RECTMENU_STRING_NONE},
    {.stringIndex = RECTMENU_STRING_NONE}, // pause-only Gamepad / Vibration category
};

static struct MenuRow s_nativeAudioRows[] =
{
	{NATIVE_MENU_STRING_AUDIO_FX, 3, 1, 0, 0},
	{NATIVE_MENU_STRING_AUDIO_MUSIC, 0, 2, 1, 1},
	{NATIVE_MENU_STRING_AUDIO_VOICE, 1, 3, 2, 2},
	{NATIVE_MENU_STRING_AUDIO_MODE, 2, 0, 3, 3},
	{.stringIndex = RECTMENU_STRING_NONE},
};

// Display frames the picture: rate, window and camera geometry.
static struct MenuRow s_nativeDisplayRows[] = {
    {.stringIndex = NATIVE_MENU_STRING_FRAME_RATE},
#ifndef __vita__
    {.stringIndex = NATIVE_MENU_STRING_BORDERLESS},
#if NATIVE_DRAW3D_SUPPORTED
    {.stringIndex = NATIVE_MENU_STRING_ASPECT_RATIO},
    {.stringIndex = NATIVE_MENU_STRING_FIELD_OF_VIEW},
    {.stringIndex = NATIVE_MENU_STRING_PROJECTION},
    {.stringIndex = NATIVE_MENU_STRING_PROJECTION_STRENGTH},
#endif
#endif
    {.stringIndex = RECTMENU_STRING_NONE},
};

#ifndef __vita__
// Graphics sets how the image is rendered: renderer, quality and the PS1 look.
// Classic-only rows come last.
static struct MenuRow s_nativeGraphicsRows[] = {
#if NATIVE_DRAW3D_SUPPORTED
    {.stringIndex = NATIVE_MENU_STRING_RENDERER},
#endif
    {.stringIndex = NATIVE_MENU_STRING_PS1_RESOLUTION}, {.stringIndex = NATIVE_MENU_STRING_ANTI_ALIASING},  {.stringIndex = NATIVE_MENU_STRING_COLOR_DEPTH},
    {.stringIndex = NATIVE_MENU_STRING_DITHERING},      {.stringIndex = NATIVE_MENU_STRING_TEXTURE_FILTER}, {.stringIndex = NATIVE_MENU_STRING_MAX_LOD},
    {.stringIndex = NATIVE_MENU_STRING_DEPTH_BUFFER},   {.stringIndex = NATIVE_MENU_STRING_PGXP},           {.stringIndex = RECTMENU_STRING_NONE},
};
#endif

static struct MenuRow s_nativeGameplayRows[] =
{
	{.stringIndex = NATIVE_MENU_STRING_AI_RACERS},
	{.stringIndex = NATIVE_MENU_STRING_MIRROR_MODE},
#ifndef __vita__
	{.stringIndex = NATIVE_MENU_STRING_ENGINE_SELECTION},
	{.stringIndex = NATIVE_MENU_STRING_ADDITIONAL_UNLOCKS},
	{.stringIndex = NATIVE_MENU_STRING_PHYSICS},
	{.stringIndex = NATIVE_MENU_STRING_AI_PHYSICS},
	{.stringIndex = NATIVE_MENU_STRING_COLLISION_PHYSICS},
	{.stringIndex = NATIVE_MENU_STRING_STEERING_PHYSICS},
#endif
	{.stringIndex = RECTMENU_STRING_NONE},
};

#ifndef __vita__
static struct MenuRow s_nativeExperimentalRows[] =
{
	{.stringIndex = NATIVE_MENU_STRING_KART_HUE},
	{.stringIndex = RECTMENU_STRING_NONE},
};
#endif

static struct MenuRow s_nativeUiRows[] =
{
	{.stringIndex = NATIVE_MENU_STRING_DEFAULT_CAMERA},
	{.stringIndex = NATIVE_MENU_STRING_DEFAULT_HUD},
	{.stringIndex = NATIVE_MENU_STRING_SKIP_MASK_HINTS},
#ifndef __vita__
	{.stringIndex = NATIVE_MENU_STRING_HD_PAUSE},
	{.stringIndex = NATIVE_MENU_STRING_MODERN_MAP},
	{.stringIndex = NATIVE_MENU_STRING_FONT},
	{.stringIndex = NATIVE_MENU_STRING_MODERN_HUD_ICONS},
#endif
	{.stringIndex = NATIVE_MENU_STRING_BOOST_COUNTER},
	{.stringIndex = RECTMENU_STRING_NONE},
};

static struct MenuRow s_nativeCheatsRows[] =
{
	{NATIVE_MENU_STRING_CHEAT_WUMPA, 9, 1, 0, 0},
	{NATIVE_MENU_STRING_CHEAT_MASK, 0, 2, 1, 1},
	{NATIVE_MENU_STRING_CHEAT_TURBO, 1, 3, 2, 2},
	{NATIVE_MENU_STRING_CHEAT_BOMBS, 2, 4, 3, 3},
	{NATIVE_MENU_STRING_CHEAT_INVISIBLE, 3, 5, 4, 4},
	{NATIVE_MENU_STRING_CHEAT_ENGINE, 4, 6, 5, 5},
	{NATIVE_MENU_STRING_CHEAT_ICY, 5, 7, 6, 6},
	{NATIVE_MENU_STRING_CHEAT_TURBOPAD, 6, 8, 7, 7},
	{NATIVE_MENU_STRING_CHEAT_ADV, 7, 9, 8, 8},
	{NATIVE_MENU_STRING_CHEAT_CHARACTERS, 8, 0, 9, 9},
	{.stringIndex = RECTMENU_STRING_NONE},
};

static struct MenuRow s_nativeControlsRows[] =
{
	{NATIVE_MENU_STRING_CONTROL_HEADER | MENU_ROW_LOCKED, 0, 1, 0, 0},
	{NATIVE_MENU_STRING_CONTROL_CROSS, 13, 2, 1, 1},
	{NATIVE_MENU_STRING_CONTROL_SQUARE, 1, 3, 2, 2},
	{NATIVE_MENU_STRING_CONTROL_CIRCLE, 2, 4, 3, 3},
	{NATIVE_MENU_STRING_CONTROL_TRIANGLE, 3, 5, 4, 4},
	{NATIVE_MENU_STRING_CONTROL_L1, 4, 6, 5, 5},
	{NATIVE_MENU_STRING_CONTROL_R1, 5, 7, 6, 6},
	{NATIVE_MENU_STRING_CONTROL_L2, 6, 8, 7, 7},
	{NATIVE_MENU_STRING_CONTROL_R2, 7, 9, 8, 8},
	{NATIVE_MENU_STRING_CONTROL_UP, 8, 10, 9, 9},
	{NATIVE_MENU_STRING_CONTROL_DOWN, 9, 11, 10, 10},
	{NATIVE_MENU_STRING_CONTROL_LEFT, 10, 12, 11, 11},
	{NATIVE_MENU_STRING_CONTROL_RIGHT, 11, 13, 12, 12},
	{NATIVE_MENU_STRING_CONTROL_START, 12, 1, 13, 13},
	{.stringIndex = RECTMENU_STRING_NONE},
};

static struct MenuRow s_nativeCreditsRows[] =
{
	{NATIVE_MENU_STRING_CREDITS_LINE_0, 0, 1, 0, 0},
	{NATIVE_MENU_STRING_CREDITS_LINE_1, 0, 2, 1, 1},
	{NATIVE_MENU_STRING_CREDITS_LINE_2, 1, 3, 2, 2},
	{NATIVE_MENU_STRING_CREDITS_LINE_3, 2, 4, 3, 3},
	{NATIVE_MENU_STRING_CREDITS_LINE_4, 3, 5, 4, 4},
	{NATIVE_MENU_STRING_CREDITS_LINE_5, 4, 6, 5, 5},
	{NATIVE_MENU_STRING_CREDITS_LINE_6, 5, 7, 6, 6},
	{NATIVE_MENU_STRING_CREDITS_LINE_7, 6, 8, 7, 7},
	{NATIVE_MENU_STRING_CREDITS_LINE_8, 7, 8, 8, 8},
	{.stringIndex = RECTMENU_STRING_NONE},
};

static struct MenuRow s_nativeBossFightRows[] =
{
	{LNG_RIPPER_ROO, 0, 1, 0, 0},
	{LNG_PAPU_PAPU, 0, 2, 1, 1},
	{LNG_KOMODO_JOE, 1, 3, 2, 2},
	{LNG_PINSTRIPE, 2, 4, 3, 3},
	{LNG_N_OXIDE_FULL, 3, 5, 4, 4},
	{NATIVE_MENU_STRING_OXIDE_FINAL, 4, 5, 5, 5},
	{.stringIndex = RECTMENU_STRING_NONE},
};

static void MM_NativeLanguageBootMenuProc(struct RectMenu *menu);
static void MM_NativeLanguageMainMenuProc(struct RectMenu *menu);
static void MM_NativeTimeTrialMenuProc(struct RectMenu *menu);
static void MM_NativeOptionsMenuProc(struct RectMenu *menu);
static void MM_NativeCheatsMenuProc(struct RectMenu *menu);
static void MM_NativeControlsMenuProc(struct RectMenu *menu);
static void MM_NativeBossFightMenuProc(struct RectMenu *menu);
static void MM_NativeCreditsMenuProc(struct RectMenu *menu);

static struct RectMenu s_nativeCreditsMenu = {
    .stringIndexTitle = RECTMENU_STRING_NONE,
    .state = CENTER_ON_X | USE_SMALL_FONT,
    .rows = P32_DEFER(s_nativeCreditsRows),
    .funcPtr = P32_DEFER(MM_NativeCreditsMenuProc),
};
#if defined(CTR_NATIVE_64BIT)
CTR_P32_STATIC_FIXUP(s_nativeCreditsMenu)
{
	P32_SET(s_nativeCreditsMenu.rows, s_nativeCreditsRows);
	P32_SET(s_nativeCreditsMenu.funcPtr, MM_NativeCreditsMenuProc);
}
#endif

static struct RectMenu s_nativeLanguageBootMenu =
{
	.stringIndexTitle = RECTMENU_STRING_NONE,
	.posX_curr = 256,
	.posY_curr = 118,
	.state = RECTMENU_STATE_EXEC_CENTERED,
	.rows = P32_DEFER(s_nativeLanguageRows),
	.funcPtr = P32_DEFER(MM_NativeLanguageBootMenuProc),
#if CTR_NATIVE_WIDESCREEN
	.drawStyle = MM_NATIVE_LANGUAGE_DRAWSTYLE_WIDESCREEN,
#endif
};
#if defined(CTR_NATIVE_64BIT)
CTR_P32_STATIC_FIXUP(s_nativeLanguageBootMenu)
{
	P32_SET(s_nativeLanguageBootMenu.rows, s_nativeLanguageRows);
	P32_SET(s_nativeLanguageBootMenu.funcPtr, MM_NativeLanguageBootMenuProc);
}
#endif

static struct RectMenu s_nativeLanguageMainMenu =
{
	.stringIndexTitle = RECTMENU_STRING_NONE,
	.state = CENTER_ON_X | USE_SMALL_FONT,
	.rows = P32_DEFER(s_nativeLanguageRows),
	.funcPtr = P32_DEFER(MM_NativeLanguageMainMenuProc),
};
#if defined(CTR_NATIVE_64BIT)
CTR_P32_STATIC_FIXUP(s_nativeLanguageMainMenu)
{
	P32_SET(s_nativeLanguageMainMenu.rows, s_nativeLanguageRows);
	P32_SET(s_nativeLanguageMainMenu.funcPtr, MM_NativeLanguageMainMenuProc);
}
#endif

static void MM_NativePresetMenuProc(struct RectMenu *menu);

static struct MenuRow s_nativePresetRows[] =
{
	{NATIVE_MENU_STRING_PRESET_PS1, 3, 1, 0, 0},
	{NATIVE_MENU_STRING_PRESET_VANILLA_PLUS, 0, 2, 1, 1},
	{NATIVE_MENU_STRING_PRESET_TURBOCHARGED, 1, 3, 2, 2},
	{NATIVE_MENU_STRING_PRESET_CUSTOM, 2, 0, 3, 3},
	{.stringIndex = RECTMENU_STRING_NONE},
};

static struct RectMenu s_nativePresetMenu = {
    .stringIndexTitle = RECTMENU_STRING_NONE,
    .posX_curr = 256,
    .posY_curr = 100,
    .state = RECTMENU_STATE_EXEC_CENTERED | RECTMENU_NATIVE_DRAW_CALLBACK,
    .rows = P32_DEFER(s_nativePresetRows),
    .funcPtr = P32_DEFER(MM_NativePresetMenuProc),
#if CTR_NATIVE_WIDESCREEN
    .drawStyle = MM_NATIVE_LANGUAGE_DRAWSTYLE_WIDESCREEN,
#endif
};
#if defined(CTR_NATIVE_64BIT)
CTR_P32_STATIC_FIXUP(s_nativePresetMenu)
{
	P32_SET(s_nativePresetMenu.rows, s_nativePresetRows);
	P32_SET(s_nativePresetMenu.funcPtr, MM_NativePresetMenuProc);
}
#endif

// Same popup, reopened from Options > Settings Preset.
static struct RectMenu s_nativePresetOptionsMenu = {
    .stringIndexTitle = RECTMENU_STRING_NONE,
    .posX_curr = 256,
    .posY_curr = 100,
    .state = RECTMENU_STATE_EXEC_CENTERED | RECTMENU_NATIVE_DRAW_CALLBACK,
    .rows = P32_DEFER(s_nativePresetRows),
    .funcPtr = P32_DEFER(MM_NativePresetMenuProc),
#if CTR_NATIVE_WIDESCREEN
    .drawStyle = MM_NATIVE_LANGUAGE_DRAWSTYLE_WIDESCREEN,
#endif
};
#if defined(CTR_NATIVE_64BIT)
CTR_P32_STATIC_FIXUP(s_nativePresetOptionsMenu)
{
	P32_SET(s_nativePresetOptionsMenu.rows, s_nativePresetRows);
	P32_SET(s_nativePresetOptionsMenu.funcPtr, MM_NativePresetMenuProc);
}
#endif

static void MM_NativeTimeTrialRefreshOnlineRow(void)
{
#if CTR_NATIVE_HAS_LEADERBOARD
	s16 onlineString = NATIVE_MENU_STRING_ONLINE_LEADERBOARD;
	if (!NativeLeaderboard_IsInternetConnected())
	{
		onlineString |= MENU_ROW_LOCKED;
	}
	s_nativeTimeTrialRows[4].stringIndex = onlineString;
#endif
}
static struct RectMenu s_nativeTimeTrialMenu =
{
	.stringIndexTitle = RECTMENU_STRING_NONE,
	.state = CENTER_ON_X,
	.rows = P32_DEFER(s_nativeTimeTrialRows),
	.funcPtr = P32_DEFER(MM_NativeTimeTrialMenuProc),
};
#if defined(CTR_NATIVE_64BIT)
CTR_P32_STATIC_FIXUP(s_nativeTimeTrialMenu)
{
	P32_SET(s_nativeTimeTrialMenu.rows, s_nativeTimeTrialRows);
	P32_SET(s_nativeTimeTrialMenu.funcPtr, MM_NativeTimeTrialMenuProc);
}
#endif

static struct RectMenu s_nativeOptionsMenu =
{
	.stringIndexTitle = RECTMENU_STRING_NONE,
	.state = CENTER_ON_X | USE_SMALL_FONT | BIG_TEXT_IN_TITLE,
	.rows = P32_DEFER(s_nativeOptionsRows),
	.funcPtr = P32_DEFER(MM_NativeOptionsMenuProc),
	.drawStyle = RECTMENU_DRAW_STYLE_NATIVE_OPTIONS,
};
#if defined(CTR_NATIVE_64BIT)
CTR_P32_STATIC_FIXUP(s_nativeOptionsMenu)
{
	P32_SET(s_nativeOptionsMenu.rows, s_nativeOptionsRows);
	P32_SET(s_nativeOptionsMenu.funcPtr, MM_NativeOptionsMenuProc);
}
#endif

static struct RectMenu s_nativeDisplayMenu = {
    .stringIndexTitle = RECTMENU_STRING_NONE,
    .state = CENTER_ON_X | USE_SMALL_FONT,
    .rows = P32_DEFER(s_nativeDisplayRows),
    .funcPtr = P32_DEFER(MM_NativeOptionsMenuProc),
    .drawStyle = RECTMENU_DRAW_STYLE_NATIVE_OPTIONS,
};
#if defined(CTR_NATIVE_64BIT)
CTR_P32_STATIC_FIXUP(s_nativeDisplayMenu)
{
	P32_SET(s_nativeDisplayMenu.rows, s_nativeDisplayRows);
	P32_SET(s_nativeDisplayMenu.funcPtr, MM_NativeOptionsMenuProc);
}
#endif

#ifndef __vita__
static struct RectMenu s_nativeGraphicsMenu = {
    .stringIndexTitle = RECTMENU_STRING_NONE,
    .state = CENTER_ON_X | USE_SMALL_FONT,
    .rows = P32_DEFER(s_nativeGraphicsRows),
    .funcPtr = P32_DEFER(MM_NativeOptionsMenuProc),
    .drawStyle = RECTMENU_DRAW_STYLE_NATIVE_OPTIONS,
};
#if defined(CTR_NATIVE_64BIT)
CTR_P32_STATIC_FIXUP(s_nativeGraphicsMenu)
{
	P32_SET(s_nativeGraphicsMenu.rows, s_nativeGraphicsRows);
	P32_SET(s_nativeGraphicsMenu.funcPtr, MM_NativeOptionsMenuProc);
}
#endif
#endif

static struct RectMenu s_nativeAudioMenu = {
    .stringIndexTitle = RECTMENU_STRING_NONE,
    .state = CENTER_ON_X | USE_SMALL_FONT,
    .rows = P32_DEFER(s_nativeAudioRows),
    .funcPtr = P32_DEFER(MM_NativeOptionsMenuProc),
    .drawStyle = RECTMENU_DRAW_STYLE_NATIVE_OPTIONS,
};
#if defined(CTR_NATIVE_64BIT)
CTR_P32_STATIC_FIXUP(s_nativeAudioMenu)
{
	P32_SET(s_nativeAudioMenu.rows, s_nativeAudioRows);
	P32_SET(s_nativeAudioMenu.funcPtr, MM_NativeOptionsMenuProc);
}
#endif

static struct RectMenu s_nativeGameplayMenu = {
    .stringIndexTitle = RECTMENU_STRING_NONE,
    .state = CENTER_ON_X | USE_SMALL_FONT,
    .rows = P32_DEFER(s_nativeGameplayRows),
    .funcPtr = P32_DEFER(MM_NativeOptionsMenuProc),
    .drawStyle = RECTMENU_DRAW_STYLE_NATIVE_OPTIONS,
};
#if defined(CTR_NATIVE_64BIT)
CTR_P32_STATIC_FIXUP(s_nativeGameplayMenu)
{
	P32_SET(s_nativeGameplayMenu.rows, s_nativeGameplayRows);
	P32_SET(s_nativeGameplayMenu.funcPtr, MM_NativeOptionsMenuProc);
}
#endif

#ifndef __vita__
static struct RectMenu s_nativeExperimentalMenu = {
    .stringIndexTitle = RECTMENU_STRING_NONE,
    .state = CENTER_ON_X | USE_SMALL_FONT,
    .rows = P32_DEFER(s_nativeExperimentalRows),
    .funcPtr = P32_DEFER(MM_NativeOptionsMenuProc),
    .drawStyle = RECTMENU_DRAW_STYLE_NATIVE_OPTIONS,
};
#if defined(CTR_NATIVE_64BIT)
CTR_P32_STATIC_FIXUP(s_nativeExperimentalMenu)
{
	P32_SET(s_nativeExperimentalMenu.rows, s_nativeExperimentalRows);
	P32_SET(s_nativeExperimentalMenu.funcPtr, MM_NativeOptionsMenuProc);
}
#endif
#endif

static struct RectMenu s_nativeUiMenu = {
    .stringIndexTitle = RECTMENU_STRING_NONE,
    .state = CENTER_ON_X | USE_SMALL_FONT,
    .rows = P32_DEFER(s_nativeUiRows),
    .funcPtr = P32_DEFER(MM_NativeOptionsMenuProc),
    .drawStyle = RECTMENU_DRAW_STYLE_NATIVE_OPTIONS,
};
#if defined(CTR_NATIVE_64BIT)
CTR_P32_STATIC_FIXUP(s_nativeUiMenu)
{
	P32_SET(s_nativeUiMenu.rows, s_nativeUiRows);
	P32_SET(s_nativeUiMenu.funcPtr, MM_NativeOptionsMenuProc);
}
#endif

static struct RectMenu s_nativeCheatsMenu = {
    .stringIndexTitle = RECTMENU_STRING_NONE,
    .state = CENTER_ON_X | USE_SMALL_FONT,
    .rows = P32_DEFER(s_nativeCheatsRows),
    .funcPtr = P32_DEFER(MM_NativeCheatsMenuProc),
};
#if defined(CTR_NATIVE_64BIT)
CTR_P32_STATIC_FIXUP(s_nativeCheatsMenu)
{
	P32_SET(s_nativeCheatsMenu.rows, s_nativeCheatsRows);
	P32_SET(s_nativeCheatsMenu.funcPtr, MM_NativeCheatsMenuProc);
}
#endif

static struct RectMenu s_nativeControlsMenu =
{
	.stringIndexTitle = NATIVE_MENU_STRING_CONTROLS,
	.posX_curr = 256,
	.posY_curr = 120,
	.state = CENTER_ON_COORDS | USE_SMALL_FONT | BIG_TEXT_IN_TITLE | RECTMENU_DRAW_CALLBACK_FLAGS,
	.rows = P32_DEFER(s_nativeControlsRows),
	.funcPtr = P32_DEFER(MM_NativeControlsMenuProc),
	.rowSelected = 1,
};
#if defined(CTR_NATIVE_64BIT)
CTR_P32_STATIC_FIXUP(s_nativeControlsMenu)
{
	P32_SET(s_nativeControlsMenu.rows, s_nativeControlsRows);
	P32_SET(s_nativeControlsMenu.funcPtr, MM_NativeControlsMenuProc);
}
#endif

static struct RectMenu s_nativeBossFightMenu =
{
	.stringIndexTitle = NATIVE_MENU_STRING_BOSS_FIGHT,
	.posX_curr = 256,
	.posY_curr = 82,
	.state = RECTMENU_STATE_EXEC_CENTERED | USE_SMALL_FONT | BIG_TEXT_IN_TITLE,
	.rows = P32_DEFER(s_nativeBossFightRows),
	.funcPtr = P32_DEFER(MM_NativeBossFightMenuProc),
};
#if defined(CTR_NATIVE_64BIT)
CTR_P32_STATIC_FIXUP(s_nativeBossFightMenu)
{
	P32_SET(s_nativeBossFightMenu.rows, s_nativeBossFightRows);
	P32_SET(s_nativeBossFightMenu.funcPtr, MM_NativeBossFightMenuProc);
}
#endif

s32 s_nativeLanguageChosen = 0;
static s32 s_nativeLanguageTimer;
static s16 s_nativeLanguageRow;
int gNativeControlsSelectedColumn = 0;
int gNativeControlsSelectedAction = 0;
int gNativeControlsCaptureActive = 0;
static s32 s_nativeControlsWaitForRelease;
static s32 s_nativeControlsWaitDevice;
static s32 s_nativeControlsWaitBinding;
extern void save_config();

int gNativePresetPending = 0;
static int s_nativePresetOptionsActive;
// Options menu context saved while the Options > Settings Preset popup is open.
static struct RectMenu *s_nativePresetReturnParent;
static u32 s_nativePresetReturnParentState;
static u32 s_nativePresetReturnState;
static s16 s_nativePresetReturnPosX;
static s16 s_nativePresetReturnPosY;
static s16 s_nativePresetReturnRow;
static void MM_NativeOptionsOpenFromPreset(void);

enum NativePreset
{
	NATIVE_PRESET_PS1,
	NATIVE_PRESET_VANILLA_PLUS,
	NATIVE_PRESET_TURBOCHARGED,
	NATIVE_PRESET_CUSTOM,
};

// Applies a preset to every graphics / gameplay / interface toggle. Language, controls,
// audio, cheats, borderless and kart hue are left alone.
static void MM_NativeApplyPreset(int preset)
{
	int vanillaPlus = (preset != NATIVE_PRESET_PS1);
	int turbo = (preset == NATIVE_PRESET_TURBOCHARGED);

	gNativeMirrorModeEnabled = 0;
	gNativeDefaultCameraFar = 0;
	gNativeDefaultHudSpeedometer = 0;
	gNativeSkipMaskHints = 0;
	gNativeAIRacersMode = turbo ? NATIVE_AI_RACERS_EXTENDED : NATIVE_AI_RACERS_RETAIL;
	gNativeEngineSelectionEnabled = vanillaPlus;
	gNativeAdditionalUnlocksEnabled = vanillaPlus;
	gNative60FpsEnabled = vanillaPlus ? NativeFrameRate_Index(60) : 0;

	NativePhysics_SetEnabled(0);
	NativePhysics_SetDomain(NATIVE_PHYSICS_AI, 0);
	NativePhysics_SetDomain(NATIVE_PHYSICS_STEERING, 0);
	NativePhysics_SetDomain(NATIVE_PHYSICS_COLLISION, vanillaPlus);

#ifndef __vita__
	gNativeColorDepth = vanillaPlus ? NATIVE_COLOR_DEPTH_TRUE : NATIVE_COLOR_DEPTH_15BIT;
	gNativePs1ResolutionEnabled = !vanillaPlus;
	gNativeAntiAliasingMode = vanillaPlus ? NATIVE_AA_FXAA : NATIVE_AA_OFF;
	gNativeDitheringEnabled = !vanillaPlus;
	gNativeMaxLodEnabled = vanillaPlus;
	gNativeDepthBufferEnabled = vanillaPlus;
	g_cfg_bilinearFiltering = 0;
	gNativePgxpMode = NATIVE_PGXP_MODE_OFF;
#if NATIVE_PGXP_SUPPORTED
	if (vanillaPlus) gNativePgxpMode = NATIVE_PGXP_MODE_PERSPECTIVE;
#endif
	gNativePgxpIntegerNclipEnabled = 0;
	gNativeHdPauseMode = turbo ? 2 : 0;
	// Modern Minimap controls both the map and the markers, so it covers Precise Minimap too.
	gNativeModernMapEnabled = turbo;
	gNativePreciseMinimapEnabled = 0;
	gNativeFont = turbo ? NativeFont_GetDefault() : NATIVE_FONT_ORIGINAL;
	gNativeModernHudIconsEnabled = turbo;
#if NATIVE_DRAW3D_SUPPORTED
	gNativeRendererMode = turbo ? NATIVE_RENDERER_NATIVE : NATIVE_RENDERER_CLASSIC;
#endif
	if (gNativeModernMapEnabled) NativeMinimap_Prepare();
#endif
}

static const char *const s_nativePresetBlurb[6][4][4] =
{
	{
		{"THE ORIGINAL PS1 EXPERIENCE", "30 FPS, 15-BIT COLOUR, DITHERING", "RETAIL AI, ENGINES AND UNLOCKS", "NO ENHANCEMENTS"},
		{"THE ORIGINAL LOOK, MODERNISED", "24-BIT COLOUR, PGXP, 60 FPS, FXAA", "MAX LOD, DEPTH BUFFER, NO DITHERING", "ENGINE SELECTION, EXTRA UNLOCKS, SMOOTH COLLISIONS"},
		{"EVERYTHING VANILLA+ HAS, AND MORE", "NATIVE 3D RENDERER, HD SMOOTH PAUSE SCREEN", "MODERN MINIMAP, CUSTOM FONT, MODERN HUD ICONS", "EXTENDED AI RACERS"},
		{"CONFIGURE EVERYTHING YOURSELF", "STARTS FROM THE VANILLA+ SETTINGS", "OPENS THE OPTIONS MENU", ""},
	},
	{
		{"L'EXPERIENCE PS1 D'ORIGINE", "30 IPS, COULEURS 15 BITS, TRAMAGE", "IA, MOTEURS ET DEBLOCAGES D'ORIGINE", "AUCUNE AMELIORATION"},
		{"LE LOOK D'ORIGINE, MODERNISE", "COULEURS 24 BITS, PGXP, 60 IPS, FXAA", "DETAIL MAX, PROFONDEUR, SANS TRAMAGE", "CHOIX DU MOTEUR, DEBLOCAGES, COLLISIONS LISSEES"},
		{"TOUT VANILLA+, ET BIEN PLUS", "MOTEUR 3D NATIF, PAUSE HD FLUIDE", "MINI-CARTE MODERNE, POLICE, ICONES HUD", "IA ETENDUE"},
		{"CONFIGUREZ TOUT VOUS-MEME", "PART DES REGLAGES VANILLA+", "OUVRE LE MENU OPTIONS", ""},
	},
	{
		{"DAS ORIGINALE PS1-ERLEBNIS", "30 FPS, 15-BIT-FARBEN, DITHERING", "ORIGINAL-KI, MOTOREN, FREISCHALTUNGEN", "KEINE VERBESSERUNGEN"},
		{"ORIGINAL-LOOK, MODERNISIERT", "24-BIT, PGXP, 60 FPS, FXAA", "MAX. DETAILS, TIEFENPUFFER, KEIN DITHERING", "MOTORWAHL, EXTRA-FREISCHALTUNGEN, GLATTE KOLLISIONEN"},
		{"ALLES AUS VANILLA+ UND MEHR", "NATIVER 3D-RENDERER, HD-PAUSENBILDSCHIRM", "MODERNE MINIKARTE, EIGENE SCHRIFT, HUD-SYMBOLE", "ERWEITERTE KI-RENNFAHRER"},
		{"ALLES SELBST EINSTELLEN", "BASIERT AUF VANILLA+", "OEFFNET DAS OPTIONSMENUE", ""},
	},
	{
		{"L'ESPERIENZA PS1 ORIGINALE", "30 FPS, COLORI A 15 BIT, DITHERING", "IA, MOTORI E SBLOCCHI ORIGINALI", "NESSUN MIGLIORAMENTO"},
		{"L'ASPETTO ORIGINALE, MODERNIZZATO", "24 BIT, PGXP, 60 FPS, FXAA", "DETTAGLI MAX, PROFONDITA, NIENTE DITHERING", "SCELTA MOTORE, SBLOCCHI EXTRA, COLLISIONI FLUIDE"},
		{"TUTTO DI VANILLA+ E ALTRO", "RENDERER 3D NATIVO, PAUSA HD FLUIDA", "MINIMAPPA MODERNA, FONT, ICONE HUD", "IA ESTESA"},
		{"CONFIGURA TUTTO TU", "PARTE DALLE IMPOSTAZIONI VANILLA+", "APRE IL MENU OPZIONI", ""},
	},
	{
		{"LA EXPERIENCIA ORIGINAL DE PS1", "30 FPS, COLOR DE 15 BITS, DITHERING", "IA, MOTORES Y DESBLOQUEOS ORIGINALES", "SIN MEJORAS"},
		{"EL ASPECTO ORIGINAL, MODERNIZADO", "24 BITS, PGXP, 60 FPS, FXAA", "DETALLE MAX, PROFUNDIDAD, SIN DITHERING", "ELECCION DE MOTOR, DESBLOQUEOS, COLISIONES SUAVES"},
		{"TODO DE VANILLA+ Y MAS", "RENDERIZADO 3D NATIVO, PAUSA HD SUAVE", "MINIMAPA MODERNO, FUENTE, ICONOS HUD", "IA EXTENDIDA"},
		{"CONFIGURA TODO TU MISMO", "PARTE DE LOS AJUSTES VANILLA+", "ABRE EL MENU DE OPCIONES", ""},
	},
	{
		{"DE ORIGINELE PS1-BELEVING", "30 FPS, 15-BITS KLEUR, DITHERING", "ORIGINELE AI, MOTOREN EN UNLOCKS", "GEEN VERBETERINGEN"},
		{"DE ORIGINELE LOOK, GEMODERNISEERD", "24-BITS, PGXP, 60 FPS, FXAA", "MAX. DETAILS, DIEPTEBUFFER, GEEN DITHERING", "MOTORKEUZE, EXTRA UNLOCKS, SOEPELE BOTSINGEN"},
		{"ALLES VAN VANILLA+ EN MEER", "NATIEVE 3D-RENDERER, HD-PAUZESCHERM", "MODERNE MINIKAART, EIGEN FONT, HUD-ICONEN", "UITGEBREIDE AI-RACERS"},
		{"STEL ALLES ZELF IN", "BEGINT MET DE VANILLA+-INSTELLINGEN", "OPENT HET OPTIESMENU", ""},
	},
};

static const char *const s_nativePresetHeader[6] =
{
	"CHOOSE YOUR EXPERIENCE",
	"CHOISISSEZ VOTRE EXPERIENCE",
	"WAEHLE DEIN ERLEBNIS",
	"SCEGLI LA TUA ESPERIENZA",
	"ELIGE TU EXPERIENCIA",
	"KIES JE BELEVING",
};

static const char *const s_nativePresetFooter[6][4] =
{
	{"EVERY FEATURE CAN BE TOGGLED INDIVIDUALLY", "FROM THE OPTIONS MENU AT ANY TIME", "I RECOMMEND TRYING THEM ALL TO FIND", "A COMBO THAT SUITS YOU"},
	{"CHAQUE FONCTION PEUT ETRE ACTIVEE", "INDIVIDUELLEMENT DANS LES OPTIONS", "JE VOUS RECOMMANDE DE TOUT ESSAYER POUR", "TROUVER LA COMBINAISON QUI VOUS CONVIENT"},
	{"JEDE FUNKTION LAESST SICH JEDERZEIT EINZELN", "IM OPTIONSMENUE UMSCHALTEN", "ICH EMPFEHLE, ALLES AUSZUPROBIEREN,", "UM DIE PASSENDE KOMBINATION ZU FINDEN"},
	{"OGNI FUNZIONE PUO ESSERE ATTIVATA SINGOLARMENTE", "DAL MENU OPZIONI IN QUALSIASI MOMENTO", "TI CONSIGLIO DI PROVARLE TUTTE PER TROVARE", "LA COMBINAZIONE GIUSTA PER TE"},
	{"CADA FUNCION SE PUEDE ACTIVAR POR SEPARADO", "DESDE EL MENU DE OPCIONES EN CUALQUIER MOMENTO", "TE RECOMIENDO PROBARLAS TODAS PARA ENCONTRAR", "LA COMBINACION QUE MEJOR TE VAYA"},
	{"ELKE FUNCTIE KAN OP ELK MOMENT APART", "IN HET OPTIESMENU WORDEN AAN- OF UITGEZET", "IK RAAD AAN ALLES UIT TE PROBEREN OM", "DE COMBINATIE TE VINDEN DIE BIJ JE PAST"},
};

static void MM_NativePresetMenuProc(struct RectMenu *menu)
{
	if (menu->funcState == RECTMENU_FUNC_STATE_DRAW)
	{
		int row = (menu->rowSelected >= 0) && (menu->rowSelected < 4) ? menu->rowSelected : 0;
		int lang = ((cfg_language >= 2) && (cfg_language <= 7)) ? cfg_language - 2 : 0;
		DecalFont_DrawLine((char *)s_nativePresetHeader[lang], 0x100, 24, FONT_BIG, JUSTIFY_CENTER | ORANGE);
		DecalFont_DrawLine((char *)s_nativePresetBlurb[lang][row][0], 0x100, 150, FONT_SMALL, JUSTIFY_CENTER | ORANGE);
		for (int i = 1; i < 4; i++)
		{
			DecalFont_DrawLine((char *)s_nativePresetBlurb[lang][row][i], 0x100, 150 + i * 14, FONT_SMALL, JUSTIFY_CENTER | PERIWINKLE);
		}
		DecalFont_DrawLine((char *)s_nativePresetFooter[lang][0], 0x100, 208, FONT_SMALL, JUSTIFY_CENTER | WHITE);
		DecalFont_DrawLine((char *)s_nativePresetFooter[lang][1], 0x100, 220, FONT_SMALL, JUSTIFY_CENTER | WHITE);
		DecalFont_DrawLine((char *)s_nativePresetFooter[lang][2], 0x100, 236, FONT_SMALL, JUSTIFY_CENTER | ORANGE);
		DecalFont_DrawLine((char *)s_nativePresetFooter[lang][3], 0x100, 248, FONT_SMALL, JUSTIFY_CENTER | ORANGE);
		return;
	}

	if (menu->funcState != RECTMENU_FUNC_STATE_INPUT)
	{
		return;
	}

	if (menu == &s_nativePresetOptionsMenu)
	{
		// Reopened from Options: Custom keeps the current settings, back changes nothing.
		if ((menu->rowSelected >= 0) && (menu->rowSelected != NATIVE_PRESET_CUSTOM))
		{
			MM_NativeApplyPreset(menu->rowSelected);
			save_config();
		}
		struct RectMenu *parent = s_nativePresetReturnParent;
		s_nativeOptionsMenu.state = s_nativePresetReturnState;
		s_nativeOptionsMenu.posX_curr = s_nativePresetReturnPosX;
		s_nativeOptionsMenu.posY_curr = s_nativePresetReturnPosY;
		s_nativeOptionsMenu.rowSelected = s_nativePresetReturnRow;
		P32_SET(s_nativeOptionsMenu.ptrNextBox_InHierarchy, NULL);
		P32_SET(s_nativeOptionsMenu.ptrPrevBox_InHierarchy, parent);
		if (parent != NULL)
		{
			P32_SET(parent->ptrNextBox_InHierarchy, &s_nativeOptionsMenu);
			parent->state = s_nativePresetReturnParentState;
		}
		P32_SET(sdata->ptrDesiredMenu, (parent != NULL) ? parent : &s_nativeOptionsMenu);
		return;
	}

	if (menu->rowSelected < 0)
	{
		return;
	}

	int preset = menu->rowSelected;
	MM_NativeApplyPreset(preset == NATIVE_PRESET_CUSTOM ? NATIVE_PRESET_VANILLA_PLUS : preset);
	gNativePresetPending = 0;
	save_config();

	if (preset == NATIVE_PRESET_CUSTOM)
	{
		// Open Options on its own; backing out returns to the main menu.
		MM_NativeOptionsOpenFromPreset();
		return;
	}
	P32_SET(sdata->ptrDesiredMenu, &D230.menuMainMenu);
}

static void MM_NativeExtraDifficultyPrepare(void)
{
	s_nativeExtraDifficultyMenu = (struct RectMenu)
	{
		.stringIndexTitle = LNG_DIFFICULTY,
		.state = CENTER_ON_X | USE_SMALL_FONT | BIG_TEXT_IN_TITLE,
	};
	P32_SET(s_nativeExtraDifficultyMenu.rows, s_nativeExtraDifficultyRows);
	P32_SET(s_nativeExtraDifficultyMenu.funcPtr, MM_MenuProc_Difficulty);
}

static void MM_NativeLanguageLoad(s16 row)
{
	if ((u16)row >= MM_NATIVE_LANGUAGE_COUNT)
	{
		row = 0;
	}

	cfg_language = s_nativeLanguageFileIndex[row];
	LOAD_LangFile(P32_GET(struct BigHeader *, sdata->ptrBigfile1), cfg_language);

	s_nativeLanguageRow = row;
	s_nativeLanguageChosen = 1;
	save_config();
}

// Fresh installs pick a settings preset right after the language.
static void MM_NativeLanguageBootDone(void)
{
	if (gNativePresetPending)
	{
		s_nativePresetMenu.state = RECTMENU_STATE_EXEC_CENTERED | RECTMENU_NATIVE_DRAW_CALLBACK;
		s_nativePresetMenu.rowSelected = 1;
		P32_SET(s_nativePresetMenu.ptrNextBox_InHierarchy, 0);
		P32_SET(s_nativePresetMenu.ptrPrevBox_InHierarchy, 0);
		P32_SET(sdata->ptrDesiredMenu, &s_nativePresetMenu);
	}
	else
	{
		P32_SET(sdata->ptrDesiredMenu, &D230.menuMainMenu);
	}
}

static void MM_NativeLanguageBootMenuProc(struct RectMenu *menu)
{
	if (menu->funcState == RECTMENU_FUNC_STATE_UPDATE)
	{
		if (P32_GET(struct GamepadSystem *, sdata->gGamepads)->anyoneHeldCurr != 0)
		{
			s_nativeLanguageTimer = FPS_DOUBLE(MM_NATIVE_LANGUAGE_TIMEOUT_FRAMES);
		}
		else if (s_nativeLanguageTimer > 0)
		{
			s_nativeLanguageTimer--;
		}

		if (s_nativeLanguageTimer == 0)
		{
			MM_NativeLanguageLoad(menu->rowSelected);
			MM_NativeLanguageBootDone();
		}
		return;
	}

	if ((menu->funcState != RECTMENU_FUNC_STATE_INPUT) || (menu->rowSelected < 0))
	{
		return;
	}

	MM_NativeLanguageLoad(menu->rowSelected);
	MM_NativeLanguageBootDone();
}

static void MM_NativeLanguageMainMenuProc(struct RectMenu *menu)
{
	if (menu->funcState != RECTMENU_FUNC_STATE_INPUT)
	{
		return;
	}

	struct RectMenu *parent = P32_GET(struct RectMenu *, menu->ptrPrevBox_InHierarchy);
	if (parent == NULL)
	{
		return;
	}

	if (menu->rowSelected >= 0)
	{
		MM_NativeLanguageLoad(menu->rowSelected);
	}

	parent->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
}

#if defined(__vita__)
static s16 MM_NativeAdhocLanguageRow(void)
{
	if ((cfg_language >= 2) && (cfg_language <= 7))
	{
		return (s16)(cfg_language - 2);
	}
	return 0;
}

static void MM_NativeAdhocApplyText(void)
{
	s16 languageRow = MM_NativeAdhocLanguageRow();

	if ((sdata->lngStrings == NULL) || (sdata->numLngStrings <= LNG_NA_242))
	{
		return;
	}

	switch (s_nativeAdhocMenuStage)
	{
	case MM_NATIVE_ADHOC_STAGE_ROLE:
		sdata->lngStrings[LNG_NA_241] = (char *)s_nativeAdhocHostText[languageRow];
		sdata->lngStrings[LNG_NA_242] = (char *)s_nativeAdhocJoinText[languageRow];
		break;

	case MM_NATIVE_ADHOC_STAGE_WAIT:
		if (NativeAdhoc_GetRole() == NATIVE_ADHOC_ROLE_CLIENT)
		{
			sdata->lngStrings[LNG_NA_241] = (char *)NativeAdhoc_GetStatusText();
			sdata->lngStrings[LNG_NA_242] = (char *)s_nativeAdhocJoinText[languageRow];
			s_nativeAdhocWaitMenu.stringIndexTitle = LNG_NA_241;
		}
		else
		{
			sdata->lngStrings[LNG_NA_241] = (char *)s_nativeAdhocHostText[languageRow];
			sdata->lngStrings[LNG_NA_242] = (char *)NativeAdhoc_GetStatusText();
			s_nativeAdhocWaitMenu.stringIndexTitle = LNG_NA_242;
		}
		break;

	default:
		break;
	}
}

static void MM_NativeAdhocSetMainBreadcrumb(s16 stringIndex)
{
	s_nativeMainMenuBasic[MM_NATIVE_ADHOC_MAIN_ROW].stringIndex = stringIndex;
	s_nativeMainMenuWithScrapbook[MM_NATIVE_ADHOC_MAIN_ROW].stringIndex = stringIndex;
}

static void MM_NativeAdhocResetHierarchy(void)
{
	D230.menuMainMenu.state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
	D230.menuMainMenu.ptrNextBox_InHierarchy = NULL;

	s_nativeAdhocModeMenu.state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
	s_nativeAdhocModeMenu.ptrNextBox_InHierarchy = NULL;
	s_nativeAdhocModeMenu.ptrPrevBox_InHierarchy = NULL;
	s_nativeAdhocModeMenu.rowSelected = 0;

	s_nativeAdhocRoleMenu.state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
	s_nativeAdhocRoleMenu.ptrNextBox_InHierarchy = NULL;
	s_nativeAdhocRoleMenu.ptrPrevBox_InHierarchy = NULL;
	s_nativeAdhocRoleMenu.rowSelected = 0;

	s_nativeAdhocWaitMenu.state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
	s_nativeAdhocWaitMenu.ptrNextBox_InHierarchy = NULL;
	s_nativeAdhocWaitMenu.ptrPrevBox_InHierarchy = NULL;
	s_nativeAdhocWaitMenu.rowSelected = 0;
	s_nativeAdhocWaitMenu.stringIndexTitle = LNG_NA_241;
	s_nativeAdhocSuppressInputFrames = 0;
}

static void MM_NativeAdhocReturnToMain(void)
{
	NativeAdhoc_Shutdown();
	MM_NativeAdhocResetHierarchy();
	MM_NativeAdhocSetMainBreadcrumb(NATIVE_MENU_STRING_ADHOC);
	s_nativeAdhocGameMode = NATIVE_ADHOC_GAME_MODE_ARCADE;
	s_nativeAdhocMenuStage = MM_NATIVE_ADHOC_STAGE_MAIN;
	RECTMENU_ClearInput();
	sdata->ptrDesiredMenu = &D230.menuMainMenu;
}

static int MM_NativeAdhocPollWait(struct GameTracker *gGT)
{
	int dialogWasRunning;

	if (s_nativeAdhocMenuStage != MM_NATIVE_ADHOC_STAGE_WAIT)
	{
		return 0;
	}

	dialogWasRunning = NativeAdhoc_IsDialogRunning();
	NativeAdhoc_Update();
	MM_NativeAdhocApplyText();

	if (dialogWasRunning || NativeAdhoc_IsDialogRunning())
	{
		s_nativeAdhocSuppressInputFrames = 2;
	}
	if (s_nativeAdhocSuppressInputFrames > 0)
	{
		RECTMENU_ClearInput();
		s_nativeAdhocSuppressInputFrames--;
	}

	if (NativeAdhoc_IsConnected())
	{
		int adhocGameMode = NativeAdhoc_GetGameMode();
		if ((adhocGameMode != NATIVE_ADHOC_GAME_MODE_ARCADE) && (adhocGameMode != NATIVE_ADHOC_GAME_MODE_VS))
		{
			MM_NativeAdhocReturnToMain();
			return 1;
		}

		s_nativeAdhocGameMode = (s16)adhocGameMode;
		gGT->gameMode1 &= ~(BATTLE_MODE | ADVENTURE_MODE | TIME_TRIAL | RELIC_RACE | ADVENTURE_ARENA | ARCADE_MODE | ADVENTURE_CUP);
		gGT->gameMode2 &= ~(CUP_ANY_KIND);
		if (adhocGameMode == NATIVE_ADHOC_GAME_MODE_ARCADE)
		{
			gGT->gameMode1 |= ARCADE_MODE;
		}
		gGT->numPlyrNextGame = 2;
		gGT->numLaps = MM_DEFAULT_LAP_COUNT;
		MM_NativeAdhocResetHierarchy();
		MM_NativeAdhocSetMainBreadcrumb(adhocGameMode == NATIVE_ADHOC_GAME_MODE_ARCADE ? LNG_ARCADE : LNG_VS);
		s_nativeAdhocMenuStage = MM_NATIVE_ADHOC_STAGE_GAME_FLOW;
		RECTMENU_ClearInput();

		if (adhocGameMode == NATIVE_ADHOC_GAME_MODE_VS)
		{
			D230.characterSelectTransitionState = EXITING_MENU;
			D230.titleMenuState = TITLE_MENU_STATE_EXITING;
			D230.desiredMenuIndex = MM_EXIT_ROUTE_CHARACTER_SELECT;
			return 1;
		}

		MM_NativeExtraDifficultyPrepare();
		s_nativeExtraDifficultyMenu.ptrPrevBox_InHierarchy = &D230.menuMainMenu;
		s_nativeExtraDifficultyMenu.ptrNextBox_InHierarchy = NULL;
		s_nativeExtraDifficultyMenu.rowSelected = 0;

		D230.menuMainMenu.ptrNextBox_InHierarchy = &s_nativeExtraDifficultyMenu;
		D230.menuMainMenu.state |= ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY;
		D230.characterSelectTransitionState = IN_MENU;
		return 1;
	}

	if (NativeAdhoc_GetStatus() == NATIVE_ADHOC_STATUS_ERROR)
	{
		MM_NativeAdhocReturnToMain();
		return 1;
	}

	return 0;
}

static void MM_NativeAdhocModeProc(struct RectMenu *menu)
{
	MM_NativeAdhocApplyText();

	if (menu->funcState != RECTMENU_FUNC_STATE_INPUT)
	{
		return;
	}

	if (menu->rowSelected < 0)
	{
		s_nativeAdhocMenuStage = MM_NATIVE_ADHOC_STAGE_MAIN;
		if (menu->ptrPrevBox_InHierarchy != NULL)
		{
			menu->ptrPrevBox_InHierarchy->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
		}
		return;
	}

	if ((menu->rowSelected == 0) || (menu->rowSelected == 1))
	{
		s_nativeAdhocGameMode = menu->rowSelected == 0 ? NATIVE_ADHOC_GAME_MODE_ARCADE : NATIVE_ADHOC_GAME_MODE_VS;
		s_nativeAdhocMenuStage = MM_NATIVE_ADHOC_STAGE_ROLE;
		MM_NativeAdhocApplyText();
		s_nativeAdhocRoleMenu.rowSelected = 0;
		menu->ptrNextBox_InHierarchy = &s_nativeAdhocRoleMenu;
		menu->state |= ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY;
	}
}

static void MM_NativeAdhocRoleProc(struct RectMenu *menu)
{
	MM_NativeAdhocApplyText();

	if (menu->funcState != RECTMENU_FUNC_STATE_INPUT)
	{
		return;
	}

	if (menu->rowSelected < 0)
	{
		s_nativeAdhocMenuStage = MM_NATIVE_ADHOC_STAGE_MODE;
		if (menu->ptrPrevBox_InHierarchy != NULL)
		{
			menu->ptrPrevBox_InHierarchy->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
		}
		return;
	}

	if ((menu->rowSelected == 0) || (menu->rowSelected == 1))
	{
		int role = menu->rowSelected == 0 ? NATIVE_ADHOC_ROLE_HOST : NATIVE_ADHOC_ROLE_CLIENT;
		if (!NativeAdhoc_Begin(role, s_nativeAdhocGameMode))
		{
			MM_NativeAdhocReturnToMain();
			return;
		}

		s_nativeAdhocMenuStage = MM_NATIVE_ADHOC_STAGE_WAIT;
		s_nativeAdhocSuppressInputFrames = 2;
		MM_NativeAdhocApplyText();
		s_nativeAdhocWaitMenu.rowSelected = 0;
		menu->ptrNextBox_InHierarchy = &s_nativeAdhocWaitMenu;
		menu->state |= ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY;
	}
}

static void MM_NativeAdhocWaitProc(struct RectMenu *menu)
{
	if ((menu->funcState == RECTMENU_FUNC_STATE_INPUT) && (menu->rowSelected <= 0))
	{
		MM_NativeAdhocReturnToMain();
	}
}
#endif

static void MM_NativeTimeTrialMenuProc(struct RectMenu *menu)
{
	if (menu->funcState == RECTMENU_FUNC_STATE_UPDATE)
	{
		MM_NativeTimeTrialRefreshOnlineRow();
		return;
	}

	if (menu->funcState != RECTMENU_FUNC_STATE_INPUT)
	{
		return;
	}

	struct RectMenu *parent = P32_GET(struct RectMenu *, menu->ptrPrevBox_InHierarchy);
	if (menu->rowSelected < 0)
	{
		if (parent != NULL)
		{
			parent->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
		}
		return;
	}

	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);
	s16 choose = P32_GET(struct MenuRow *, menu->rows)[menu->rowSelected].stringIndex & MENU_ROW_LNG_MASK;

	gGT->gameMode1 &= ~(TIME_TRIAL | RELIC_RACE);
	gNativeGhostReplayMode = 0;
	gNativeOnlineLeaderboardMode = 0;
	gNativeRelicRaceMode = 0;
	gNativeRelicRaceResultTier = -1;
	NativeGhostInput_ClearSelection();

	if (choose == LNG_TIME_TRIAL)
	{
		D230.titleMenuState = TITLE_MENU_STATE_EXITING;
		D230.desiredMenuIndex = MM_EXIT_ROUTE_CHARACTER_SELECT;
		gGT->numPlyrNextGame = 1;
		gGT->gameMode1 |= TIME_TRIAL;
		gGT->gameMode2 &= ~(CHEAT_WUMPA | CHEAT_MASK | CHEAT_TURBO | CHEAT_ENGINE | CHEAT_BOMBS);
		return;
	}

	if (choose == LNG_RELIC_RACE)
	{
		gNativeRelicRaceMode = 1;
		D230.titleMenuState = TITLE_MENU_STATE_EXITING;
		D230.desiredMenuIndex = MM_EXIT_ROUTE_CHARACTER_SELECT;
		gGT->numPlyrNextGame = 1;
		gGT->gameMode1 &= ~TIME_TRIAL;
		gGT->gameMode1 |= RELIC_RACE;
		gGT->gameMode2 &= ~(CHEAT_WUMPA | CHEAT_MASK | CHEAT_TURBO | CHEAT_ENGINE | CHEAT_BOMBS);
		return;
	}

	if (choose == NATIVE_MENU_STRING_GHOST_REPLAY)
	{
		gNativeGhostReplayMode = 1;
		D230.titleMenuState = TITLE_MENU_STATE_EXITING;
		D230.desiredMenuIndex = MM_EXIT_ROUTE_GHOST_REPLAY;
		gGT->numPlyrNextGame = 1;
		gGT->gameMode1 |= TIME_TRIAL;
		gGT->gameMode2 &= ~(CHEAT_WUMPA | CHEAT_MASK | CHEAT_TURBO | CHEAT_ENGINE | CHEAT_BOMBS);
		return;
	}

	if (choose == LNG_HIGH_SCORE)
	{
		D230.desiredMenuIndex = MM_EXIT_ROUTE_HIGH_SCORE;
		D230.titleMenuState = TITLE_MENU_STATE_EXITING;
		return;
	}

#if CTR_NATIVE_HAS_LEADERBOARD
	if (choose == NATIVE_MENU_STRING_ONLINE_LEADERBOARD)
	{
		gNativeOnlineLeaderboardMode = 1;
		NativeLeaderboard_RequestRefresh();
		D230.desiredMenuIndex = MM_EXIT_ROUTE_HIGH_SCORE;
		D230.titleMenuState = TITLE_MENU_STATE_EXITING;
	}
#endif
}

static b32 MM_NativeOptionsInGame(void)
{
	return P32_GET(struct GameTracker *, sdata->gGT)->levelID != MAIN_MENU_LEVEL;
}

static b32 MM_NativeOptionsRowLocked(s16 stringIndex, b32 inGame)
{
	const s16 row = stringIndex & MENU_ROW_LNG_MASK;
	if ((row == NATIVE_MENU_STRING_DEPTH_BUFFER) || (row == NATIVE_MENU_STRING_MAX_LOD))
	{
		// Native 3D always depth-tests and draws at max detail.
		return NATIVE_DRAW3D_ACTIVE();
	}
	if (!inGame)
	{
		return false;
	}
	switch (stringIndex & MENU_ROW_LNG_MASK)
	{
	case LNG_LANGUAGE:
	case NATIVE_MENU_STRING_MIRROR_MODE:
	case NATIVE_MENU_STRING_AI_RACERS:
	case NATIVE_MENU_STRING_PRESET:
		return true;
	default:
		return false;
	}
}

static void MM_NativeOptionsApplyLocks(struct MenuRow *rows, b32 inGame)
{
	for (struct MenuRow *row = rows; row->stringIndex != RECTMENU_STRING_NONE; row++)
	{
		b32 locked = MM_NativeOptionsRowLocked(row->stringIndex, inGame);
		if (locked)
		{
			row->stringIndex |= MENU_ROW_LOCKED;
		}
		else
		{
			row->stringIndex &= ~MENU_ROW_LOCKED;
		}
	}
}

// Configure categories and lock settings that cannot change during a race.
// Gamepad / Vibration is an additional category available from pause.
static void MM_NativeOptionsConfigureRows(b32 inGame)
{
	static int baseCount = 0;
	if (baseCount == 0)
	{
		while (s_nativeOptionsRows[baseCount].stringIndex != RECTMENU_STRING_NONE)
		{
			baseCount++;
		}
	}

	struct MenuRow *rows = s_nativeOptionsRows;
	if (inGame)
	{
		rows[baseCount] = (struct MenuRow){.stringIndex = NATIVE_MENU_STRING_GAMEPAD};
		rows[baseCount + 1] = (struct MenuRow){.stringIndex = RECTMENU_STRING_NONE};
	}
	else
	{
		rows[baseCount] = (struct MenuRow){.stringIndex = RECTMENU_STRING_NONE};
	}

	struct MenuRow *menus[] = {
	    s_nativeOptionsRows,  s_nativeDisplayRows,      s_nativeAudioRows, s_nativeGameplayRows, s_nativeUiRows,
#ifndef __vita__
	    s_nativeGraphicsRows, s_nativeExperimentalRows,
#endif
	};
	for (unsigned int menu = 0; menu < sizeof(menus) / sizeof(menus[0]); menu++)
	{
		struct MenuRow *settings = menus[menu];
		int count = 0;
		while (settings[count].stringIndex != RECTMENU_STRING_NONE) count++;
		for (int i = 0; i < count; i++)
		{
			settings[i].rowOnPressUp = (char)((i + count - 1) % count);
			settings[i].rowOnPressDown = (char)((i + 1) % count);
			settings[i].rowOnPressLeft = (char)i;
			settings[i].rowOnPressRight = (char)i;
		}
		MM_NativeOptionsApplyLocks(settings, inGame);
	}
}

// Opens the full options menu from the in-game pause menu.
void MM_NativeOptions_OpenFromPause(void)
{
	MM_NativeOptionsConfigureRows(1);
	s_nativeOptionsMenu.rowSelected = 0;
	s_nativeOptionsMenu.posX_curr = 256;
	s_nativeOptionsMenu.posY_curr = 120;
	s_nativeOptionsMenu.state = CENTER_ON_COORDS | USE_SMALL_FONT | BIG_TEXT_IN_TITLE;
	P32_SET(s_nativeOptionsMenu.ptrNextBox_InHierarchy, NULL);
	P32_SET(s_nativeOptionsMenu.ptrPrevBox_InHierarchy, NULL);
	P32_SET(sdata->ptrDesiredMenu, &s_nativeOptionsMenu);
}

static void MM_NativeOptionsOpenFromPreset(void)
{
	s_nativePresetOptionsActive = 1;
	MM_NativeOptionsConfigureRows(0);
	s_nativeOptionsMenu.rowSelected = 0;
	s_nativeOptionsMenu.posX_curr = 256;
	s_nativeOptionsMenu.posY_curr = 120;
	s_nativeOptionsMenu.state = CENTER_ON_COORDS | USE_SMALL_FONT | BIG_TEXT_IN_TITLE;
	P32_SET(s_nativeOptionsMenu.ptrNextBox_InHierarchy, NULL);
	P32_SET(s_nativeOptionsMenu.ptrPrevBox_InHierarchy, NULL);
	P32_SET(sdata->ptrDesiredMenu, &s_nativeOptionsMenu);
}

static void MM_NativeOptionsMenuProc(struct RectMenu *menu)
{
	if (menu->funcState == RECTMENU_FUNC_STATE_UPDATE)
	{
		MM_NativeOptionsApplyLocks(P32_GET(struct MenuRow *, menu->rows), MM_NativeOptionsInGame());
		return;
	}

	if (menu->funcState != RECTMENU_FUNC_STATE_INPUT)
	{
		return;
	}

	struct RectMenu *parent = P32_GET(struct RectMenu *, menu->ptrPrevBox_InHierarchy);
	if (menu->rowSelected < 0)
	{
		if (parent != NULL)
		{
			parent->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
		}
		else if (s_nativePresetOptionsActive)
		{
			// opened from the first-launch preset menu, continue to the main menu
			s_nativePresetOptionsActive = 0;
			P32_SET(sdata->ptrDesiredMenu, &D230.menuMainMenu);
		}
		else if (MM_NativeOptionsInGame())
		{
			// opened from the pause menu, go back to it
			P32_SET(sdata->ptrDesiredMenu, MainFreeze_GetMenuPtr());
		}
		return;
	}

	MM_NativeOptionsApplyLocks(P32_GET(struct MenuRow *, menu->rows), MM_NativeOptionsInGame());

	// left/right bypass the row lock check done for confirm
	if ((P32_GET(struct MenuRow *, menu->rows)[menu->rowSelected].stringIndex & MENU_ROW_LOCKED) != 0)
	{
		return;
	}

	s16 choose = P32_GET(struct MenuRow *, menu->rows)[menu->rowSelected].stringIndex & MENU_ROW_LNG_MASK;
	u32 button = sdata->buttonTapPerPlayer[0];

	if (choose == NATIVE_MENU_STRING_PRESET)
	{
		s_nativePresetOptionsMenu.rowSelected = 1;
		P32_SET(s_nativePresetOptionsMenu.ptrNextBox_InHierarchy, NULL);
		P32_SET(s_nativePresetOptionsMenu.ptrPrevBox_InHierarchy, NULL);
		s_nativePresetReturnParent = P32_GET(struct RectMenu *, menu->ptrPrevBox_InHierarchy);
		s_nativePresetReturnParentState = (s_nativePresetReturnParent != NULL) ? s_nativePresetReturnParent->state : 0;
		s_nativePresetReturnState = menu->state;
		s_nativePresetReturnPosX = menu->posX_curr;
		s_nativePresetReturnPosY = menu->posY_curr;
		s_nativePresetReturnRow = menu->rowSelected;
		// opened standalone (not as a hierarchy child) so it keeps its own centered position
		P32_SET(sdata->ptrDesiredMenu, &s_nativePresetOptionsMenu);
		return;
	}

	if (choose == NATIVE_MENU_STRING_GAMEPAD)
	{
		P32_SET(sdata->ptrDesiredMenu, &data.menuRacingWheelConfig);
		data.menuRacingWheelConfig.rowSelected = 8;
		return;
	}

	if ((choose >= NATIVE_MENU_STRING_AUDIO_FX) && (choose <= NATIVE_MENU_STRING_AUDIO_VOICE))
	{
		if ((button & (BTN_LEFT | BTN_RIGHT)) != 0)
		{
			int volumeType = choose - NATIVE_MENU_STRING_AUDIO_FX;
			int volume = howl_VolumeGet(volumeType) & 0xff;
			volume += (button & BTN_LEFT) ? -16 : 16;
			if (volume < 0) volume = 0;
			if (volume > 0xff) volume = 0xff;
			howl_VolumeSet(volumeType, (u8)volume);
			OtherFX_Play(0, 1);
		}
		return;
	}

	if (choose == NATIVE_MENU_STRING_AUDIO_MODE)
	{
		if (button & BTN_LEFT)
		{
			howl_ModeSet(0);
		}
		else if (button & BTN_RIGHT)
		{
			howl_ModeSet(1);
		}
		else
		{
			howl_ModeSet(howl_ModeGet() == 0);
		}
		return;
	}

	if (choose == LNG_LANGUAGE)
	{
		s_nativeLanguageMainMenu.rowSelected = s_nativeLanguageRow;
		P32_SET(s_nativeLanguageMainMenu.ptrNextBox_InHierarchy, NULL);
		P32_SET(s_nativeLanguageMainMenu.ptrPrevBox_InHierarchy, menu);

		P32_SET(menu->ptrNextBox_InHierarchy, &s_nativeLanguageMainMenu);
		menu->state |= ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY;
		return;
	}

	if (choose == NATIVE_MENU_STRING_CONTROLS)
	{
		s_nativeControlsMenu.rowSelected = 1;
		s_nativeControlsMenu.posX_curr = 256;
		s_nativeControlsMenu.posY_curr = 120;
		s_nativeControlsMenu.state = CENTER_ON_COORDS | USE_SMALL_FONT | BIG_TEXT_IN_TITLE | RECTMENU_DRAW_CALLBACK_FLAGS;
		P32_SET(s_nativeControlsMenu.ptrNextBox_InHierarchy, NULL);
		P32_SET(s_nativeControlsMenu.ptrPrevBox_InHierarchy, menu);
		gNativeControlsSelectedColumn = 0;
		gNativeControlsSelectedAction = 0;
		gNativeControlsCaptureActive = 0;
		s_nativeControlsWaitForRelease = 0;

		P32_SET(sdata->ptrDesiredMenu, &s_nativeControlsMenu);
		return;
	}

	if (choose == NATIVE_MENU_STRING_CHEATS)
	{
		s_nativeCheatsMenu.rowSelected = 0;
		s_nativeCheatsMenu.state = CENTER_ON_X | USE_SMALL_FONT;
		P32_SET(s_nativeCheatsMenu.ptrNextBox_InHierarchy, NULL);
		P32_SET(s_nativeCheatsMenu.ptrPrevBox_InHierarchy, menu);
		P32_SET(menu->ptrNextBox_InHierarchy, &s_nativeCheatsMenu);
		menu->state |= ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY;
		return;
	}

	if (choose == NATIVE_MENU_STRING_MIRROR_MODE)
	{
		gNativeMirrorModeEnabled ^= 1;
		save_config();
		return;
	}

	if (choose == NATIVE_MENU_STRING_FRAME_RATE)
	{
		gNative60FpsEnabled = (gNative60FpsEnabled + ((button & BTN_LEFT) ? NATIVE_FRAME_RATE_COUNT - 1 : 1)) % NATIVE_FRAME_RATE_COUNT;
		save_config();
		return;
	}

	if (choose == NATIVE_MENU_STRING_DEFAULT_CAMERA)
	{
		gNativeDefaultCameraFar ^= 1;
		save_config();
		return;
	}

	if (choose == NATIVE_MENU_STRING_DEFAULT_HUD)
	{
		gNativeDefaultHudSpeedometer ^= 1;
		save_config();
		return;
	}

	if (choose == NATIVE_MENU_STRING_SKIP_MASK_HINTS)
	{
		gNativeSkipMaskHints ^= 1;
		save_config();
		return;
	}

	if (choose == NATIVE_MENU_STRING_AI_RACERS)
	{
		if (button & BTN_LEFT)
		{
			gNativeAIRacersMode--;
			if (gNativeAIRacersMode < NATIVE_AI_RACERS_RETAIL)
			{
				gNativeAIRacersMode = NATIVE_AI_RACERS_EXTENDED_CUSTOM;
			}
			OtherFX_Play(0, 1);
		}
		else
		{
			gNativeAIRacersMode++;
			if (gNativeAIRacersMode >= NATIVE_AI_RACERS_MODE_COUNT)
			{
				gNativeAIRacersMode = NATIVE_AI_RACERS_RETAIL;
			}
			if (button & BTN_RIGHT)
			{
				OtherFX_Play(0, 1);
			}
		}
		save_config();
		return;
	}
	if ((choose == NATIVE_MENU_STRING_DISPLAY) || (choose == NATIVE_MENU_STRING_AUDIO) || (choose == NATIVE_MENU_STRING_GAMEPLAY) ||
#ifndef __vita__
	    (choose == NATIVE_MENU_STRING_GRAPHICS) || (choose == NATIVE_MENU_STRING_EXPERIMENTAL) ||
#endif
	    (choose == NATIVE_MENU_STRING_UI))
	{
		struct RectMenu *submenu = choose == NATIVE_MENU_STRING_DISPLAY ? &s_nativeDisplayMenu
		                           : choose == NATIVE_MENU_STRING_AUDIO ? &s_nativeAudioMenu
		                           :
#ifndef __vita__
		                           choose == NATIVE_MENU_STRING_GRAPHICS       ? &s_nativeGraphicsMenu
		                           : choose == NATIVE_MENU_STRING_EXPERIMENTAL ? &s_nativeExperimentalMenu
		                           :
#endif
		                           choose == NATIVE_MENU_STRING_GAMEPLAY ? &s_nativeGameplayMenu
		                                                                 : &s_nativeUiMenu;
		submenu->rowSelected = 0;
		submenu->posY_curr = 0;
		submenu->state = CENTER_ON_X | USE_SMALL_FONT;
		P32_SET(submenu->ptrNextBox_InHierarchy, NULL);
		P32_SET(submenu->ptrPrevBox_InHierarchy, menu);
		P32_SET(menu->ptrNextBox_InHierarchy, submenu);
		menu->state |= ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY;
		return;
	}
#ifndef __vita__
	if (choose == NATIVE_MENU_STRING_MODERN_MAP)
	{
		gNativeModernMapEnabled ^= 1;
		gNativePreciseMinimapEnabled = 0; // Modern Minimap controls both maps and markers.
		if (gNativeModernMapEnabled) NativeMinimap_Prepare();
		save_config();
		return;
	}

	if (choose == NATIVE_MENU_STRING_MODERN_HUD_ICONS)
	{
		gNativeModernHudIconsEnabled ^= 1;
		save_config();
		return;
	}

	if (choose == NATIVE_MENU_STRING_ADDITIONAL_UNLOCKS)
	{
		gNativeAdditionalUnlocksEnabled ^= 1;
		save_config();
		return;
	}

	if (choose == NATIVE_MENU_STRING_BOOST_COUNTER)
	{
		gNativeCheatConfigMask ^= CHEAT_TURBOCOUNT;
		NativeCheat_ApplyConfigured();
		save_config();
		return;
	}

	if (choose == NATIVE_MENU_STRING_ENGINE_SELECTION)
	{
		gNativeEngineSelectionEnabled ^= 1;
		save_config();
		return;
	}

	if (choose == NATIVE_MENU_STRING_KART_HUE)
	{
		if (button & BTN_LEFT)
		{
			gNativeKartHue = (gNativeKartHue + NATIVE_KART_HUE_STEPS - 1) % NATIVE_KART_HUE_STEPS;
			OtherFX_Play(0, 1);
		}
		else
		{
			gNativeKartHue = (gNativeKartHue + 1) % NATIVE_KART_HUE_STEPS;
			if (button & BTN_RIGHT)
			{
				OtherFX_Play(0, 1);
			}
		}
		save_config();
		return;
	}

	if (choose == NATIVE_MENU_STRING_FONT)
	{
		if (button & BTN_LEFT)
		{
			gNativeFont = (gNativeFont + NATIVE_FONT_COUNT - 1) % NATIVE_FONT_COUNT;
			OtherFX_Play(0, 1);
		}
		else
		{
			gNativeFont = (gNativeFont + 1) % NATIVE_FONT_COUNT;
			if (button & BTN_RIGHT)
			{
				OtherFX_Play(0, 1);
			}
		}
		save_config();
		return;
	}

	if (choose == NATIVE_MENU_STRING_COLLISION_PHYSICS && CTR_FRAMES_PER_SECOND > 60)
		return; // forced on above 60 FPS

	if (choose == NATIVE_MENU_STRING_AI_PHYSICS || choose == NATIVE_MENU_STRING_COLLISION_PHYSICS || choose == NATIVE_MENU_STRING_STEERING_PHYSICS)
	{
		enum NativePhysicsDomain domain = NATIVE_PHYSICS_AI;
		int enabled = gNativeSmoothedAIEnabled;
		if (choose == NATIVE_MENU_STRING_COLLISION_PHYSICS) { domain = NATIVE_PHYSICS_COLLISION; enabled = gNativeSmoothedCollisionEnabled; }
		if (choose == NATIVE_MENU_STRING_STEERING_PHYSICS) { domain = NATIVE_PHYSICS_STEERING; enabled = gNativeSmoothedSteeringEnabled; }
		NativePhysics_SetDomain(domain, !enabled);
		save_config();
		return;
	}

	if (choose == NATIVE_MENU_STRING_ANTI_ALIASING)
	{
		if (button & BTN_LEFT)
		{
			gNativeAntiAliasingMode = (gNativeAntiAliasingMode + NATIVE_AA_MODE_COUNT - 1) % NATIVE_AA_MODE_COUNT;
			OtherFX_Play(0, 1);
		}
		else
		{
			gNativeAntiAliasingMode = (gNativeAntiAliasingMode + 1) % NATIVE_AA_MODE_COUNT;
			if (button & BTN_RIGHT)
			{
				OtherFX_Play(0, 1);
			}
		}
		save_config();
		return;
	}

	if (choose == NATIVE_MENU_STRING_DITHERING)
	{
		gNativeDitheringEnabled ^= 1;
		save_config();
		return;
	}

	if (choose == NATIVE_MENU_STRING_DEPTH_BUFFER)
	{
		// This stores the Classic preference; Native 3D keeps depth testing enabled.
		// The row is locked (see MM_NativeOptionsRowLocked) while Native 3D is active.
		gNativeDepthBufferEnabled ^= 1;
		save_config();
		return;
	}

	if (choose == NATIVE_MENU_STRING_RENDERER)
	{
#if NATIVE_DRAW3D_SUPPORTED
		gNativeRendererMode = (gNativeRendererMode + 1) % NATIVE_RENDERER_MODE_COUNT;
		MM_NativeOptionsApplyLocks(P32_GET(struct MenuRow *, menu->rows), MM_NativeOptionsInGame());
		if (button & (BTN_LEFT | BTN_RIGHT))
		{
			OtherFX_Play(0, 1);
		}
		save_config();
#endif
		return;
	}

	if (choose == NATIVE_MENU_STRING_ASPECT_RATIO)
	{
#if NATIVE_DRAW3D_SUPPORTED
		if ((button & (BTN_LEFT | BTN_RIGHT)) != 0)
		{
			if (button & BTN_LEFT)
			{
				gNativeAspectRatio = (gNativeAspectRatio + NATIVE_ASPECT_COUNT - 1) % NATIVE_ASPECT_COUNT;
			}
			else if (button & BTN_RIGHT)
			{
				gNativeAspectRatio = (gNativeAspectRatio + 1) % NATIVE_ASPECT_COUNT;
			}
			save_config();
		}
#endif
		return;
	}

	if (choose == NATIVE_MENU_STRING_FIELD_OF_VIEW)
	{
#if NATIVE_DRAW3D_SUPPORTED
		if ((button & (BTN_LEFT | BTN_RIGHT)) != 0)
		{
			if (button & BTN_LEFT)
			{
				gNativeFovDegrees = (gNativeFovDegrees < 45 || gNativeFovDegrees > 100) ? 100 :
				                    (gNativeFovDegrees < 50) ? 0 : gNativeFovDegrees - 5;
			}
			else if (button & BTN_RIGHT)
			{
				gNativeFovDegrees = (gNativeFovDegrees == 0 || gNativeFovDegrees < 45 || gNativeFovDegrees > 100) ? 45 :
				                    (gNativeFovDegrees > 95) ? 0 : gNativeFovDegrees + 5;
			}
			save_config();
		}
#endif
		return;
	}

	if (choose == NATIVE_MENU_STRING_PROJECTION)
	{
#if NATIVE_DRAW3D_SUPPORTED
		if ((button & (BTN_LEFT | BTN_RIGHT)) != 0)
		{
			if (button & BTN_LEFT)
			{
				gNativeProjectionMode = (gNativeProjectionMode + NATIVE_PROJECTION_MODE_COUNT - 1) % NATIVE_PROJECTION_MODE_COUNT;
			}
			else if (button & BTN_RIGHT)
			{
				gNativeProjectionMode = (gNativeProjectionMode + 1) % NATIVE_PROJECTION_MODE_COUNT;
			}
			save_config();
		}
#endif
		return;
	}

	if (choose == NATIVE_MENU_STRING_PROJECTION_STRENGTH)
	{
#if NATIVE_DRAW3D_SUPPORTED
		if ((button & (BTN_LEFT | BTN_RIGHT)) != 0)
		{
			if (button & BTN_LEFT)
			{
				gNativeProjectionStrength = (gNativeProjectionStrength < 10) ? 100 : gNativeProjectionStrength - 10;
			}
			else if (button & BTN_RIGHT)
			{
				gNativeProjectionStrength = (gNativeProjectionStrength > 90) ? 0 : gNativeProjectionStrength + 10;
			}
			save_config();
		}
#endif
		return;
	}

	if (choose == NATIVE_MENU_STRING_COLOR_DEPTH)
	{
		gNativeColorDepth = (gNativeColorDepth + 1) % NATIVE_COLOR_DEPTH_COUNT;
		if (button & (BTN_LEFT | BTN_RIGHT))
		{
			OtherFX_Play(0, 1);
		}
		save_config();
		return;
	}

	if (choose == NATIVE_MENU_STRING_PS1_RESOLUTION)
	{
		gNativePs1ResolutionEnabled ^= 1;
		if (button & (BTN_LEFT | BTN_RIGHT))
		{
			OtherFX_Play(0, 1);
		}
		save_config();
		return;
	}

	if (choose == NATIVE_MENU_STRING_TEXTURE_FILTER)
	{
		g_cfg_bilinearFiltering ^= 1;
		if (button & (BTN_LEFT | BTN_RIGHT))
		{
			OtherFX_Play(0, 1);
		}
		save_config();
		return;
	}

	if (choose == NATIVE_MENU_STRING_HD_PAUSE)
	{
		if (button & BTN_LEFT)
		{
			gNativeHdPauseMode = (gNativeHdPauseMode + 2) % 3;
			OtherFX_Play(0, 1);
		}
		else
		{
			gNativeHdPauseMode = (gNativeHdPauseMode + 1) % 3;
			if (button & BTN_RIGHT)
			{
				OtherFX_Play(0, 1);
			}
		}
		save_config();
		return;
	}

	if (choose == NATIVE_MENU_STRING_PGXP)
	{
		if (button & BTN_LEFT)
		{
			gNativePgxpMode = (gNativePgxpMode + NATIVE_PGXP_MODE_COUNT - 1) % NATIVE_PGXP_MODE_COUNT;
			OtherFX_Play(0, 1);
		}
		else
		{
			gNativePgxpMode = (gNativePgxpMode + 1) % NATIVE_PGXP_MODE_COUNT;
			if (button & BTN_RIGHT)
			{
				OtherFX_Play(0, 1);
			}
		}
		save_config();
		return;
	}

	if (choose == NATIVE_MENU_STRING_PHYSICS)
	{
		NativePhysics_SetEnabled(!gNativeSmoothedPhysicsEnabled);
		save_config();
		return;
	}

	if (choose == NATIVE_MENU_STRING_MAX_LOD)
	{
		gNativeMaxLodEnabled ^= 1;
		save_config();
		return;
	}

	if (choose == NATIVE_MENU_STRING_BORDERLESS)
	{
		Platform_SetBorderless(!gNativeBorderlessEnabled);
		save_config();
	}
#endif
}

static void MM_NativeCheatsMenuProc(struct RectMenu *menu)
{
	if (menu->funcState == RECTMENU_FUNC_STATE_UPDATE) return;
	if (menu->funcState != RECTMENU_FUNC_STATE_INPUT) return;

	struct RectMenu *parent = P32_GET(struct RectMenu *, menu->ptrPrevBox_InHierarchy);
	if (menu->rowSelected < 0)
	{
		if (parent != NULL) parent->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
		return;
	}

	u32 cheatBit = NativeCheat_GetMenuBit(menu->rowSelected);
	if (menu->rowSelected == 9)
	{
		NativeCheat_ToggleAllCharacters();
		return;
	}
	if (cheatBit == 0) return;

	gNativeCheatConfigMask ^= cheatBit;
	NativeCheat_ApplyConfigured();
	save_config();
}

static void MM_NativeCreditsMenuProc(struct RectMenu *menu)
{
	if (menu->funcState != RECTMENU_FUNC_STATE_INPUT) return;

	// text only: any confirm or back returns to the main menu
	struct RectMenu *parent = P32_GET(struct RectMenu *, menu->ptrPrevBox_InHierarchy);
	if (parent != NULL) parent->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
}

static void MM_NativeControlsMenuProc(struct RectMenu *menu)
{
	if ((menu == NULL) || (menu->funcState != RECTMENU_FUNC_STATE_DRAW))
	{
		return;
	}

	if (menu->rowSelected < 1 || menu->rowSelected > PLATFORM_INPUT_BIND_ACTION_COUNT)
	{
		menu->rowSelected = 1;
	}
	gNativeControlsSelectedAction = menu->rowSelected - 1;

	if (gNativeControlsCaptureActive)
	{
		int binding;
#ifdef __vita__
		const int device = PLATFORM_INPUT_BINDING_CONTROLLER;
#else
		const int device = gNativeControlsSelectedColumn == 0 ? PLATFORM_INPUT_BINDING_KBM : PLATFORM_INPUT_BINDING_CONTROLLER;
#endif
		RECTMENU_ClearInput();
		if (Platform_InputPollBindingCapture(device, &binding))
		{
			Platform_InputSetBinding(gNativeControlsSelectedAction, device, binding);
			save_config();
			gNativeControlsCaptureActive = 0;
			s_nativeControlsWaitForRelease = 1;
			s_nativeControlsWaitDevice = device;
			s_nativeControlsWaitBinding = binding;
			OtherFX_Play(1, 1);
		}
		return;
	}

	if (s_nativeControlsWaitForRelease)
	{
		RECTMENU_ClearInput();
		if (!Platform_InputBindingIsActive(s_nativeControlsWaitDevice, s_nativeControlsWaitBinding))
		{
			s_nativeControlsWaitForRelease = 0;
		}
		return;
	}

	u32 tap = sdata->buttonTapPerPlayer[0];
	if ((tap & BTN_UP) != 0)
	{
		menu->rowSelected--;
		if (menu->rowSelected < 1)
		{
			menu->rowSelected = PLATFORM_INPUT_BIND_ACTION_COUNT;
		}
		gNativeControlsSelectedAction = menu->rowSelected - 1;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
		return;
	}
	if ((tap & BTN_DOWN) != 0)
	{
		menu->rowSelected++;
		if (menu->rowSelected > PLATFORM_INPUT_BIND_ACTION_COUNT)
		{
			menu->rowSelected = 1;
		}
		gNativeControlsSelectedAction = menu->rowSelected - 1;
		OtherFX_Play(0, 1);
		RECTMENU_ClearInput();
		return;
	}

#ifndef __vita__
	if ((tap & BTN_LEFT) != 0)
	{
		if (gNativeControlsSelectedColumn != 0)
		{
			gNativeControlsSelectedColumn = 0;
			OtherFX_Play(0, 1);
		}
		RECTMENU_ClearInput();
		return;
	}
	if ((tap & BTN_RIGHT) != 0)
	{
		if (gNativeControlsSelectedColumn != 1)
		{
			gNativeControlsSelectedColumn = 1;
			OtherFX_Play(0, 1);
		}
		RECTMENU_ClearInput();
		return;
	}
#endif

	if ((tap & (BTN_CROSS_one | BTN_CIRCLE)) != 0)
	{
#ifdef __vita__
		const int device = PLATFORM_INPUT_BINDING_CONTROLLER;
#else
		const int device = gNativeControlsSelectedColumn == 0 ? PLATFORM_INPUT_BINDING_KBM : PLATFORM_INPUT_BINDING_CONTROLLER;
#endif
		gNativeControlsCaptureActive = 1;
		Platform_InputBeginBindingCapture(device);
		OtherFX_Play(1, 1);
		RECTMENU_ClearInput();
		return;
	}

	if ((tap & (BTN_TRIANGLE | BTN_SQUARE_one)) != 0)
	{
		struct RectMenu *parent = P32_GET(struct RectMenu *, menu->ptrPrevBox_InHierarchy);
		gNativeControlsCaptureActive = 0;
		s_nativeControlsWaitForRelease = 0;
		if (parent != NULL)
		{
			struct RectMenu *root = parent;
			while (P32_GET(struct RectMenu *, root->ptrPrevBox_InHierarchy) != NULL)
			{
				root = P32_GET(struct RectMenu *, root->ptrPrevBox_InHierarchy);
			}
			P32_SET(sdata->ptrDesiredMenu, root);
		}
		OtherFX_Play(2, 1);
		RECTMENU_ClearInput();
	}
}

static void MM_NativeBossFightPrepareMenu(void)
{
	s_nativeBossFightMenu.stringIndexTitle = NATIVE_MENU_STRING_BOSS_FIGHT;
	s_nativeBossFightMenu.posX_curr = 256;
	s_nativeBossFightMenu.posY_curr = 82;
	s_nativeBossFightMenu.state = RECTMENU_STATE_EXEC_CENTERED | USE_SMALL_FONT | BIG_TEXT_IN_TITLE;
	P32_SET(s_nativeBossFightMenu.rows, s_nativeBossFightRows);
	P32_SET(s_nativeBossFightMenu.funcPtr, MM_NativeBossFightMenuProc);
	s_nativeBossFightMenu.rowSelected = (s16)gNativeBossFightBossID;
	P32_SET(s_nativeBossFightMenu.ptrNextBox_InHierarchy, NULL);
	P32_SET(s_nativeBossFightMenu.ptrPrevBox_InHierarchy, NULL);
}

void MM_NativeBossFight_OpenBossSelect(void)
{
	MM_NativeBossFightPrepareMenu();
	P32_SET(sdata->ptrDesiredMenu, &s_nativeBossFightMenu);
}

void MM_NativeBossFight_JumpToBossSelect(void)
{
	MM_NativeBossFightPrepareMenu();
	P32_SET(sdata->ptrActiveMenu, &s_nativeBossFightMenu);
}

static void MM_NativeBossFightMenuProc(struct RectMenu *menu)
{
	if (menu->funcState != RECTMENU_FUNC_STATE_INPUT)
	{
		return;
	}

	if (menu->rowSelected < 0)
	{
		P32_SET(sdata->ptrDesiredMenu, &D230.menuCharacterSelect);
		MM_Characters_RestoreIDs();
		return;
	}

	NativeBossFight_SelectBoss(menu->rowSelected);
	P32_SET(sdata->ptrDesiredMenu, &D230.menuTrackSelect);
	MM_TrackSelect_Init();
}
#endif

// NOTE(aalhendi): ASM-verified against retail 230 0x800abaf0-0x800abcac.
u8 MM_TransitionInOut(struct TransitionMeta *meta, int framesPassed, int numFrames)
{
	u8 allTransitionsDone = 1;
	int transitionIndex = 0;

	// last member of array is null-terminated with 0xFFFF
	for (/**/; meta->headStart > -1; meta++, transitionIndex++)
	{
		// framesPassed and numFrames count rendered frames; headStart and the
		// swish frame are 30 FPS frame counts
		s16 start = (s16)FPS_DOUBLE(meta->headStart);
		s16 framesLeft = ((s16)framesPassed - start);

		if ((framesLeft == FPS_DOUBLE(MM_TRANSITION_SWISH_FRAME)) && (transitionIndex == 0))
		{
			// Play "swoosh" sound for menu transition
			OtherFX_Play(MM_TRANSITION_SWISH_SFX, 0);
		}

		if (framesLeft < 1)
		{
			allTransitionsDone = 0;
			meta->currX = 0;
			meta->currY = 0;
			continue;
		}

		// else if
		if (framesLeft < (s16)numFrames)
		{
			allTransitionsDone = 0;
			meta->currX = framesLeft * meta->distX / (s16)numFrames;
			meta->currY = framesLeft * meta->distY / (s16)numFrames;
			continue;
		}

		// else
		meta->currX = meta->distX;
		meta->currY = meta->distY;
	}
	return allTransitionsDone;
}

#if defined(CTR_NATIVE)
static void MM_NativeAdventureConfigureRows(void);
#endif

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800acff4-0x800ad448.
void MM_MenuProc_Main(struct RectMenu *mainMenu)
{
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);

#if defined(CTR_NATIVE)
	if (MM_BootCredits_Update(mainMenu))
	{
		return;
	}

	if ((mainMenu->state & DRAW_NEXT_MENU_IN_HIERARCHY) == 0)
	{
		NativeCheat_ApplyConfigured();
	}
#if defined(__vita__)
	if ((mainMenu->funcState == RECTMENU_FUNC_STATE_UPDATE) && MM_NativeAdhocPollWait(gGT))
	{
		return;
	}

	if (((mainMenu->state & DRAW_NEXT_MENU_IN_HIERARCHY) == 0) &&
	    ((s_nativeAdhocMenuStage != MM_NATIVE_ADHOC_STAGE_GAME_FLOW) || !NativeAdhoc_IsActive()))
	{
		if (NativeAdhoc_IsActive())
		{
			NativeAdhoc_Shutdown();
		}
		MM_NativeAdhocSetMainBreadcrumb(NATIVE_MENU_STRING_ADHOC);
		s_nativeAdhocMenuStage = MM_NATIVE_ADHOC_STAGE_MAIN;
	}
#endif

	if (CHECK_ADV_BIT(sdata->gameProgress.unlocks, GAME_UNLOCK_BIT_SCRAPBOOK))
	{
		P32_SET(mainMenu->rows, s_nativeMainMenuWithScrapbook);
	}
	else
	{
		P32_SET(mainMenu->rows, s_nativeMainMenuBasic);
	}
#else
	// if scrapbook is unlocked, change "rows" to extended array
	if (CHECK_ADV_BIT(sdata->gameProgress.unlocks, GAME_UNLOCK_BIT_SCRAPBOOK))
	{
		mainMenu->rows = &D230.rowsMainMenuWithScrapbook[0];
	}
#endif

	MM_ParseCheatCodes();
	MM_ToggleRows_Difficulty();
	MM_ToggleRows_PlayerCount();

	// If you are at the highest hierarchy level of main menu
	if (mainMenu->funcState == RECTMENU_FUNC_STATE_UPDATE)
	{
#if defined(CTR_NATIVE)
		MM_NativeTimeTrialRefreshOnlineRow();
#endif
		MM_Title_MenuUpdate();

		if (
		    // main menu, "title" exists, and timer >= 230
		    (D230.titleMenuState == TITLE_MENU_STATE_IN_MENU) && (P32_GET(struct Title *, D230.titleObj) != NULL) && (FPS_DOUBLE(TITLE_INTRO_TM_DRAW_MIN_FRAME) < D230.titleIntroFrame))
		{
			DecalFont_DrawLineOT(P32_GET(char *, P32_GET(P32(char *) *, sdata->lngStrings)[LNG_TM]), MM_TITLE_TM_X, MM_TITLE_TM_Y, FONT_SMALL, ORANGE,
			                     &P32_GET(uint32_t *, P32_GET(struct DB *, gGT->backBuffer)->otMem.uiOT)[MM_TITLE_TM_OT_INDEX]);

#if defined(CTR_NATIVE)
			b32 userIdShown = false;
			if ((D230.menuMainMenu.state & DRAW_NEXT_MENU_IN_HIERARCHY) == 0)
			{
				const char *userId = NativeUserId_GetDisplayString();
				if (userId != NULL)
				{
					char userIdText[32];
					snprintf(userIdText, sizeof(userIdText), "USER ID: %s", userId);
					DecalFont_DrawLineOT(userIdText, 8, 0xc8, FONT_SMALL, WHITE,
					                     &P32_GET(uint32_t *, P32_GET(struct DB *, gGT->backBuffer)->otMem.uiOT)[MM_TITLE_TM_OT_INDEX]);
					userIdShown = true;
				}
			}
			MM_BootCredits_DrawTitleExtras(gGT, userIdShown);
#endif
		}

		if ((D230.menuMainMenu.state & DRAW_NEXT_MENU_IN_HIERARCHY) == 0)
		{
#if defined(__vita__)
			if ((s_nativeAdhocMenuStage == MM_NATIVE_ADHOC_STAGE_GAME_FLOW) && NativeAdhoc_IsConnected())
			{
				gGT->numPlyrNextGame = 2;
			}
			else
#endif
			{
				gGT->numPlyrNextGame = 1;
			}

			// if no buttons pressed, check demo mode
			if (P32_GET(struct GamepadSystem *, sdata->gGamepads)->anyoneHeldCurr == 0)
			{
				gGT->demoCountdownTimer--;

				// If time runs out
				if (gGT->demoCountdownTimer < 1)
				{
					// Transition out of main menu
					D230.titleMenuState = TITLE_MENU_STATE_EXITING;

					// Go to a cutscene of some kind, either the Oxide intro
					// or a demo-mode race.
					D230.desiredMenuIndex = MM_EXIT_ROUTE_DEMO;
				}
			}

			// if button pressed, reset timer
			else
			{
				gGT->demoCountdownTimer = FPS_DOUBLE(TITLE_DEMO_IDLE_FRAMES);
			}
		}
	}

	MM_Title_Init();

	// if drawing ptrNextBox_InHierarchy
	if ((mainMenu->state & DRAW_NEXT_MENU_IN_HIERARCHY) != 0)
	{
		D230.titleIntroFrame = FPS_DOUBLE(TITLE_INTRO_SKIP_FRAME);
	}

	// if funcPtr is null
	if ((mainMenu->state & EXECUTE_FUNCPTR) == 0)
	{
		return;
	}

	struct Title *titleObj = P32_GET(struct Title *, D230.titleObj);

	// if "title" object exists
	if (titleObj != NULL)
	{
		// CameraPosOffset X
		titleObj->cameraPosOffset.x = 0;
	}

	// if you are at highest level of menu hierarchy
	if (mainMenu->funcState != RECTMENU_FUNC_STATE_INPUT)
	{
		// leave the function
		return;
	}

	// If you are here, then you must not be
	// at the highest level of menu hierarchy

	// if row is negative, do nothing
	if ((mainMenu->rowSelected) < 0)
	{
		return;
	}

	// clear flags from game mode
	gGT->gameMode1 &= ~(BATTLE_MODE | ADVENTURE_MODE | TIME_TRIAL | RELIC_RACE | ADVENTURE_ARENA | ARCADE_MODE | ADVENTURE_CUP);

	// clear more game mode flags
	gGT->gameMode2 &= ~(CUP_ANY_KIND);

	mainMenu->state |= ONLY_DRAW_TITLE;

	// Default to 3,
	// this intentionally disables the 1-lap cheat
	// in Time Trial and Adventure, DONT change it
	gGT->numLaps = MM_DEFAULT_LAP_COUNT;

	// get LNG index of row selected
	s16 choose = P32_GET(struct MenuRow *, mainMenu->rows)[mainMenu->rowSelected].stringIndex;

	gNativeGhostReplayMode = 0;
	gNativeRelicRaceMode = 0;
	gNativeRelicRaceResultTier = -1;
	NativeGhostInput_ClearSelection();
	NativeBossFight_Clear();

	// Adventure Mode
	if (choose == LNG_ADVENTURE)
	{
		// Turn on Adventure Mode, turn off item cheats
		gGT->gameMode1 |= ADVENTURE_MODE;
		gGT->gameMode2 &= ~(CHEAT_WUMPA | CHEAT_MASK | CHEAT_TURBO | CHEAT_ENGINE | CHEAT_BOMBS);

		// menu for new/load
#if defined(CTR_NATIVE)
		MM_NativeAdventureConfigureRows();
#endif
		P32_SET(mainMenu->ptrNextBox_InHierarchy, &D230.menuAdventure);
		mainMenu->state |= DRAW_NEXT_MENU_IN_HIERARCHY;
		return;
	}

	// Time Trial
	if (choose == LNG_TIME_TRIAL)
	{
#if defined(CTR_NATIVE)
		s_nativeTimeTrialMenu.rowSelected = 0;
		s_nativeTimeTrialMenu.state = CENTER_ON_X;
		P32_SET(s_nativeTimeTrialMenu.ptrNextBox_InHierarchy, NULL);
		P32_SET(s_nativeTimeTrialMenu.ptrPrevBox_InHierarchy, mainMenu);

		P32_SET(mainMenu->ptrNextBox_InHierarchy, &s_nativeTimeTrialMenu);
		mainMenu->state |= DRAW_NEXT_MENU_IN_HIERARCHY;
		return;
#else
		D230.titleMenuState = TITLE_MENU_STATE_EXITING;
		D230.desiredMenuIndex = MM_EXIT_ROUTE_CHARACTER_SELECT;
		gGT->numPlyrNextGame = 1;
		gGT->gameMode1 |= TIME_TRIAL;
		gGT->gameMode2 &= ~(CHEAT_WUMPA | CHEAT_MASK | CHEAT_TURBO | CHEAT_ENGINE | CHEAT_BOMBS);
		return;
#endif
	}

	// Arcade Mode
	if (choose == LNG_ARCADE)
	{
		// DONT change, should only work in Arcade, and VS
		// set game mode to Arcade Mode
		gGT->gameMode1 |= ARCADE_MODE;

		// set next menu
		P32_SET(mainMenu->ptrNextBox_InHierarchy, &D230.menuRaceType);
		mainMenu->state |= DRAW_NEXT_MENU_IN_HIERARCHY;
		return;
	}

	// Versus
	if (choose == LNG_VS)
	{
		// DONT change, should only work in Arcade, and VS
		// next menu is choosing single+cup
		P32_SET(mainMenu->ptrNextBox_InHierarchy, &D230.menuRaceType);
		mainMenu->state |= DRAW_NEXT_MENU_IN_HIERARCHY;
		return;
	}

	if (choose == NATIVE_MENU_STRING_BOSS_FIGHT)
	{
		gNativeBossFightMode = 1;
		NativeBossFight_SelectBoss(0);
		gGT->numPlyrNextGame = 1;
		gGT->gameMode1 |= ADVENTURE_BOSS;
		gGT->gameMode2 &= ~(CHEAT_WUMPA | CHEAT_MASK | CHEAT_TURBO | CHEAT_ENGINE | CHEAT_BOMBS);
		D230.titleMenuState = TITLE_MENU_STATE_EXITING;
		D230.desiredMenuIndex = MM_EXIT_ROUTE_CHARACTER_SELECT;
		return;
	}

	// Battle
	if (choose == LNG_BATTLE)
	{
		D230.characterSelectTransitionState = EXITING_MENU;

		// set game mode to Battle Mode
		gGT->gameMode1 |= BATTLE_MODE;

		// set next menu to 2P,3P,4P
		P32_SET(mainMenu->ptrNextBox_InHierarchy, &D230.menuPlayers2P3P4P);
		mainMenu->state |= DRAW_NEXT_MENU_IN_HIERARCHY;
		return;
	}

	// High Score
	if (choose == LNG_HIGH_SCORE)
	{
		// Set next stage to high score menu
		D230.desiredMenuIndex = MM_EXIT_ROUTE_HIGH_SCORE;

		// Leave main menu hierarchy
		D230.titleMenuState = TITLE_MENU_STATE_EXITING;

		return;
	}

#if defined(CTR_NATIVE)
#if defined(__vita__)
	if (choose == NATIVE_MENU_STRING_ADHOC)
	{
		s_nativeAdhocModeMenu.state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
		s_nativeAdhocModeMenu.ptrNextBox_InHierarchy = NULL;
		s_nativeAdhocRoleMenu.state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
		s_nativeAdhocRoleMenu.ptrNextBox_InHierarchy = NULL;
		s_nativeAdhocWaitMenu.state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
		s_nativeAdhocWaitMenu.ptrNextBox_InHierarchy = NULL;
		s_nativeAdhocSuppressInputFrames = 0;
		s_nativeAdhocGameMode = NATIVE_ADHOC_GAME_MODE_ARCADE;
		s_nativeAdhocMenuStage = MM_NATIVE_ADHOC_STAGE_MODE;
		s_nativeAdhocModeMenu.rowSelected = 0;
		s_nativeAdhocModeMenu.ptrPrevBox_InHierarchy = mainMenu;

		mainMenu->ptrNextBox_InHierarchy = &s_nativeAdhocModeMenu;
		mainMenu->state |= DRAW_NEXT_MENU_IN_HIERARCHY;
		return;
	}
#endif


	// Options
	if (choose == LNG_OPTIONS)
	{
		MM_NativeOptionsConfigureRows(0);
		s_nativeOptionsMenu.rowSelected = 0;
		// As a submenu, inherit the main menu's position. Pause and preset
		// entry points set absolute coordinates on this same menu instance.
		s_nativeOptionsMenu.posX_curr = 0;
		s_nativeOptionsMenu.posY_curr = 0;
		s_nativeOptionsMenu.state = CENTER_ON_X | USE_SMALL_FONT | BIG_TEXT_IN_TITLE;
		P32_SET(s_nativeOptionsMenu.ptrNextBox_InHierarchy, NULL);
		P32_SET(s_nativeOptionsMenu.ptrPrevBox_InHierarchy, mainMenu);

		P32_SET(mainMenu->ptrNextBox_InHierarchy, &s_nativeOptionsMenu);
		mainMenu->state |= DRAW_NEXT_MENU_IN_HIERARCHY;
		return;
	}

	if (choose == NATIVE_MENU_STRING_UNLOCKS)
	{
		s_nativeUnlockFirst = 0;
		s_nativeUnlocksMenu.rowSelected = 0;
		mainMenu->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
		P32_SET(sdata->ptrDesiredMenu, &s_nativeUnlocksMenu);
		RECTMENU_ClearInput();
		return;
	}

	if (choose == NATIVE_MENU_STRING_CREDITS)
	{
		s_nativeCreditsMenu.rowSelected = 0;
		P32_SET(s_nativeCreditsMenu.ptrNextBox_InHierarchy, NULL);
		P32_SET(s_nativeCreditsMenu.ptrPrevBox_InHierarchy, mainMenu);

		P32_SET(mainMenu->ptrNextBox_InHierarchy, &s_nativeCreditsMenu);
		mainMenu->state |= DRAW_NEXT_MENU_IN_HIERARCHY;
		return;
	}

	if (choose == NATIVE_MENU_STRING_EXIT_GAME)
	{
		sdata->mainGameState = 5;
		return;
	}
#endif

	// Scrapbook
	if (choose == LNG_SCRAPBOOK)
	{
		// Set next stage to Scrapbook
		D230.desiredMenuIndex = MM_EXIT_ROUTE_SCRAPBOOK;

		// Leave main menu hierarchy
		D230.titleMenuState = TITLE_MENU_STATE_EXITING;

		return;
	}
}

// NOTE(aalhendi): ASM-verified against NTSC-U 926 overlay 230 0x800ad448-0x800ad560.
void MM_ToggleRows_PlayerCount(void)
{
	for (s32 rowIndex = 0; rowIndex < MM_PLAYER_1P2P_SELECTABLE_ROWS; rowIndex++)
	{
		struct MenuRow *row = &D230.rowsPlayers1P2P[rowIndex];

		// unlock row
		row->stringIndex &= MENU_ROW_LNG_MASK;

		if (!MainFrame_HaveAllPads(rowIndex + 1))
		{
			// lock row
			row->stringIndex |= MENU_ROW_LOCKED;
		}
	}

	for (s32 rowIndex = 0; rowIndex < MM_PLAYER_2P3P4P_SELECTABLE_ROWS; rowIndex++)
	{
		struct MenuRow *row = &D230.rowsPlayers2P3P4P[rowIndex];

		// unlock row
		row->stringIndex &= MENU_ROW_LNG_MASK;

		if (!MainFrame_HaveAllPads(rowIndex + 2))
		{
			// lock row
			row->stringIndex |= MENU_ROW_LOCKED;
		}
	}
}

// NOTE(aalhendi): ASM-verified against NTSC-U 926 overlay 230 0x800ad560-0x800ad5e8.
void MM_MenuProc_1p2p(struct RectMenu *menu)
{
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);
	s16 row = menu->rowSelected;

	// if uninitialized
	if (row == -1)
	{
		P32_GET(struct RectMenu *, menu->ptrPrevBox_InHierarchy)->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);

		gGT->numPlyrNextGame = 1;

		D230.characterSelectTransitionState = ENTERING_MENU;
	}

	else
	{
		// if on row 0 or 1
		if ((row >= 0) && (row < MM_PLAYER_1P2P_SELECTABLE_ROWS))
		{
			// row 0 is 1P, row 1 is 2P
			gGT->numPlyrNextGame = menu->rowSelected + 1;

			// go to difficulty box
#if defined(CTR_NATIVE)
			MM_NativeExtraDifficultyPrepare();
			P32_SET(menu->ptrNextBox_InHierarchy, &s_nativeExtraDifficultyMenu);
#else
			menu->ptrNextBox_InHierarchy = &D230.menuDifficulty;
#endif

			menu->state |= ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY;
			return;
		}
	}
	return;
}

// NOTE(aalhendi): ASM-verified against NTSC-U 926 overlay 230 0x800ad5e8-0x800ad678.
void MM_MenuProc_2p3p4p(struct RectMenu *menu)
{
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);
	s16 row = menu->rowSelected;

	// if uninitialized
	if (row == -1)
	{
		P32_GET(struct RectMenu *, menu->ptrPrevBox_InHierarchy)->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);

		gGT->numPlyrNextGame = 1;

		D230.characterSelectTransitionState = ENTERING_MENU;
	}
	else
	{
		// row is 0, 1, 2
		if ((row >= 0) && (row < MM_PLAYER_2P3P4P_SELECTABLE_ROWS))
		{
			// row 0 is 2P, row 1 is 3P, row 2 is 4P
			gGT->numPlyrNextGame = menu->rowSelected + 2;

			D230.titleMenuState = TITLE_MENU_STATE_EXITING;
			D230.desiredMenuIndex = MM_EXIT_ROUTE_CHARACTER_SELECT;

			menu->state |= ONLY_DRAW_TITLE;
			return;
		}
	}
	return;
}

// NOTE(aalhendi): ASM-verified against NTSC-U 926 overlay 230 0x800ad678-0x800ad7a4.
void MM_ToggleRows_Difficulty(void)
{
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);

	// check 3 mods (easy, medium, hard)
	for (s32 difficultyIndex = 0; difficultyIndex < MM_DIFFICULTY_COUNT; difficultyIndex++)
	{
		s16 bitIndex = D230.cupDifficulty.firstUnlockBit[difficultyIndex];

		// if -1 (for EASY row), skip
		if (-1 == bitIndex)
		{
			continue;
		}

		// assume unlocked
		u32 isUnlocked = 1;

		// check 4 bits starting at bitIndex,
		// one for each track in cup
		for (s32 trackIndex = 0; trackIndex < MM_CUP_TRACK_COUNT; trackIndex++)
		{
			b32 shouldCheckNextTrack = (isUnlocked != 0);
			isUnlocked = 0;

			// if not determined locked
			if (shouldCheckNextTrack)
			{
				s32 unlockBit = (s32)bitIndex + trackIndex;

				// check what is unlocked
				isUnlocked = CHECK_ADV_BIT(sdata->gameProgress.unlocks, unlockBit);
			}
		}

		// get current value of lng index,
		// for easy, medium, hard
		u16 lngIndex = D230.cupDifficulty.stringIndex[difficultyIndex];

		if (
		    // if locked
		    (isUnlocked == 0) &&

		    // If you're in Arcade mode
		    ((gGT->gameMode1 & ARCADE_MODE) != 0) &&

		    // if you are in Arcade or VS cup
		    ((gGT->gameMode2 & CUP_ANY_KIND) != 0))
		{
			// use high bits for "LOCKED"
			lngIndex |= MENU_ROW_LOCKED;
		}

		// save new value
		D230.rowsDifficulty[difficultyIndex].stringIndex = lngIndex;
	}

#if defined(CTR_NATIVE)
	for (s32 difficultyIndex = 0; difficultyIndex < MM_DIFFICULTY_COUNT; difficultyIndex++)
	{
		s_nativeExtraDifficultyRows[difficultyIndex].stringIndex = D230.rowsDifficulty[difficultyIndex].stringIndex;
	}

	u16 hardLockFlag = D230.rowsDifficulty[MM_NATIVE_DIFFICULTY_HARD].stringIndex & MENU_ROW_LOCKED;
	s_nativeExtraDifficultyRows[MM_NATIVE_DIFFICULTY_SUPER_HARD].stringIndex = NATIVE_MENU_STRING_SUPER_HARD | hardLockFlag;
	s_nativeExtraDifficultyRows[MM_NATIVE_DIFFICULTY_ULTRA_HARD].stringIndex = NATIVE_MENU_STRING_ULTRA_HARD | hardLockFlag;
#endif
}

// NOTE(aalhendi): ASM-verified against NTSC-U 926 overlay 230 0x800ad7a4-0x800ad828.
void MM_MenuProc_Difficulty(struct RectMenu *menu)
{
	s16 row = menu->rowSelected;

	// if uninitialized
	if (row == -1)
	{
		P32_GET(struct RectMenu *, menu->ptrPrevBox_InHierarchy)->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
	}

	else
	{
		// if you are on a valid row
		if ((row >= 0) &&
#if defined(CTR_NATIVE)
		    (row < MM_NATIVE_DIFFICULTY_COUNT)
#else
		    (row < MM_DIFFICULTY_COUNT)
#endif
		)
		{
#if defined(CTR_NATIVE)
			if (row == MM_NATIVE_DIFFICULTY_SUPER_HARD)
			{
				P32_GET(struct GameTracker *, sdata->gGT)->arcadeDifficulty = 0x140;
			}
			else if (row == MM_NATIVE_DIFFICULTY_ULTRA_HARD)
			{
				P32_GET(struct GameTracker *, sdata->gGT)->arcadeDifficulty = 0x280;
			}
			else
#endif
			{
				// set difficulty to value, from array of fixed difficulty values
				P32_GET(struct GameTracker *, sdata->gGT)->arcadeDifficulty = D230.cupDifficulty.speed[row];
			}

			D230.titleMenuState = TITLE_MENU_STATE_EXITING;
			D230.desiredMenuIndex = MM_EXIT_ROUTE_CHARACTER_SELECT;

			menu->state |= ONLY_DRAW_TITLE;
			return;
		}
	}
	return;
}

// NOTE(aalhendi): ASM-verified against NTSC-U 926 overlay 230 0x800ad828-0x800ad8f0.
void MM_MenuProc_SingleCup(struct RectMenu *menu)
{
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);
	s16 row = menu->rowSelected;

	if (row == -1)
	{
		P32_GET(struct RectMenu *, menu->ptrPrevBox_InHierarchy)->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
		return;
	}

	if ((row >= 0) && (row < MM_RACE_TYPE_SELECTABLE_ROWS))
	{
		// disable Cup mode
		gGT->gameMode2 &= ~(CUP_ANY_KIND);

		// if you choose cup mode
		if (menu->rowSelected != 0)
		{
			// enable cup mode
			gGT->gameMode2 |= CUP_ANY_KIND;
		}

		menu->state |= ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY;

		// if mode is Arcade
		if ((gGT->gameMode1 & ARCADE_MODE) != 0)
		{
			// set next menu to 1P+2P select
			P32_SET(menu->ptrNextBox_InHierarchy, &D230.menuPlayers1P2P);
			D230.characterSelectTransitionState = IN_MENU;
			return;
		}

		// if mode is VS

		// set next menu to 2P+3P+4P (vs or battle)
		P32_SET(menu->ptrNextBox_InHierarchy, &D230.menuPlayers2P3P4P);
		D230.characterSelectTransitionState = EXITING_MENU;
	}
}

#if defined(CTR_NATIVE)
static struct MenuRow s_nativeAdventureRows[] =
{
	{NATIVE_MENU_STRING_QUICK_LOAD, 0, 1, 0, 0},
	{0x8d, 0, 2, 1, 1},
	{0x8e, 1, 2, 2, 2},
	{.stringIndex = RECTMENU_STRING_NONE},
};
static b32 s_nativeAdventureHasQuickLoad;

// Adds a CONTINUE row (default selection) above NEW / LOAD when an autosave exists.
static void MM_NativeAdventureConfigureRows(void)
{
	NativeAutoSave_Refresh();
	s_nativeAdventureHasQuickLoad = NativeAutoSave_Exists();
	P32_SET(D230.menuAdventure.rows, s_nativeAdventureHasQuickLoad ? &s_nativeAdventureRows[0] : &D230.rowsAdventure[0]);
	D230.menuAdventure.rowSelected = 0;
}
#endif

// NOTE(aalhendi): ASM-verified against NTSC-U 926 overlay 230 0x800ad8f0-0x800ad980.
void MM_MenuProc_NewLoad(struct RectMenu *menu)
{
	// row number
	s16 row = menu->rowSelected;

	if (row == -1)
	{
		P32_GET(struct RectMenu *, menu->ptrPrevBox_InHierarchy)->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);
		return;
	}

	s16 routeCount = MM_ADV_NEW_LOAD_ROUTE_COUNT;
#if defined(CTR_NATIVE)
	routeCount += s_nativeAdventureHasQuickLoad ? 1 : 0;
#endif
	if ((row < 0) || (row >= routeCount))
	{
		return;
	}

#if defined(CTR_NATIVE)
	// Reapply persistent cheat choices here: retail allowed cheat codes to be
	// entered on NEW/LOAD after Adventure selection cleared item-cheat bits.
	NativeCheat_ApplyConfigured();
#endif

#if defined(CTR_NATIVE)
	if (s_nativeAdventureHasQuickLoad)
	{
		row = (row == 0) ? MM_EXIT_ROUTE_ADV_QUICKLOAD : row - 1;
	}
#endif

	// if Load was chosen
	D230.desiredMenuIndex = row;

	// MM_Title transitioning out
	D230.titleMenuState = TITLE_MENU_STATE_EXITING;

	menu->state |= ONLY_DRAW_TITLE;
	return;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800ad980-0x800ad98c.
struct RectMenu *MM_AdvNewLoad_GetMenuPtr(void)
{
	// menu for new/load
	return &D230.menuAdventure;
}

// NOTE(aalhendi): ASM-verified against NTSC-U 926 overlay 230 0x800b42b0-0x800b4334.
void MM_ResetAllMenus(void)
{
	for (s32 menuIndex = 0; menuIndex < MM_MENU_RESET_COUNT; menuIndex++)
	{
		struct RectMenu *menu = P32_GET(struct RectMenu *, D230.arrayMenuPtrs[menuIndex]);

// NOTE(aalhendi): Retail resets one menu per array slot; native walks chained
// menus because overlay 230 data is not reloaded.
#ifdef CTR_NATIVE
		do
		{
			struct RectMenu *next = P32_GET(struct RectMenu *, menu->ptrNextBox_InHierarchy);
#endif

			// Close menu
			menu->state |= RECTMENU_CLOSE_TRANSIENT;
			menu->state &= ~(ONLY_DRAW_TITLE | DRAW_NEXT_MENU_IN_HIERARCHY);

			// Reset ptrNext and ptrPrev
			P32_SET(menu->ptrNextBox_InHierarchy, 0);
			P32_SET(menu->ptrPrevBox_InHierarchy, 0);

#ifdef CTR_NATIVE
			menu = next;
		} while (menu != 0);
#endif
	}

	// unused
	sdata->framesRemainingInMenu = MM_MENU_RESET_DONE_FRAMES;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b4334-0x800b4364.
void MM_JumpTo_Title_Returning(void)
{
	// return to main menu from another menu
	D230.titleMenuState = TITLE_MENU_STATE_RETURNING;

	// return to main menu
	P32_SET(sdata->ptrDesiredMenu, &D230.menuMainMenu);

	D230.titleMenuTransitionFrame = FPS_DOUBLE(D230.titleMenuTransitionDurationFrames);
}

// NOTE(aalhendi): ASM-verified against NTSC-U 926 overlay 230 0x800b4364-0x800b43f4.
void MM_JumpTo_Title_FirstTime(void)
{
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);

	MM_ResetAllMenus();

	MainStats_ClearBattleVS();

#if defined(CTR_NATIVE)
	if (s_nativeLanguageChosen == 0)
	{
		s_nativeLanguageBootMenu.state = RECTMENU_STATE_EXEC_CENTERED;
		s_nativeLanguageBootMenu.rowSelected = s_nativeLanguageRow;
		P32_SET(s_nativeLanguageBootMenu.ptrNextBox_InHierarchy, 0);
		P32_SET(s_nativeLanguageBootMenu.ptrPrevBox_InHierarchy, 0);
		s_nativeLanguageTimer = FPS_DOUBLE(MM_NATIVE_LANGUAGE_TIMEOUT_FRAMES);
		P32_SET(sdata->ptrActiveMenu, &s_nativeLanguageBootMenu);
	}
	else if (gNativePresetPending)
	{
		s_nativePresetMenu.state = RECTMENU_STATE_EXEC_CENTERED | RECTMENU_NATIVE_DRAW_CALLBACK;
		s_nativePresetMenu.rowSelected = 1;
		P32_SET(s_nativePresetMenu.ptrNextBox_InHierarchy, 0);
		P32_SET(s_nativePresetMenu.ptrPrevBox_InHierarchy, 0);
		P32_SET(sdata->ptrActiveMenu, &s_nativePresetMenu);
	}
	else
	{
		P32_SET(sdata->ptrActiveMenu, &D230.menuMainMenu);
	}
#elif BUILD == EurRetail
	// if you have not chose a language or skipped the language menu
	if (sdata->boolLangChosen == 0)
	{
		sdata->ptrActiveMenu = &D230.menuLngBoot;
		D230.langMenuTimer = FPS_DOUBLE(MM_LANGUAGE_MENU_TIMEOUT_FRAMES);
	}
	else
	{
		// if not set to normal main menu
		sdata->ptrActiveMenu = &D230.menuMainMenu;
	}
#else
	// open Main Menu for the first time
	sdata->ptrActiveMenu = &D230.menuMainMenu;
#endif

	D230.titleIntroFrame = 0;

	// first time in main menu
	// (play crash trophy anim)
	D230.titleMenuState = TITLE_MENU_STATE_INTRO;

	// reset countdown clock for battle or crystal challenge
	gGT->originalEventTime = TITLE_INITIAL_EVENT_TIME;

	D230.menuMainMenu.state &= ~(EXECUTE_FUNCPTR | ONLY_DRAW_TITLE);
	D230.menuMainMenu.state |= DISABLE_INPUT_ALLOW_FUNCPTRS;

	// distance to screen (perspective)
	gGT->pushBuffer[0].distanceToScreen_PREV = TITLE_DEFAULT_DISTANCE_TO_SCREEN;
	gGT->pushBuffer[0].distanceToScreen_CURR = TITLE_DEFAULT_DISTANCE_TO_SCREEN;
	gGT->gameMode1 &= ~(TIME_TRIAL | RELIC_RACE);
	gNativeRelicRaceMode = 0;
	gNativeRelicRaceResultTier = -1;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b43f4-0x800b4430.
void MM_JumpTo_BattleSetup(void)
{
	// Go to battle setup
	P32_SET(sdata->ptrActiveMenu, &D230.menuBattleWeapons);

	D230.menuBattleWeapons.state &= ~(ONLY_DRAW_TITLE);

	MM_Battle_Init();
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b4430-0x800b446c.
void MM_JumpTo_TrackSelect(void)
{
	// return to track selection
	P32_SET(sdata->ptrActiveMenu, &D230.menuTrackSelect);

	D230.menuTrackSelect.state &= ~(ONLY_DRAW_TITLE);

	MM_TrackSelect_Init();
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b446c-0x800b44a8.
void MM_JumpTo_Characters(void)
{
	// return to character selection
	P32_SET(sdata->ptrActiveMenu, &D230.menuCharacterSelect);

	D230.menuCharacterSelect.state &= ~(ONLY_DRAW_TITLE);

	MM_Characters_RestoreIDs();
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 overlay 230 0x800b44a8-0x800b44e4.
void MM_JumpTo_Scrapbook(void)
{
	// go to scrapbook
	P32_SET(sdata->ptrActiveMenu, &D230.menuScrapbook);

	D230.menuScrapbook.state &= ~(ONLY_DRAW_TITLE);

	MM_Scrapbook_Init();
}
