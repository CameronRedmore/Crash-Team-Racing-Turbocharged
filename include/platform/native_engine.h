#ifndef PLATFORM_NATIVE_ENGINE_H
#define PLATFORM_NATIVE_ENGINE_H

enum NativeEngineProfile
{
	NATIVE_ENGINE_DEFAULT = -1,
	NATIVE_ENGINE_BALANCED = 0,
	NATIVE_ENGINE_ACCEL = 1,
	NATIVE_ENGINE_SPEED = 2,
	NATIVE_ENGINE_TURN = 3,
	NATIVE_ENGINE_PENTA = 4,
	NATIVE_ENGINE_COUNT = 5,
};

extern int gNativeEngineSelectionEnabled;
extern int gNativeAdditionalUnlocksEnabled;

struct AdvProgress;
void NativeEngine_SaveAdventureProfile(struct AdvProgress *adv);
void NativeEngine_LoadAdventureProfile(const struct AdvProgress *adv);

int NativeEngine_GetDefaultProfile(int characterID);
int NativeEngine_GetSelectedProfile(int playerID);
int NativeEngine_GetEffectiveProfile(int driverID);
int NativeEngine_IsProfileUnlocked(int profile);
void NativeEngine_SetSelectedProfile(int playerID, int profile);
const char *NativeEngine_GetProfileName(int profile);

void NativeEngine_SetReplayOverride(int driverID, int profile);
void NativeEngine_ClearReplayOverride(int driverID);
void NativeEngine_ClearReplayOverrides(void);

#endif
