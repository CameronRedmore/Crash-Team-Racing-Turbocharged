#include <common.h>
#include <platform/native_unlock.h>

#if defined(CTR_NATIVE)
#include <platform/native_custom_racer.h>


static const u32 s_nativeCheatMenuBits[] =
{
	CHEAT_WUMPA, CHEAT_MASK, CHEAT_TURBO, CHEAT_BOMBS, CHEAT_INVISIBLE, CHEAT_ENGINE,
	CHEAT_ICY, CHEAT_TURBOPAD, CHEAT_ADV,
};

static const u32 s_nativeCharacterUnlockMask[GAME_PROGRESS_UNLOCK_WORD_COUNT] =
{
	UNLOCK_CHARACTERS,
	MEMCARD_BIT_MASK(GAME_UNLOCK_BIT_OXIDE - 32),
};
#endif

u32 NativeCheat_GetMenuBit(int index)
{
#if defined(CTR_NATIVE)
	if ((index < 0) || ((u32)index >= (u32)(sizeof(s_nativeCheatMenuBits) / sizeof(s_nativeCheatMenuBits[0]))))
	{
		return 0;
	}
	return s_nativeCheatMenuBits[index];
#else
	(void)index;
	return 0;
#endif
}

void NativeCheat_ToggleAllCharacters(void)
{
#if defined(CTR_NATIVE)
	if (sdata == NULL)
	{
		return;
	}

	b32 allUnlocked = NativeCheat_AreAllCharactersUnlocked();
	for (u32 word = 0; word < GAME_PROGRESS_UNLOCK_WORD_COUNT; word++)
	{
		if (allUnlocked)
		{
			sdata->gameProgress.unlocks[word] &= ~s_nativeCharacterUnlockMask[word];
		}
		else
		{
			for (int bit = 0; bit < 32; bit++)
				if (s_nativeCharacterUnlockMask[word] & (1u << bit)) NativeUnlock_GrantBit((int)word * 32 + bit);
		}
	}
#endif
}

b32 NativeCheat_AreAllCharactersUnlocked(void)
{
#if defined(CTR_NATIVE)
	if (sdata == NULL)
	{
		return false;
	}

	for (u32 word = 0; word < GAME_PROGRESS_UNLOCK_WORD_COUNT; word++)
	{
		if ((sdata->gameProgress.unlocks[word] & s_nativeCharacterUnlockMask[word]) != s_nativeCharacterUnlockMask[word])
		{
			return false;
		}
	}
	return true;
#else
	return false;
#endif
}

void NativeCheat_ApplyConfigured(void)
{
#if defined(CTR_NATIVE)
	if ((sdata == NULL) || (P32_GET(struct GameTracker *, sdata->gGT) == NULL))
	{
		return;
	}
	sdata->gGT->gameMode2 = (sdata->gGT->gameMode2 & ~(CHEAT_ALL | CHEAT_TURBOCOUNT)) | (gNativeCheatConfigMask & (CHEAT_ALL | CHEAT_TURBOCOUNT));
#endif
}

b32 NativeCheat_DisablesRecords(void)
{
#if defined(CTR_NATIVE)
	if ((sdata == NULL) || (P32_GET(struct GameTracker *, sdata->gGT) == NULL))
		return 0;

	if ((P32_GET(struct GameTracker *, sdata->gGT)->gameMode2 & CHEAT_ALL) != 0)
		return 1;

	return ((P32_GET(struct GameTracker *, sdata->gGT)->gameMode1 & (TIME_TRIAL | RELIC_RACE)) != 0) && NativeCustomRacer_DisablesRecords();
#else
	return 0;
#endif
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800ac9fc-0x800aca34.
void MM_Cheat_MaxWumpa(void)
{
	P32_GET(struct GameTracker *, sdata->gGT)->gameMode2 |= CHEAT_WUMPA;
	OtherFX_Play(MM_CHEAT_SUCCESS_SFX, 1);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800aca34-0x800aca6c.
void MM_Cheat_UnlockRoo(void)
{
	NativeUnlock_GrantMask(UNLOCK_ROO);
	OtherFX_Play(MM_CHEAT_SUCCESS_SFX, 1);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800aca6c-0x800acaa4.
void MM_Cheat_UnlockPapu(void)
{
	NativeUnlock_GrantMask(UNLOCK_PAPU);
	OtherFX_Play(MM_CHEAT_SUCCESS_SFX, 1);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800acaa4-0x800acadc.
void MM_Cheat_UnlockJoe(void)
{
	NativeUnlock_GrantMask(UNLOCK_JOE);
	OtherFX_Play(MM_CHEAT_SUCCESS_SFX, 1);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800acadc-0x800acb14.
void MM_Cheat_UnlockPinstripe(void)
{
	NativeUnlock_GrantMask(UNLOCK_PINSTRIPE);
	OtherFX_Play(MM_CHEAT_SUCCESS_SFX, 1);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800acb14-0x800acb4c.
void MM_Cheat_UnlockFakeCrash(void)
{
	NativeUnlock_GrantMask(UNLOCK_FAKE_CRASH);
	OtherFX_Play(MM_CHEAT_SUCCESS_SFX, 1);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800acb4c-0x800acb84.
void MM_Cheat_UnlockPenta(void)
{
	NativeUnlock_GrantMask(UNLOCK_PENTA);
	OtherFX_Play(MM_CHEAT_SUCCESS_SFX, 1);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800acb84-0x800acbbc.
void MM_Cheat_UnlockTropy(void)
{
	NativeUnlock_GrantMask(UNLOCK_TROPY);
	OtherFX_Play(MM_CHEAT_SUCCESS_SFX, 1);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800acbbc-0x800acbf4.
void MM_Cheat_UnlockScrapbook(void)
{
	NativeUnlock_GrantBit(GAME_UNLOCK_BIT_SCRAPBOOK);
	OtherFX_Play(MM_CHEAT_SUCCESS_SFX, 1);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800acbf4-0x800acc2c.
void MM_Cheat_UnlockTracks(void)
{
	NativeUnlock_GrantMask(GAME_UNLOCK_TRACKS_MASK);
	OtherFX_Play(MM_CHEAT_SUCCESS_SFX, 1);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800acc2c-0x800acc64.
void MM_Cheat_InfiniteMasks(void)
{
	P32_GET(struct GameTracker *, sdata->gGT)->gameMode2 |= CHEAT_MASK;
	OtherFX_Play(MM_CHEAT_SUCCESS_SFX, 1);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800acc64-0x800acc9c.
void MM_Cheat_MaxTurbos(void)
{
	P32_GET(struct GameTracker *, sdata->gGT)->gameMode2 |= CHEAT_TURBO;
	OtherFX_Play(MM_CHEAT_SUCCESS_SFX, 1);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800acc9c-0x800accd4.
void MM_Cheat_MaxInvisibility(void)
{
	P32_GET(struct GameTracker *, sdata->gGT)->gameMode2 |= CHEAT_INVISIBLE;
	OtherFX_Play(MM_CHEAT_SUCCESS_SFX, 1);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800accd4-0x800acd10.
void MM_Cheat_MaxEngine(void)
{
	P32_GET(struct GameTracker *, sdata->gGT)->gameMode2 |= CHEAT_ENGINE;
	OtherFX_Play(MM_CHEAT_SUCCESS_SFX, 1);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800acd10-0x800acd4c.
void MM_Cheat_MaxBombs(void)
{
	P32_GET(struct GameTracker *, sdata->gGT)->gameMode2 |= CHEAT_BOMBS;
	OtherFX_Play(MM_CHEAT_SUCCESS_SFX, 1);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800acd4c-0x800acd88.
void MM_Cheat_AdvDifficulty(void)
{
	P32_GET(struct GameTracker *, sdata->gGT)->gameMode2 |= CHEAT_ADV;
	OtherFX_Play(MM_CHEAT_SUCCESS_SFX, 1);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800acdc4-0x800ace00.
void MM_Cheat_IcyTracks(void)
{
	P32_GET(struct GameTracker *, sdata->gGT)->gameMode2 |= CHEAT_ICY;
	OtherFX_Play(MM_CHEAT_SUCCESS_SFX, 1);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800ace00-0x800ace3c.
void MM_Cheat_SuperTurboPads(void)
{
	P32_GET(struct GameTracker *, sdata->gGT)->gameMode2 |= CHEAT_TURBOPAD;
	OtherFX_Play(MM_CHEAT_SUCCESS_SFX, 1);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800ace78-0x800aceb4.
void MM_Cheat_TurboCounter(void)
{
	P32_GET(struct GameTracker *, sdata->gGT)->gameMode2 |= CHEAT_TURBOCOUNT;
	OtherFX_Play(MM_CHEAT_SUCCESS_SFX, 1);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800aceb4-0x800acff4.
void MM_ParseCheatCodes(void)
{
	struct GamepadBuffer *gpad = &P32_GET(struct GamepadSystem *, sdata->gGamepads)->gamepad[0];

	// if not holding L1 and R1
	if ((gpad->buttonsHeldCurrFrame & (BTN_L1 | BTN_R1)) != (BTN_L1 | BTN_R1))
	{
		// skip function
		return;
	}

	u32 tap = (u32)gpad->buttonsTapped;
	if (tap == 0)
	{
		return;
	}

	// at this point, must be holding L1 and R1,
	// and also must have tapped a buttons

	// shift the loop
	for (s32 historyIndex = MM_CHEAT_BUTTON_HISTORY_COUNT - 1; historyIndex > 0; historyIndex--)
	{
		D230.cheatButtonHistory[historyIndex] = D230.cheatButtonHistory[historyIndex - 1];
	}

	// add to input
	D230.cheatButtonHistory[0] = tap;

	// loop through all cheats
	for (s32 cheatIndex = 0; cheatIndex < MM_CHEAT_COUNT; cheatIndex++)
	{
		b32 cheatMatches = true;

		// check if buttons match this cheat
		for (s32 buttonIndex = 0; buttonIndex < D230.cheats[cheatIndex].buttonCount; buttonIndex++)
		{
			// remember, inputButtons is backward
			s32 expectedButtonIndex = D230.cheats[cheatIndex].buttonCount - buttonIndex - 1;
			if ((D230.cheatButtonHistory[buttonIndex] & D230.cheats[cheatIndex].buttons[expectedButtonIndex]) == 0)
			{
				cheatMatches = false;
				break;
			}
		}

		// skip to next cheat if needed
		if (!cheatMatches)
		{
			continue;
		}

		if (P32_GET(void (*)(void), D230.cheats[cheatIndex].handler) != NULL)
		{
			P32_GET(void (*)(void), D230.cheats[cheatIndex].handler)();
		}
	}

	return;
}
