#include <common.h>
#include <platform/native_engine.h>
#include <platform/native_engine_metadata.h>

enum
{
	NATIVE_ENGINE_PLAYER_COUNT = 4,
	NATIVE_ENGINE_DRIVER_COUNT = 8,
};

int gNativeEngineSelectionEnabled = 0;
int gNativeAdditionalUnlocksEnabled = 1;

static int s_selectedProfiles[NATIVE_ENGINE_PLAYER_COUNT] =
{
	NATIVE_ENGINE_DEFAULT,
	NATIVE_ENGINE_DEFAULT,
	NATIVE_ENGINE_DEFAULT,
	NATIVE_ENGINE_DEFAULT,
};

static int s_replayOverrides[NATIVE_ENGINE_DRIVER_COUNT] =
{
	NATIVE_ENGINE_DEFAULT,
	NATIVE_ENGINE_DEFAULT,
	NATIVE_ENGINE_DEFAULT,
	NATIVE_ENGINE_DEFAULT,
	NATIVE_ENGINE_DEFAULT,
	NATIVE_ENGINE_DEFAULT,
	NATIVE_ENGINE_DEFAULT,
	NATIVE_ENGINE_DEFAULT,
};

static int NativeEngine_IsProfileValid(int profile)
{
	return (profile >= NATIVE_ENGINE_BALANCED) && (profile < NATIVE_ENGINE_COUNT);
}

int NativeEngine_GetDefaultProfile(int characterID)
{
	if ((u32)characterID >= len(data.MetaDataCharacters))
	{
		return NATIVE_ENGINE_BALANCED;
	}
	if (characterID == PENTA_PENGUIN)
	{
		return NATIVE_ENGINE_PENTA;
	}

	int profile = data.MetaDataCharacters[characterID].engineID;
	return NativeEngine_IsProfileValid(profile) ? profile : NATIVE_ENGINE_BALANCED;
}

int NativeEngine_IsProfileUnlocked(int profile)
{
	if (profile == NATIVE_ENGINE_DEFAULT)
	{
		return 1;
	}

	if (!NativeEngine_IsProfileValid(profile))
	{
		return 0;
	}

	if (profile == NATIVE_ENGINE_PENTA)
	{
		return (sdata != NULL) && ((sdata->gameProgress.unlockFlags & UNLOCK_PENTA) != 0);
	}

	return 1;
}

int NativeEngine_GetSelectedProfile(int playerID)
{
	if ((u32)playerID >= NATIVE_ENGINE_PLAYER_COUNT)
	{
		return NATIVE_ENGINE_DEFAULT;
	}

	return s_selectedProfiles[playerID];
}

// Tagged metadata in the unused reward word preserves the retail save layout.
// Keep the choice even while the option is off; DEFAULT remains character-based.
void NativeEngine_SaveAdventureProfile(struct AdvProgress *adv)
{
	adv->reservedRewardFlags = NativeEngineMetadata_EncodeWord(s_selectedProfiles[0]);
}

void NativeEngine_LoadAdventureProfile(const struct AdvProgress *adv)
{
	int profile = NativeEngineMetadata_DecodeWord(adv->reservedRewardFlags);
	// Reset first so legacy, corrupt or locked choices cannot inherit another save.
	NativeEngine_SetSelectedProfile(0, NATIVE_ENGINE_DEFAULT);
	NativeEngine_SetSelectedProfile(0, profile);
	NativeEngine_ClearReplayOverrides();
}

void NativeEngine_SetSelectedProfile(int playerID, int profile)
{
	if (((u32)playerID >= NATIVE_ENGINE_PLAYER_COUNT) || !NativeEngine_IsProfileUnlocked(profile))
	{
		return;
	}

	s_selectedProfiles[playerID] = profile;
}

int NativeEngine_GetEffectiveProfile(int driverID)
{
	if ((u32)driverID >= NATIVE_ENGINE_DRIVER_COUNT)
	{
		return NATIVE_ENGINE_BALANCED;
	}

	int characterID = data.characterIDs[driverID];
	int defaultProfile = NativeEngine_GetDefaultProfile(characterID);
	int replayProfile = s_replayOverrides[driverID];

	// Replay data is authoritative and intentionally bypasses user settings and unlocks.
	if (NativeEngine_IsProfileValid(replayProfile))
	{
		return replayProfile;
	}

	struct GameTracker *gGT = (sdata != NULL) ? sdata->gGT : NULL;
	if ((gNativeEngineSelectionEnabled == 0) || (gGT == NULL) ||
	    (driverID >= gGT->numPlyrCurrGame) || (driverID >= NATIVE_ENGINE_PLAYER_COUNT))
	{
		return defaultProfile;
	}

	int selectedProfile = s_selectedProfiles[driverID];
	if ((selectedProfile == NATIVE_ENGINE_DEFAULT) || !NativeEngine_IsProfileUnlocked(selectedProfile))
	{
		return defaultProfile;
	}

	return selectedProfile;
}

const char *NativeEngine_GetProfileName(int profile)
{
	switch (profile)
	{
	case NATIVE_ENGINE_DEFAULT:
		return "Default";
	case NATIVE_ENGINE_BALANCED:
		return "Balanced";
	case NATIVE_ENGINE_ACCEL:
		return "Acceleration";
	case NATIVE_ENGINE_SPEED:
		return "Speed";
	case NATIVE_ENGINE_TURN:
		return "Turning";
	case NATIVE_ENGINE_PENTA:
		return "Penta";
	default:
		return "Unknown";
	}
}

void NativeEngine_SetReplayOverride(int driverID, int profile)
{
	if (((u32)driverID >= NATIVE_ENGINE_DRIVER_COUNT) ||
	    ((profile != NATIVE_ENGINE_DEFAULT) && !NativeEngine_IsProfileValid(profile)))
	{
		return;
	}

	s_replayOverrides[driverID] = profile;
}

void NativeEngine_ClearReplayOverride(int driverID)
{
	if ((u32)driverID < NATIVE_ENGINE_DRIVER_COUNT)
	{
		s_replayOverrides[driverID] = NATIVE_ENGINE_DEFAULT;
	}
}

void NativeEngine_ClearReplayOverrides(void)
{
	for (int driverID = 0; driverID < NATIVE_ENGINE_DRIVER_COUNT; driverID++)
	{
		s_replayOverrides[driverID] = NATIVE_ENGINE_DEFAULT;
	}
}
