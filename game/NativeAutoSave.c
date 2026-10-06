#include <common.h>
#include <platform/native_memcard.h>
#include <platform/native_custom_racer.h>
#include <platform/native_engine.h>

// Native-only adventure autosave. This is a separate file next to the normal
// SLOTS file and never goes through the RefreshCard state machine.
#define NATIVE_AUTOSAVE_NAME "BASCUS-94426-AUTO"

enum
{
	NATIVE_AUTOSAVE_MAGIC = 0x31545541, // "AUT1"
	NATIVE_AUTOSAVE_VERSION = 2,
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
	s32 exitPortalHub;
	s32 exitPortalID;
};

static struct NativeAutoSaveFile s_nativeAutoSave;
static b32 s_nativeAutoSaveValid;
static s32 s_exitPortalHub = -1;
static s32 s_exitPortalID = -1;

void NativeAutoSave_ResetExitPortal(void)
{
	s_exitPortalHub = -1;
	s_exitPortalID = -1;
}

// Remember the pad's own ID, including synthetic gem cup IDs, before loading
// its race. This stays available after the hub is replaced by character select.
void NativeAutoSave_SetExitPortal(int hubID, int portalID)
{
	s_exitPortalHub = hubID;
	s_exitPortalID = portalID;
}

int NativeAutoSave_GetExitPortalHub(void)
{
	return (s_exitPortalID >= 0 && (u32)(s_exitPortalHub - GEM_STONE_VALLEY) < 5) ? s_exitPortalHub : -1;
}

int NativeAutoSave_GetExitPortal(int hubID)
{
	return (s_exitPortalHub == hubID) ? s_exitPortalID : -1;
}

static u32 NativeAutoSave_Checksum(const struct NativeAutoSaveFile *save)
{
	const u8 *bytes = (const u8 *)&save->adv;
	u32 hash = 2166136261u;

	for (u32 i = 0; i < save->size - OFFSETOF(struct NativeAutoSaveFile, adv); i++)
	{
		hash = (hash ^ bytes[i]) * 16777619u;
	}

	return hash;
}

// Reads and validates one generation (the file or its backup) into the cache.
static b32 NativeAutoSave_TryRead(b32 backup)
{
	struct NativeAutoSaveFile save;
	enum NativeMemcardResult (*readFn)(const char *, unsigned char *, int, int) = backup ? NativeMemcard_ReadBackupData : NativeMemcard_ReadSaveData;

	memset(&save, 0, sizeof(save));

	// Version 1 ends at gameOptions. Read its original size and checksum so
	// existing autosaves remain usable with the default spawn.
	const u32 legacySize = OFFSETOF(struct NativeAutoSaveFile, exitPortalHub);
	if (readFn(NATIVE_AUTOSAVE_NAME, (u8 *)&save, OFFSETOF(struct NativeAutoSaveFile, adv), 0) != NATIVE_MEMCARD_OK)
	{
		return false;
	}
	if ((save.magic != NATIVE_AUTOSAVE_MAGIC) ||
	    !(((save.version == 1) && (save.size == legacySize)) || ((save.version == NATIVE_AUTOSAVE_VERSION) && (save.size == sizeof(save)))))
	{
		return false;
	}
	const u32 saveSize = save.size;
	const u32 saveVersion = save.version;
	if ((readFn(NATIVE_AUTOSAVE_NAME, (u8 *)&save, saveSize, 0) == NATIVE_MEMCARD_OK) && (save.magic == NATIVE_AUTOSAVE_MAGIC) && (save.size == saveSize) &&
	    (save.version == saveVersion) && (save.checksum == NativeAutoSave_Checksum(&save)) && (save.adv.characterID >= 0))
	{
		if (save.version == 1)
		{
			save.exitPortalHub = -1;
			save.exitPortalID = -1;
		}
		s_nativeAutoSave = save;
		return true;
	}
	return false;
}

// Re-reads the file and caches it. Called when the Adventure Load screen opens
// and after every write; Exists/Read only touch the cache. A file that fails
// validation is quarantined and replaced by the previous valid generation.
void NativeAutoSave_Refresh(void)
{
	s_nativeAutoSaveValid = false;

	if (NativeAutoSave_TryRead(false))
	{
		s_nativeAutoSaveValid = true;
		return;
	}

	if (!NativeMemcard_FileExists(NATIVE_AUTOSAVE_NAME))
	{
		return;
	}

	if (NativeAutoSave_TryRead(true) && (NativeMemcard_RecoverFromBackup(NATIVE_AUTOSAVE_NAME) == NATIVE_MEMCARD_OK))
	{
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
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);

	if (!s_nativeAutoSaveValid)
	{
		return false;
	}

	GAMEPROG_SyncGameAndCard(&s_nativeAutoSave.gameProgress, &sdata->gameProgress);
	sdata->advProgress = s_nativeAutoSave.adv;
	NativeAutoSave_SetExitPortal(s_nativeAutoSave.exitPortalHub, s_nativeAutoSave.exitPortalID);
	NativeCustomRacer_ClearDriverSelections();
	NativeEngine_LoadAdventureProfile(&sdata->advProgress);
	data.characterIDs[0] = sdata->advProgress.characterID;
	memmove(gGT->prevNameEntered, sdata->advProgress.name, sizeof(gGT->prevNameEntered));
	return true;
}

// Loads the autosave and queues the saved hub directly, skipping the
// profile screen (mirrors SelectProfile_FinalizeAdventure for ADV_LOAD).
b32 NativeAutoSave_QuickLoad(void)
{
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);

	if (!NativeAutoSave_Apply())
	{
		return false;
	}

	// Not a card slot, so a later manual save asks for overwrite confirmation.
	sdata->advProfileIndex = 0xffff;
	gGT->currLEV = (sdata->advProgress.HubLevYouSavedOn != 0) ? sdata->advProgress.HubLevYouSavedOn : N_SANITY_BEACH;
	memmove(gGT->prevNameEntered, sdata->advProgress.name, sizeof(gGT->prevNameEntered));
	memmove(gGT->currNameEntered, sdata->advProgress.name, sizeof(gGT->currNameEntered));
	P32_SET(sdata->ptrDesiredMenu, QueueLoadTrack_GetMenuPtr());
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
	NativeEngine_SaveAdventureProfile(&save.adv);
	save.gameProgress = sdata->gameProgress;
	save.gameOptions = sdata->gameOptions;
	save.exitPortalHub = s_exitPortalHub;
	save.exitPortalID = s_exitPortalID;
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
// arena or gem cup can trigger this save; hub-to-hub swaps, the main
// menu/garage and cutscenes are ignored.
void NativeAutoSave_OnHubLoaded(void)
{
	struct GameTracker *gGT = P32_GET(struct GameTracker *, sdata->gGT);

	if (((gGT->gameMode1 & ADVENTURE_MODE) == 0) || ((gGT->gameMode1 & ADVENTURE_ARENA) == 0) || ((gGT->gameMode1 & (MAIN_MENU | GAME_CUTSCENE)) != 0))
	{
		return;
	}

	if (!(((gGT->prevLEV >= 0) && (gGT->prevLEV < GEM_STONE_VALLEY)) || ((u32)(gGT->prevLEV - ADVENTURE_CUP_SYNTHETIC_LEVEL_ID_BASE) < 5)))
	{
		return;
	}

	// HubLevYouSavedOn is the hub being entered, as SelectProfile_GetTrackID does.
	sdata->advProgress.HubLevYouSavedOn = gGT->levelID;
	NativeAutoSave_Write();
}
