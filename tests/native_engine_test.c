#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <common.h>
#include <stdio.h>

struct Data data;
struct sData sdata_static;
static struct GameTracker s_tracker;

#include "../game/NativeEngine.c"

static void test_default_and_selected_profiles(void)
{
	memset(&s_tracker, 0, sizeof(s_tracker));
	P32_SET(sdata->gGT, &s_tracker);
	s_tracker.numPlyrCurrGame = 1;
	data.characterIDs[0] = CRASH_BANDICOOT;
	data.characterIDs[1] = TINY_TIGER;
	data.characterIDs[2] = COCO_BANDICOOT;
	data.characterIDs[3] = POLAR;
	data.MetaDataCharacters[CRASH_BANDICOOT].engineID = NATIVE_ENGINE_BALANCED;
	data.MetaDataCharacters[TINY_TIGER].engineID = NATIVE_ENGINE_SPEED;
	data.MetaDataCharacters[COCO_BANDICOOT].engineID = NATIVE_ENGINE_ACCEL;
	data.MetaDataCharacters[POLAR].engineID = NATIVE_ENGINE_TURN;
	data.MetaDataCharacters[PENTA_PENGUIN].engineID = NATIVE_ENGINE_TURN;

	assert(gNativeEngineSelectionEnabled == 0);
	assert(NativeEngine_GetDefaultProfile(CRASH_BANDICOOT) == NATIVE_ENGINE_BALANCED);
	assert(NativeEngine_GetDefaultProfile(TINY_TIGER) == NATIVE_ENGINE_SPEED);
	assert(NativeEngine_GetDefaultProfile(PENTA_PENGUIN) == NATIVE_ENGINE_PENTA);
	assert(NativeEngine_GetSelectedProfile(0) == NATIVE_ENGINE_DEFAULT);
	assert(NativeEngine_GetProfileName(NATIVE_ENGINE_ACCEL)[0] == 'A');
	assert(!NativeEngine_IsProfileUnlocked(NATIVE_ENGINE_COUNT));

	NativeEngine_SetSelectedProfile(0, NATIVE_ENGINE_TURN);
	assert(NativeEngine_GetEffectiveProfile(0) == NATIVE_ENGINE_BALANCED);
	gNativeEngineSelectionEnabled = 1;
	assert(NativeEngine_GetEffectiveProfile(0) == NATIVE_ENGINE_TURN);
	for (int profile = NATIVE_ENGINE_BALANCED; profile <= NATIVE_ENGINE_TURN; profile++)
	{
		NativeEngine_SetSelectedProfile(0, profile);
		assert(NativeEngine_GetEffectiveProfile(0) == profile);
		gNativeEngineSelectionEnabled = 0;
		assert(NativeEngine_GetEffectiveProfile(0) == NATIVE_ENGINE_BALANCED);
		gNativeEngineSelectionEnabled = 1;
		assert(NativeEngine_GetEffectiveProfile(0) == profile);
	}
	NativeEngine_SetSelectedProfile(0, NATIVE_ENGINE_DEFAULT);
	assert(NativeEngine_GetEffectiveProfile(0) == NATIVE_ENGINE_BALANCED);
	NativeEngine_SetSelectedProfile(0, NATIVE_ENGINE_TURN);
	assert(NativeEngine_GetEffectiveProfile(1) == NATIVE_ENGINE_SPEED); // AI retains character default.

	s_tracker.numPlyrCurrGame = 2;
	NativeEngine_SetSelectedProfile(1, NATIVE_ENGINE_ACCEL);
	assert(NativeEngine_GetEffectiveProfile(1) == NATIVE_ENGINE_ACCEL);
	assert(NativeEngine_GetEffectiveProfile(2) == NATIVE_ENGINE_ACCEL); // nonhuman driver keeps its default.
	s_tracker.numPlyrCurrGame = 4;
	NativeEngine_SetSelectedProfile(3, NATIVE_ENGINE_SPEED);
	assert(NativeEngine_GetEffectiveProfile(3) == NATIVE_ENGINE_SPEED);
	assert(NativeEngine_GetSelectedProfile(-1) == NATIVE_ENGINE_DEFAULT);
	NativeEngine_SetSelectedProfile(-1, NATIVE_ENGINE_TURN);
	NativeEngine_SetSelectedProfile(3, NATIVE_ENGINE_COUNT);
	assert(NativeEngine_GetSelectedProfile(3) == NATIVE_ENGINE_SPEED);
	P32_SET(sdata->gGT, NULL);
	assert(NativeEngine_GetEffectiveProfile(0) == NATIVE_ENGINE_BALANCED);
	assert(NativeEngine_GetEffectiveProfile(8) == NATIVE_ENGINE_BALANCED);
	P32_SET(sdata->gGT, &s_tracker);
	s_tracker.numPlyrCurrGame = 1;

	NativeEngine_SetSelectedProfile(0, NATIVE_ENGINE_PENTA);
	assert(NativeEngine_GetSelectedProfile(0) == NATIVE_ENGINE_TURN); // locked selections are rejected.
	sdata->gameProgress.unlockFlags |= UNLOCK_PENTA;
	NativeEngine_SetSelectedProfile(0, NATIVE_ENGINE_PENTA);
	assert(NativeEngine_GetEffectiveProfile(0) == NATIVE_ENGINE_PENTA);
	sdata->gameProgress.unlockFlags &= ~UNLOCK_PENTA;
	assert(NativeEngine_GetEffectiveProfile(0) == NATIVE_ENGINE_BALANCED); // unlock is checked live.
	gNativeEngineSelectionEnabled = 0;
	assert(NativeEngine_GetEffectiveProfile(0) == NATIVE_ENGINE_BALANCED);
	gNativeEngineSelectionEnabled = 1;
	assert(NativeEngine_GetEffectiveProfile(0) == NATIVE_ENGINE_BALANCED);
	sdata->gameProgress.unlockFlags |= UNLOCK_PENTA;
	assert(NativeEngine_GetEffectiveProfile(0) == NATIVE_ENGINE_PENTA); // retained selection becomes available again.
	sdata->gameProgress.unlockFlags &= ~UNLOCK_PENTA;
}

static void test_replay_overrides(void)
{
	NativeEngine_ClearReplayOverrides();
	gNativeEngineSelectionEnabled = 0;
	sdata->gameProgress.unlockFlags &= ~UNLOCK_PENTA;
	NativeEngine_SetReplayOverride(0, NATIVE_ENGINE_PENTA);
	assert(NativeEngine_GetEffectiveProfile(0) == NATIVE_ENGINE_PENTA);
	NativeEngine_ClearReplayOverride(0);
	assert(NativeEngine_GetEffectiveProfile(0) == NATIVE_ENGINE_BALANCED);
	NativeEngine_SetReplayOverride(8, NATIVE_ENGINE_SPEED); // out-of-range writes are ignored.
	NativeEngine_SetReplayOverride(0, NATIVE_ENGINE_COUNT);
	assert(NativeEngine_GetEffectiveProfile(0) == NATIVE_ENGINE_BALANCED);
}

int main(void)
{
	test_default_and_selected_profiles();
	test_replay_overrides();
	puts("native engine tests passed");
	return 0;
}
