#ifndef PLATFORM_NATIVE_UPDATE_H
#define PLATFORM_NATIVE_UPDATE_H

#include <platform/native_options.h>

// Optional check for a newer build, against the website's /api/update.
//
// Nothing is checked until the player agrees at the boot prompt. The answer is
// config.ini's update_check (1 = check, 0 = don't), also under Options >
// Interface; while the key is absent the prompt is shown at each boot. Nothing
// is downloaded: when a newer build exists the player is offered the website's
// download page.

// -1 until the boot prompt has been answered, then 0 or 1.
#define NATIVE_UPDATE_CHECK_UNASKED (-1)
extern int gNativeUpdateCheck;

// Before the game window opens: asks if update_check is unset, then, when it
// is enabled, checks and offers the download page if a newer build exists.
void NativeUpdate_Boot(void);
// After Platform_Init: saves the prompt's answer. config.ini can't be written
// earlier because the input bindings only get their defaults in Platform_Init.
void NativeUpdate_SavePromptAnswer(void);

#endif
