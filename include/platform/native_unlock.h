#ifndef NATIVE_UNLOCK_H
#define NATIVE_UNLOCK_H

#if defined(CTR_NATIVE)
// Only reward grants enter the queue. Save imports keep their existing flags silently.
void NativeUnlock_GrantBit(int bit);
void NativeUnlock_GrantMask(u32 mask);
void NativeUnlock_NotifySlideColiseum(void);
void NativeUnlock_Draw(void);
#else
#define NativeUnlock_GrantBit(bit) UNLOCK_ADV_BIT(sdata->gameProgress.unlocks, (bit))
#define NativeUnlock_GrantMask(mask) (sdata->gameProgress.unlockFlags |= (mask))
#endif

#endif
