#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <common.h>

struct Data data;
struct sData sdata_static;
int cfg_language = 2;
int gNative60FpsEnabled;
int gNativeForce30Fps;
int gNativeGhostReplayFpsOverride = -1;
static struct GameTracker tracker;
static struct DB db;
static char *strings[LNG_SCRAPBOOK + 1];
static char rewardNames[LNG_SCRAPBOOK + 1][24];
static int chimes, lines, boxes;
static char *drawnName;

int OtherFX_Play(u32 soundID, int flags)
{
	assert(soundID == MM_CHEAT_SUCCESS_SFX && flags == 1);
	chimes++;
	return 0;
}
int DecalFont_GetLineWidth(char *str, s16 font)
{
	return (int)strlen(str) * (font == FONT_BIG ? 12 : 6);
}
void DecalFont_DrawLine(char *str, int x, int y, s16 font, int flags)
{
	(void)font;
	assert(str != NULL && x == 256 && (flags & JUSTIFY_CENTER));
	if (y == 28)
		drawnName = str;
	lines++;
}
void CTR_Box_DrawSolidBox(RECT *r, Color color, uint32_t *ot)
{
	(void)color;
	(void)ot;
	assert(r->x >= 16 && r->x + r->w <= 496 && r->y + r->h <= 240);
	boxes++;
}
void CTR_Box_DrawWireBox(RECT *r, const Color *color, void *ot, struct PrimMem *mem)
{
	(void)r;
	(void)color;
	(void)ot;
	(void)mem;
}

#include "../game/NativeUnlock.c"

static void reset(void)
{
	memset(&sdata_static, 0, sizeof(sdata_static));
	memset(&tracker, 0, sizeof(tracker));
	sdata->gGT = &tracker;
	sdata->lngStrings = strings;
	tracker.backBuffer = &db;
	tracker.gameMode1 = MAIN_MENU;
	sdata->Loading.stage = LOAD_IDLE;
	s_nativeUnlockHead = s_nativeUnlockCount = 0;
	s_nativeUnlockSeconds = 0;
	s_nativeUnlockStarted = false;
	gNative60FpsEnabled = 0;
	chimes = lines = boxes = 0;
}

static void drawFrames(int frames)
{
	for (int i = 0; i < frames; i++)
		NativeUnlock_Draw();
}

static void test_grants_and_save_import(void)
{
	reset();
	// A save import is deliberately separate from a live reward grant.
	sdata->gameProgress.unlockFlags = UNLOCK_TROPY;
	NativeUnlock_GrantMask(UNLOCK_TROPY);
	assert(s_nativeUnlockCount == 0);
	NativeUnlock_GrantMask(UNLOCK_ROO | UNLOCK_PENTA);
	NativeUnlock_GrantBit(GAME_UNLOCK_BIT_OXIDE);
	assert(s_nativeUnlockCount == 3);
	assert(CHECK_ADV_BIT(sdata->gameProgress.unlocks, GAME_UNLOCK_BIT_OXIDE));
	NativeUnlock_GrantMask(UNLOCK_ROO | UNLOCK_PENTA);
	NativeUnlock_GrantBit(GAME_UNLOCK_BIT_OXIDE);
	assert(s_nativeUnlockCount == 3);
	NativeUnlock_GrantBit(-1);
	NativeUnlock_GrantBit(64);
	assert(s_nativeUnlockCount == 3);
	drawFrames(150);
	assert(s_nativeUnlockCount == 2 && chimes == 1);
	assert(drawnName == strings[LNG_PENTA_PENGUIN]);
	drawFrames(150);
	assert(s_nativeUnlockCount == 1 && chimes == 2);
	assert(drawnName == strings[LNG_RIPPER_ROO]);
	drawFrames(150);
	assert(s_nativeUnlockCount == 0 && chimes == 3);
	assert(drawnName == strings[LNG_N_OXIDE_FULL]);
	NativeUnlock_Draw();
	assert(chimes == 3 && boxes == 450 && lines == 1350);
}

static void test_queue_capacity_and_reward_types(void)
{
	reset();
	NativeUnlock_GrantMask(~0u);
	NativeUnlock_GrantBit(GAME_UNLOCK_BIT_SCRAPBOOK);
	NativeUnlock_GrantBit(GAME_UNLOCK_BIT_OXIDE);
	NativeUnlock_NotifySlideColiseum();
	NativeUnlock_NotifySlideColiseum();
	assert(s_nativeUnlockCount == NATIVE_UNLOCK_REWARD_COUNT);
	drawFrames(150 * NATIVE_UNLOCK_REWARD_COUNT);
	assert(s_nativeUnlockCount == 0 && chimes == NATIVE_UNLOCK_REWARD_COUNT);
	assert(drawnName == strings[LNG_SLIDE_COLISEUM]);
}

static void test_deferred_display_and_frame_rates(void)
{
	const int rates[] = {30, 60, 90, 120, 144, 240};
	for (int rate = 0; rate < 6; rate++)
	{
		reset();
		gNative60FpsEnabled = rate;
		NativeUnlock_GrantMask(UNLOCK_PENTA);
		sdata->Loading.stage = LOAD_IDLE + 1;
		drawFrames(1000);
		sdata->Loading.stage = LOAD_IDLE;
		tracker.gameMode1 = GAME_CUTSCENE;
		drawFrames(1000);
		tracker.gameMode1 = TIME_TRIAL;
		drawFrames(1000);
		assert(chimes == 0 && boxes == 0 && s_nativeUnlockCount == 1);
		tracker.gameMode1 = END_OF_RACE;
		drawFrames(rates[rate] * 2);
		sdata->AkuAkuHintState = 1;
		drawFrames(1000);
		sdata->AkuAkuHintState = 0;
		assert(s_nativeUnlockCount == 1 && chimes == 1);
		drawFrames(rates[rate] * 3 - 1);
		assert(s_nativeUnlockCount == 1);
		NativeUnlock_Draw();
		assert(s_nativeUnlockCount == 0 && chimes == 1);
	}
	reset();
	NativeUnlock_GrantMask(UNLOCK_PENTA);
	drawFrames(60); // Two seconds at 30 fps, then three at 144 fps.
	gNative60FpsEnabled = 4;
	drawFrames(432);
	assert(s_nativeUnlockCount == 0);
}

int main(void)
{
	for (unsigned int i = 0; i < sizeof(strings) / sizeof(strings[0]); i++)
	{
		snprintf(rewardNames[i], sizeof(rewardNames[i]), "REWARD %u", i);
		strings[i] = rewardNames[i];
	}
	test_grants_and_save_import();
	test_queue_capacity_and_reward_types();
	test_deferred_display_and_frame_rates();
	puts("native unlock notifications passed");
	return 0;
}
