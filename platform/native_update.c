#include "platform/native_update.h"

int gNativeUpdateCheck = NATIVE_UPDATE_CHECK_UNASKED;

#if NATIVE_UPDATE_CHECK_SUPPORTED

#include "platform/native_log.h"
#if defined(_WIN32)
#include "platform/native_http_win32.h"
#endif

#include <SDL3/SDL.h>

#ifndef CTR_NATIVE_UPDATE_URL
#define CTR_NATIVE_UPDATE_URL "https://ctr.cmzi.uk/api/update"
#endif
#ifndef CTR_NATIVE_UPDATE_CHANNEL
#define CTR_NATIVE_UPDATE_CHANNEL "stable"
#endif
#ifndef CTR_NATIVE_BUILD_TIME
#define CTR_NATIVE_BUILD_TIME "0"
#endif

#if defined(_WIN32)
#define NATIVE_UPDATE_PLATFORM "windows"
#else
#define NATIVE_UPDATE_PLATFORM "linux"
#endif
#if defined(__aarch64__) || defined(_M_ARM64)
#define NATIVE_UPDATE_ARCH "arm64"
#elif defined(__x86_64__) || defined(_M_X64)
#define NATIVE_UPDATE_ARCH "x64"
#else
#define NATIVE_UPDATE_ARCH "x86"
#endif

#define NATIVE_UPDATE_RESPONSE_MAX (16 * 1024)
// How long boot waits for the answer before carrying on without it.
#define NATIVE_UPDATE_WAIT_MS      3000

extern void save_config(void);

enum NativeUpdateState
{
	NATIVE_UPDATE_IDLE,
	NATIVE_UPDATE_RUNNING,
	NATIVE_UPDATE_DONE
};

struct NativeUpdateResult
{
	int available;
	char commit[41];
	char date[11];
	char downloadUrl[256];
};

global_variable SDL_AtomicInt s_nativeUpdateState;
global_variable struct NativeUpdateResult s_nativeUpdateResult;
global_variable char s_nativeUpdateRequestUrl[512];
global_variable int s_nativeUpdateAnswered;

// The build's commit without any "-dirty" suffix, or "" when it isn't known.
internal void NativeUpdate_GetCommit(char *commit, size_t size)
{
	size_t length = 0;
	const char *buildId = CTR_NATIVE_BUILD_ID;
	while ((length + 1 < size) && SDL_isxdigit((unsigned char)buildId[length]))
	{
		commit[length] = (char)SDL_tolower((unsigned char)buildId[length]);
		length++;
	}
	commit[length < 7 ? 0 : length] = '\0';
}

// Finds "key": in the website's flat JSON and returns the value after it.
internal const char *NativeUpdate_JsonValue(const char *json, const char *key)
{
	char quoted[64];
	SDL_snprintf(quoted, sizeof(quoted), "\"%s\"", key);
	const char *found = SDL_strstr(json, quoted);
	if (found == NULL)
	{
		return NULL;
	}
	found += SDL_strlen(quoted);
	while (SDL_isspace((unsigned char)*found))
	{
		found++;
	}
	if (*found++ != ':')
	{
		return NULL;
	}
	while (SDL_isspace((unsigned char)*found))
	{
		found++;
	}
	return found;
}

// Copies a JSON string value, keeping only characters in allowed: the values
// read here are hashes, dates and URLs, so anything else means a bad response.
internal int NativeUpdate_JsonString(const char *json, const char *key, const char *allowed, char *out, size_t size)
{
	const char *value = NativeUpdate_JsonValue(json, key);
	size_t length = 0;
	if ((value == NULL) || (*value++ != '"'))
	{
		return 0;
	}
	while ((value[length] != '"') && (value[length] != '\0'))
	{
		if ((length + 1 >= size) || !(SDL_isalnum((unsigned char)value[length]) || SDL_strchr(allowed, value[length]) != NULL))
		{
			return 0;
		}
		out[length] = value[length];
		length++;
	}
	out[length] = '\0';
	return value[length] == '"';
}

internal int NativeUpdate_Fetch(const char *url, char **body)
{
	*body = NULL;
#if defined(_WIN32)
	NativeWinHttpResponse response;
	if (!NativeWinHttp_Request("GET", url, NULL, NULL, NULL, 0, NATIVE_UPDATE_RESPONSE_MAX, &response) || (response.data == NULL))
	{
		Platform_Log("[Update] Check failed (HTTP %ld)\n", response.status);
		NativeWinHttp_FreeResponse(&response);
		return 0;
	}
	*body = (char *)response.data;
	return 1;
#else
	// No HTTP library is linked on Linux, and a 32-bit build can't count on a
	// 32-bit libcurl being installed, so the system's curl command does it.
	const char *args[] = {"curl",
	                      "--silent",
	                      "--fail",
	                      "--proto",
	                      "=https,http",
	                      "--connect-timeout",
	                      "3",
	                      "--max-time",
	                      "5",
	                      "--max-filesize",
	                      "16384",
	                      "--user-agent",
	                      "CTR-Turbocharged/" CTR_NATIVE_VERSION,
	                      url,
	                      NULL};
	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)args);
	SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
	SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_NULL);
	SDL_Process *process = SDL_CreateProcessWithProperties(props);
	SDL_DestroyProperties(props);
	if (process == NULL)
	{
		Platform_Log("[Update] Could not run curl: %s\n", SDL_GetError());
		return 0;
	}
	size_t size = 0;
	int exitCode = -1;
	char *output = (char *)SDL_ReadProcess(process, &size, &exitCode);
	SDL_DestroyProcess(process);
	if ((output == NULL) || (exitCode != 0) || (size > NATIVE_UPDATE_RESPONSE_MAX))
	{
		Platform_Log("[Update] Check failed (curl exit %d)\n", exitCode);
		SDL_free(output);
		return 0;
	}
	*body = output;
	return 1;
#endif
}

internal void NativeUpdate_FreeBody(char *body)
{
#if defined(_WIN32)
	free(body);
#else
	SDL_free(body);
#endif
}

internal int SDLCALL NativeUpdate_Thread(void *userdata)
{
	(void)userdata;
	struct NativeUpdateResult result = {0};
	char *body = NULL;
	if (NativeUpdate_Fetch(s_nativeUpdateRequestUrl, &body))
	{
		const char *available = NativeUpdate_JsonValue(body, "updateAvailable");
		result.available = (available != NULL) && (SDL_strncmp(available, "true", 4) == 0);
		if (result.available && (!NativeUpdate_JsonString(body, "downloadUrl", ":/.#?=&_-", result.downloadUrl, sizeof(result.downloadUrl)) ||
		                         (SDL_strncmp(result.downloadUrl, "https://", 8) != 0)))
		{
			result.available = 0;
		}
		NativeUpdate_JsonString(body, "commit", "", result.commit, sizeof(result.commit));
		// Only the YYYY-MM-DD part of the ISO date is kept.
		char date[40];
		if (NativeUpdate_JsonString(body, "date", ":.-+", date, sizeof(date)))
		{
			SDL_strlcpy(result.date, date, sizeof(result.date));
		}
		Platform_Log("[Update] %s (newest build %s)\n", result.available ? "A newer build is available" : "Up to date",
		             result.commit[0] ? result.commit : "unknown");
		NativeUpdate_FreeBody(body);
	}
	s_nativeUpdateResult = result;
	SDL_SetAtomicInt(&s_nativeUpdateState, NATIVE_UPDATE_DONE);
	return 0;
}

// Shown before the game window exists, like the disc setup dialogs. On Linux
// SDL draws message boxes with zenity, a separate window that can't be tied to
// the game's, so over a borderless fullscreen game it could open behind it and
// leave the game looking frozen.
internal void NativeUpdate_Ask(void)
{
	SDL_MessageBoxButtonData buttons[] = {{SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Don't check"},
	                                      {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Check for updates"}};
	SDL_MessageBoxData box = {SDL_MESSAGEBOX_INFORMATION,
	                          NULL,
	                          "Check for updates?",
	                          "Turbocharged can check ctr.cmzi.uk for a newer build each time it starts, and tell you when one is available. "
	                          "Nothing is downloaded or installed automatically.\n\n"
	                          "The check only sends which build you're running: its commit, platform and release channel.\n\n"
	                          "You can change this later in Options > Interface > Check for updates.",
	                          2,
	                          buttons,
	                          NULL};
	// Closing the dialog leaves chosen untouched, which counts as "Don't check".
	int chosen = -1;
	if (!SDL_ShowMessageBox(&box, &chosen))
	{
		// No dialog could be shown, so nothing was answered: ask again next boot.
		Platform_Log("[Update] Could not show the update prompt: %s\n", SDL_GetError());
		return;
	}
	gNativeUpdateCheck = (chosen == 1);
	s_nativeUpdateAnswered = 1;
}

internal int NativeUpdate_Start(void)
{
	char commit[41];
	NativeUpdate_GetCommit(commit, sizeof(commit));
	long long buildTime = SDL_strtoll(CTR_NATIVE_BUILD_TIME, NULL, 10);
	if ((commit[0] == '\0') || (buildTime <= 0))
	{
		Platform_Log("[Update] Skipping the update check: this build's commit is unknown\n");
		return 0;
	}
	// CTR_TURBOCHARGED_UPDATE_URL points the check at another server, such as a local copy of the website.
	const char *base = SDL_getenv("CTR_TURBOCHARGED_UPDATE_URL");
	if ((base == NULL) || (base[0] == '\0'))
	{
		base = CTR_NATIVE_UPDATE_URL;
	}
	SDL_snprintf(s_nativeUpdateRequestUrl, sizeof(s_nativeUpdateRequestUrl),
	             "%s?channel=" CTR_NATIVE_UPDATE_CHANNEL "&platform=" NATIVE_UPDATE_PLATFORM "&arch=" NATIVE_UPDATE_ARCH "&commit=%s&time=%lld", base, commit,
	             buildTime);

	SDL_SetAtomicInt(&s_nativeUpdateState, NATIVE_UPDATE_RUNNING);
	SDL_Thread *thread = SDL_CreateThread(NativeUpdate_Thread, "update-check", NULL);
	if (thread == NULL)
	{
		Platform_Log("[Update] Could not start the update check: %s\n", SDL_GetError());
		SDL_SetAtomicInt(&s_nativeUpdateState, NATIVE_UPDATE_IDLE);
		return 0;
	}
	// Never joined: a check that outlives NATIVE_UPDATE_WAIT_MS is abandoned
	// and its result ignored.
	SDL_DetachThread(thread);
	return 1;
}

internal void NativeUpdate_Notify(void)
{
	char message[1024];
	char current[41];
	NativeUpdate_GetCommit(current, sizeof(current));
	SDL_snprintf(message, sizeof(message),
	             "A newer build of Turbocharged is available%s%s%s%s.\n\n"
	             "You're running build %.7s. Open the website to download the new build?\n\n"
	             "To stop checking for updates, turn off Options > Interface > Check for updates.",
	             s_nativeUpdateResult.commit[0] ? ": build " : "", s_nativeUpdateResult.commit, s_nativeUpdateResult.date[0] ? " from " : "",
	             s_nativeUpdateResult.date, current);
	SDL_MessageBoxButtonData buttons[] = {{SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Not now"},
	                                      {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Open download page"}};
	SDL_MessageBoxData box = {SDL_MESSAGEBOX_INFORMATION, NULL, "Update available", message, 2, buttons, NULL};
	int chosen = 0;
	if (SDL_ShowMessageBox(&box, &chosen) && (chosen == 1) && !SDL_OpenURL(s_nativeUpdateResult.downloadUrl))
	{
		Platform_Log("[Update] Could not open %s: %s\n", s_nativeUpdateResult.downloadUrl, SDL_GetError());
	}
}

void NativeUpdate_Boot(void)
{
	if (gNativeUpdateCheck == NATIVE_UPDATE_CHECK_UNASKED)
	{
		NativeUpdate_Ask();
	}
	if ((gNativeUpdateCheck != 1) || !NativeUpdate_Start())
	{
		return;
	}
	// The answer usually arrives in well under a second; offline, curl and
	// WinHTTP fail at once. Only a stalled connection uses the whole wait.
	const Uint64 deadline = SDL_GetTicks() + NATIVE_UPDATE_WAIT_MS;
	while ((SDL_GetAtomicInt(&s_nativeUpdateState) != NATIVE_UPDATE_DONE) && (SDL_GetTicks() < deadline))
	{
		SDL_Delay(10);
	}
	if (SDL_GetAtomicInt(&s_nativeUpdateState) != NATIVE_UPDATE_DONE)
	{
		Platform_Log("[Update] No answer within %d ms; skipping the update check this time\n", NATIVE_UPDATE_WAIT_MS);
		return;
	}
	if (s_nativeUpdateResult.available)
	{
		NativeUpdate_Notify();
	}
}

void NativeUpdate_SavePromptAnswer(void)
{
	if (s_nativeUpdateAnswered)
	{
		save_config();
	}
}

#else
void NativeUpdate_Boot(void)
{
}
void NativeUpdate_SavePromptAnswer(void)
{
}
#endif
