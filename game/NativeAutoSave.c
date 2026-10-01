#include <common.h>
#include <platform/native_memcard.h>
#include <platform/native_custom_racer.h>

// Native-only adventure autosave. This is a separate file next to the normal
// SLOTS file and never goes through the RefreshCard state machine.
#define NATIVE_AUTOSAVE_NAME "BASCUS-94426-AUTO"

enum
{
	NATIVE_AUTOSAVE_MAGIC = 0x31545541, // "AUT1"
	NATIVE_AUTOSAVE_VERSION = 1,
};

struct NativeAutoSaveFile
{
	u32 magic;
	u32 version;
	u32 size;
	u32 checksum;
	struct AdvProgress adv;
	struct GameProgress gameProgress;
	struct GameOptions gameOptions;
};

static struct NativeAutoSaveFile s_nativeAutoSave;
static b32 s_nativeAutoSaveValid;

static u32 NativeAutoSave_Checksum(const struct NativeAutoSaveFile *save)
{
	const u8 *bytes = (const u8 *)&save->adv;
	u32 hash = 2166136261u;

	for (u32 i = 0; i < sizeof(*save) - OFFSETOF(struct NativeAutoSaveFile, adv); i++)
	{
		hash = (hash ^ bytes[i]) * 16777619u;
	}

	return hash;
}

// Re-reads the file and caches it. Called when the Adventure Load screen opens
// and after every write; Exists/Read only touch the cache.
void NativeAutoSave_Refresh(void)
{
	struct NativeAutoSaveFile save;

	s_nativeAutoSaveValid = false;
	memset(&save, 0, sizeof(save));

	if ((NativeMemcard_ReadSaveData(NATIVE_AUTOSAVE_NAME, (u8 *)&save, sizeof(save), 0) == NATIVE_MEMCARD_OK) && (save.magic == NATIVE_AUTOSAVE_MAGIC) &&
	    (save.version == NATIVE_AUTOSAVE_VERSION) && (save.size == sizeof(save)) && (save.checksum == NativeAutoSave_Checksum(&save)) &&
	    (save.adv.characterID >= 0))
	{
		s_nativeAutoSave = save;
		s_nativeAutoSaveValid = true;
	}
}

b32 NativeAutoSave_Exists(void)
{
	return s_nativeAutoSaveValid;
}

b32 NativeAutoSave_Read(struct AdvProgress *adv)
{
	if (!s_nativeAutoSaveValid)
	{
		return false;
	}

	if (adv != NULL)
	{
		*adv = s_nativeAutoSave.adv;
	}
	return true;
}

// Loads the autosave into the live adventure/game progress, matching
// SelectProfile_LoadAdvProfile for a normal slot.
b32 NativeAutoSave_Apply(void)
{
	struct GameTracker *gGT = sdata->gGT;

	if (!s_nativeAutoSaveValid)
	{
		return false;
	}

	GAMEPROG_SyncGameAndCard(&s_nativeAutoSave.gameProgress, &sdata->gameProgress);
	sdata->advProgress = s_nativeAutoSave.adv;
	NativeCustomRacer_ClearDriverSelections();
	data.characterIDs[0] = sdata->advProgress.characterID;
	memmove(gGT->prevNameEntered, sdata->advProgress.name, sizeof(gGT->prevNameEntered));
	return true;
}

// Loads the autosave and queues the saved hub directly, skipping the
// profile screen (mirrors SelectProfile_FinalizeAdventure for ADV_LOAD).
b32 NativeAutoSave_QuickLoad(void)
{
	struct GameTracker *gGT = sdata->gGT;

	if (!NativeAutoSave_Apply())
	{
		return false;
	}

	// Not a card slot, so a later manual save asks for overwrite confirmation.
	sdata->advProfileIndex = 0xffff;
	gGT->currLEV = (sdata->advProgress.HubLevYouSavedOn != 0) ? sdata->advProgress.HubLevYouSavedOn : N_SANITY_BEACH;
	memmove(gGT->prevNameEntered, sdata->advProgress.name, sizeof(gGT->prevNameEntered));
	memmove(gGT->currNameEntered, sdata->advProgress.name, sizeof(gGT->currNameEntered));
	sdata->ptrDesiredMenu = QueueLoadTrack_GetMenuPtr();
	return true;
}

b32 NativeAutoSave_Write(void)
{
	struct NativeAutoSaveFile save;

	// Never write an empty profile.
	if (sdata->advProgress.characterID < 0)
	{
		return false;
	}

	RaceConfig_SaveGameOptions();
	GAMEPROG_SaveCupProgress();

	memset(&save, 0, sizeof(save));
	save.magic = NATIVE_AUTOSAVE_MAGIC;
	save.version = NATIVE_AUTOSAVE_VERSION;
	save.size = sizeof(save);
	save.adv = sdata->advProgress;
	save.gameProgress = sdata->gameProgress;
	save.gameOptions = sdata->gameOptions;
	save.checksum = NativeAutoSave_Checksum(&save);

	if (NativeMemcard_WriteSaveData(NATIVE_AUTOSAVE_NAME, "", 0, (const u8 *)&save, sizeof(save)) != NATIVE_MEMCARD_OK)
	{
		return false;
	}

	s_nativeAutoSave = save;
	s_nativeAutoSaveValid = true;
	return true;
}

// Called once when an adventure hub finishes loading. A race, boss fight or
// arena (levelID < GEM_STONE_VALLEY) is the only thing that can precede a hub
// load with a different hub; hub-to-hub swaps, the main menu/garage and
// cutscenes are ignored.
void NativeAutoSave_OnHubLoaded(void)
{
	struct GameTracker *gGT = sdata->gGT;

	if (((gGT->gameMode1 & ADVENTURE_MODE) == 0) || ((gGT->gameMode1 & ADVENTURE_ARENA) == 0) || ((gGT->gameMode1 & (MAIN_MENU | GAME_CUTSCENE)) != 0))
	{
		return;
	}

	if ((gGT->prevLEV < 0) || (gGT->prevLEV >= GEM_STONE_VALLEY))
	{
		return;
	}

	// HubLevYouSavedOn is the hub being entered, as SelectProfile_GetTrackID does.
	sdata->advProgress.HubLevYouSavedOn = gGT->levelID;
	NativeAutoSave_Write();
}
