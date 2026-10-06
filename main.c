#define _CRT_SECURE_NO_WARNINGS
#define SDL_MAIN_HANDLED

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#include "platform/native_win32.h"
#else
#include <unistd.h>
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#define _EnterCriticalSection(x)
#define EnterCriticalSection(x)
#define ExitCriticalSection()

#include "platform/native_assets.h"
#include "platform/native_aspect.h"
#include "platform/native_projection.h"
#include "platform/native_log.h"
#include "platform/native_memory.h"
#include "platform/native_perf.h"
#include "platform/native_replay_scheduler.h"
#include "platform/native_savestate.h"
#include "platform/native_adhoc.h"
#include "platform/native_custom_racer.h"
#include "platform/native_engine.h"
#include "platform/native_leaderboard.h"
#include "platform/native_network.h"
#include "platform/native_user_id.h"
#include "platform/native_update.h"

#ifdef __vita__
#include <vitasdk.h>
#include <dirent.h>
int _newlib_heap_size_user = 256 * 1024 * 1024;
#if 0
int __real_mkdir(const char *fname, mode_t mode);
int __wrap_mkdir(const char *fname, mode_t mode) {
	sceClibPrintf("mkdir %s\n", fname);
	char patched_fname[256];
	sprintf(patched_fname, "ux0:data/ctr/%s", fname);
	return __real_mkdir(patched_fname, mode);
}
FILE *__real_fopen(char *fname, char *mode);
FILE *__wrap_fopen(char *fname, char *mode) {
	sceClibPrintf("fopen %s\n", fname);
	char patched_fname[256];
	sprintf(patched_fname, "ux0:data/ctr/%s", fname);
	return __real_fopen(patched_fname, mode);
}
int __real_unlink(const char *fname);
int __wrap_unlink(const char *fname) {
	sceClibPrintf("unlink %s\n", fname);
	char patched_fname[256];
	sprintf(patched_fname, "ux0:data/ctr/%s", fname);
	return __real_unlink(patched_fname);
}
DIR *__real_opendir(const char *fname);
DIR *__wrap_opendir(const char *fname) {
	sceClibPrintf("opendir %s\n", fname);
	char patched_fname[256];
	sprintf(patched_fname, "ux0:data/ctr/%s", fname);
	return __real_opendir(patched_fname);
}
#endif
#endif

#include <platform.h>
#include <platform/native_input.h>
#include <platform/native_kart_color.h>
#include <platform/native_options.h>

int gNativeRelicRaceMode = 0;
int gNativeRelicRaceResultTier = -1;

#include "game/game_unity.h"

#include "game/zGlobal_RDATA.c"
#include "game/zGlobal_DATA.c"
#include "game/zGlobal_SDATA.c"

#undef RECT

#include "platform/native_disc_image.c"
#include "platform/native_assets.c"
#include "platform/native_disc_setup.c"
#include "platform/native_audio.c"
#include "platform/native_memory.c"
#include "platform/native_checkpoint.c"
#include "platform/native_checkpoint_file.c"
#include "platform/native_cd.c"
#include "platform/native_custom_racer.c"
#include "platform/native_gpu_links.c"
#include "platform/native_gpu.c"
#include "platform/native_gte_core.c"
#include "platform/native_pgxp.c"
#include "platform/native_draw3d.c"
#include "platform/native_aspect.c"
#include "platform/native_projection.c"
#include "platform/native_physics.c"
#include "platform/native_collision.c"
#if !defined(__EMSCRIPTEN__)
#include "platform/native_glad.c"
#endif
#include "platform/native_input.c"
#include "platform/native_network.c"
#include "platform/native_pc_account.c"
#include "platform/native_user_id.c"
#include "platform/native_discord.c"
#include "platform/native_leaderboard.c"
#include "platform/native_adhoc.c"
#include "platform/native_inline_c.c"
#include "platform/native_libapi.c"
#include "platform/native_libetc.c"
#include "platform/native_libgte.c"
#include "platform/native_libgpu.c"
#include "platform/native_libpad.c"
#include "platform/native_libspu.c"
#include "platform/native_log.c"
#include "platform/native_memcard.c"
#include "platform/native_memcard_adapter.c"
#include "platform/native_perf.c"
#include "platform/native_platform.c"
#include "platform/native_replay_scheduler.c"
// The renderer is split into subsystems that share one translation unit with
// the rest of the game; platform/native_renderer_internal.h is their contract
// and carries the shared state and the cross-module operations, so those do
// not depend on order. What a module keeps to itself -- the PSX shader objects
// and their variant enum in native_renderer_shaders.c -- is still reached by a
// later module through the unity build, so keep shaders before gpu_state.
#include "platform/renderer/native_renderer_core.c"
#include "platform/renderer/native_renderer_shaders.c"
#include "platform/renderer/native_renderer_textures.c"
#include "platform/renderer/native_renderer_submit.c"
#include "platform/renderer/native_renderer_targets.c"
#include "platform/renderer/native_renderer_vram.c"
#include "platform/renderer/native_renderer_p4.c"
#include "platform/renderer/native_renderer_gpu_state.c"
#include "platform/renderer/native_renderer_passes.c"
#include "platform/renderer/native_renderer_overlays.c"
#include "platform/renderer/native_renderer_device.c"
#include "platform/native_font.c"
#include "platform/native_title_logo.c"
#include "platform/native_kart_color.c"
#include "platform/native_minimap.c"
#include "platform/native_hud_icons.c"
#include "platform/native_savestate.c"
#include "platform/native_state.c"
#include "platform/native_str.c"
#include "platform/native_options.c"
#include "platform/native_update.c"

#ifndef CC
#if defined(__GNUC__)
#if _WIN32
#ifndef __clang__
#define CC "MINGW-GCC"
#else
#define CC "MINGW-CLANG"
#endif
#else
#ifndef __clang__
#define CC "GCC"
#else
#define CC "CLANG"
#endif
#endif
#elif defined(_MSC_VER)
#define CC "MSVC"
#else
#define CC "Unknown"
#endif
#endif

#ifndef CTR_NATIVE_VERSION
// Fallback for translation units compiled without the build's definitions
// (clangd, ad-hoc compiles). Deliberately not version-shaped: the previous
// 0.0.0-dev here could reach a leaderboard client version or a replay header
// and read as a real release. The build always supplies the real value from
// the VERSION file.
#define CTR_NATIVE_VERSION "unknown"
#endif

#ifndef CTR_PRODUCT_NAME
#define CTR_PRODUCT_NAME "Crash Team Racing: Turbocharged"
#endif

#ifndef CTR_NATIVE_BUILD_ID
#define CTR_NATIVE_BUILD_ID "unknown"
#endif

static int NativeConsole_ShouldPauseOnError(void)
{
#if defined(_WIN32)
	DWORD consoleProcesses[2];
	DWORD consoleProcessCount;

	if (GetConsoleWindow() == NULL)
		return 0;

	consoleProcessCount = GetConsoleProcessList(consoleProcesses, (DWORD)(sizeof(consoleProcesses) / sizeof(consoleProcesses[0])));
	return (consoleProcessCount == 1) && (consoleProcesses[0] == GetCurrentProcessId());
#else
	return 0;
#endif
}

static s32 NativeConsole_Return(const u32 result)
{
	if ((result != 0) && NativeConsole_ShouldPauseOnError())
	{
		fflush(stdout);
		fflush(stderr);
		fprintf(stderr, "\n[CTR Native] Press Enter to close this window...");
		fflush(stderr);

		while (getchar() != '\n' && !feof(stdin))
		{
		}
	}

	return (s32)result;
}

// TODO(aalhendi): just make an argparser?
static int NativeArg_IsVersion(const char *arg)
{
	return (arg != NULL) && ((strcmp(arg, "--version") == 0) || (strcmp(arg, "-v") == 0));
}

int gNative60FpsEnabled = 0;
int gNativeForce30Fps = 0;
int gNativeDefaultCameraFar = 0;
int gNativeDefaultHudSpeedometer = 0;
int gNativeSkipMaskHints = 0;
int gNativeAIRacersMode = NATIVE_AI_RACERS_EXTENDED;
u32 gNativeCheatConfigMask = 0;
#ifndef __vita__
int gNativeAntiAliasingMode = NATIVE_AA_FXAA;
int gNativeDitheringEnabled = 1;
int gNativePs1ResolutionEnabled = 0;
int gNativeBorderlessEnabled = 0;
int gNativeMaxLodEnabled = 1;
int gNativeHdPauseMode = 2;
#endif
int cfg_language = 2; // Default: PAL UK language

static const char *NativeConfig_GetPath(void)
{
#ifdef __vita__
	return "ux0:data/ctr/config.ini";
#elif defined(__EMSCRIPTEN__)
	return "/persistent/config.ini";
#else
	return "config.ini";
#endif
}

struct NativeCheatConfigEntry
{
	const char *key;
	u32 bit;
};

static const struct NativeCheatConfigEntry s_nativeCheatConfig[] =
{
	{"cheat_wumpa", CHEAT_WUMPA},
	{"cheat_mask", CHEAT_MASK},
	{"cheat_turbo", CHEAT_TURBO},
	{"cheat_bombs", CHEAT_BOMBS},
	{"cheat_invisible", CHEAT_INVISIBLE},
	{"cheat_engine", CHEAT_ENGINE},
	{"cheat_icy", CHEAT_ICY},
	{"cheat_turbopad", CHEAT_TURBOPAD},
	{"cheat_adv", CHEAT_ADV},
	{"cheat_turbocount", CHEAT_TURBOCOUNT},
};

static int NativeConfig_SetCheat(const char *key, int value)
{
	for (u32 i = 0; i < (u32)(sizeof(s_nativeCheatConfig) / sizeof(s_nativeCheatConfig[0])); i++)
	{
		if (strcmp(key, s_nativeCheatConfig[i].key) == 0)
		{
			if (value != 0)
			{
				gNativeCheatConfigMask |= s_nativeCheatConfig[i].bit;
			}
			else
			{
				gNativeCheatConfigMask &= ~s_nativeCheatConfig[i].bit;
			}
			return 1;
		}
	}
	return 0;
}

void load_config(void)
{
	char buffer[NATIVE_CONFIG_KEY_MAX];
	int value;
	gNativeAspectRatio = NATIVE_ASPECT_16_9;
	gNativeFovDegrees = 0;
	gNativeProjectionMode = NATIVE_PROJECTION_PERSPECTIVE;
	gNativeProjectionStrength = 50;
	FILE *config = fopen(NativeConfig_GetPath(), "r");
#ifndef __vita__
	gNativePresetPending = 1; // cleared by the preset_seen key: installs that haven't seen the preset menu get offered it
#endif
	if (config)
	{
		// Aliased keys resolve by precedence, not file order, so the state that
		// tracks which alias won has to be reset per pass.
		NativeOptions_BeginLoad();
		// One line per iteration. A malformed line is skipped, not fatal, and
		// never stalls the pass; see NativeConfig_ReadEntry.
		for (;;)
		{
			int read = NativeConfig_ReadEntry(config, buffer, (int)sizeof(buffer), &value);
			const struct NativeOption *option;
			if (read == NATIVE_CONFIG_EOF)
			{
				break;
			}
			if (read == NATIVE_CONFIG_MALFORMED)
			{
				continue;
			}
			option = NativeOption_Find(buffer);
			if (option != NULL)
			{
				NativeOption_Apply(option, value);
			}
			else if (NativeConfig_SetCheat(buffer, value))
			{
				// Persistent gameplay cheat toggle consumed.
			}
			else if (Platform_InputConfigSetBinding(buffer, value))
			{
				// Binding override consumed by the native input layer.
			}
		}
		fclose(config);
	}
}

void save_config(void)
{
	// Keep the config unwritten until a preset is chosen so an interrupted first launch repeats the popup.
	if (gNativePresetPending) return;
	FILE *config = fopen(NativeConfig_GetPath(), "w+");
	if (config != NULL)
	{
		// Registry order; read-only legacy aliases are skipped by the writer.
		NativeOptions_WriteAll(config);
		for (u32 i = 0; i < (u32)(sizeof(s_nativeCheatConfig) / sizeof(s_nativeCheatConfig[0])); i++)
		{
			fprintf(config, "%s=%d\n", s_nativeCheatConfig[i].key, (gNativeCheatConfigMask & s_nativeCheatConfig[i].bit) != 0);
		}
		for (int device = 0; device < PLATFORM_INPUT_BINDING_DEVICE_COUNT; device++)
		{
#ifdef __vita__
			if (device == PLATFORM_INPUT_BINDING_KBM)
			{
				continue;
			}
#endif
			for (int action = 0; action < PLATFORM_INPUT_BIND_ACTION_COUNT; action++)
			{
				char key[32];
				Platform_InputGetBindingConfigKey(action, device, key, sizeof(key));
				if (key[0] != 0)
				{
					fprintf(config, "%s=%d\n", key, Platform_InputGetBinding(action, device));
				}
			}
		}
		fclose(config);
#if defined(__EMSCRIPTEN__)
		NativeMemcard_RequestPersistenceSync();
#endif
	}
}

#ifdef __vita__
#include <pthread.h>
void *real_main(void *argv);

int main(int argc, char *argv[])
{
	pthread_t t;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 0x400000);
	pthread_create(&t, &attr, real_main, NULL);

	return sceKernelExitDeleteThread(0);
}

void *real_main(void *_argv)
{
	scePowerSetArmClockFrequency(444);
	scePowerSetBusClockFrequency(222);
	scePowerSetGpuClockFrequency(222);
	scePowerSetGpuXbarClockFrequency(166);
	sceAppUtilInit(&(SceAppUtilInitParam){}, &(SceAppUtilBootParam){});
	SceCommonDialogConfigParam cmnDlgCfgParam;
	sceCommonDialogConfigParamInit(&cmnDlgCfgParam);
	sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_LANG, (int *)&cmnDlgCfgParam.language);
	sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_ENTER_BUTTON, (int *)&cmnDlgCfgParam.enterButtonAssign);
	sceCommonDialogSetConfigParam(&cmnDlgCfgParam);
	
	load_config();
	sceIoMkdir("ux0:data/ctr/shader_cache", 0777);
	char **argv = _argv;
	int argc = 0;
#else
int main(int argc, char *argv[])
{
#endif
	for (int argIndex = 1; argIndex < argc; argIndex++)
	{
		if (NativeArg_IsVersion(argv[argIndex]))
		{
			printf("%s %s (%s)\n", CTR_PRODUCT_NAME, CTR_NATIVE_VERSION, CTR_NATIVE_BUILD_ID);
			return 0;
		}
	}

#if defined(__linux__) && !defined(__vita__) && !defined(__EMSCRIPTEN__) && (UINTPTR_MAX == 0xFFFFFFFFu)
	// 32-bit Linux builds only; 64-bit builds keep SDL's default driver order.
	// 32-bit builds do not get along with GPU drivers under native Wayland, so
	// prefer X11 (through XWayland on Wayland desktops), keeping Wayland only as
	// a fallback when X11 is unavailable. A SDL_VIDEODRIVER that asks for Wayland
	// is overridden unless CTR_TURBOCHARGED_ALLOW_WAYLAND=1 confirms it. Any other
	// SDL_VIDEODRIVER value (x11, offscreen, dummy...) is respected as given.
	{
		const char *requested = SDL_getenv("SDL_VIDEODRIVER");
		const char *allowWayland = SDL_getenv("CTR_TURBOCHARGED_ALLOW_WAYLAND");
		const int wantsWayland = (requested != NULL) && (SDL_strstr(requested, "wayland") != NULL);
		const int confirmed = (allowWayland != NULL) && (SDL_strcmp(allowWayland, "1") == 0);
		if (requested == NULL || requested[0] == '\0' || (wantsWayland && !confirmed))
		{
			SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "x11,wayland", SDL_HINT_OVERRIDE);
		}
	}
#endif

	printf("[CTR Native] Starting...\n");
	fflush(stdout);

#ifdef __vita__
	const char *sdlBasePath = "ux0:data/ctr";
#else
	const char *sdlBasePath = SDL_GetBasePath();
#endif
	printf("[CTR Native] SDL base path: %s\n", sdlBasePath ? sdlBasePath : "(null)");
	fflush(stdout);

	if (!NativeAssets_Init(sdlBasePath))
	{
		fprintf(stderr, "[CTR Native] Failed to initialize asset paths.\n");
		return NativeConsole_Return(1);
	}

	printf("[CTR Native] Product: %s\n", CTR_PRODUCT_NAME);
	printf("[CTR Native] Version: %s (%s)\n", CTR_NATIVE_VERSION, CTR_NATIVE_BUILD_ID);
	printf("[CTR Native] Built with: " CC "\n");
	printf("[CTR Native] Base: %s\n", NativeAssets_GetBaseDir());
	printf("[CTR Native] Assets: %s\n", NativeAssets_GetAssetDir());
	fflush(stdout);

	if (chdir(NativeAssets_GetBaseDir()) != 0)
	{
		fprintf(stderr, "[CTR Native] Failed to enter base directory: %s\n", NativeAssets_GetBaseDir());
		return NativeConsole_Return(1);
	}

#ifndef __vita__
	load_config();
#endif

	if (!NativeAssets_Validate())
	{
		if (!NativeDiscSetup_Run(1))
		{
			return NativeConsole_Return(1);
		}
	}
	else if (gNativePresetPending)
	{
		NativeDiscSetup_Run(0);
	}

	NativeUpdate_Boot();

	NativeCustomRacer_Scan();

#if defined(CTR_INTERNAL)
	if (NativeReplayScheduler_PrepareReportFromArgs(argc, argv) != 0)
	{
		return NativeConsole_Return(1);
	}
#endif

#if defined(__vita__)
	printf("[CTR Native] Turbocharged widescreen 960x544\n");
	Platform_Init("Crash Team Racing: Turbocharged", 960, 544);
#elif CTR_NATIVE_WIDESCREEN
	printf("[CTR Native] Turbocharged widescreen 1280x720\n");
	Platform_Init("Crash Team Racing: Turbocharged", 1280, 720);
#elif defined(USE_16BY9)
	printf("[CTR Native] Widescreen\n");
	Platform_Init("Crash Team Racing: Turbocharged", 1280, 720);
#else
	printf("[CTR Native] 4:3\n");
	Platform_Init("Crash Team Racing: Turbocharged", 800, 600);
#endif

#if defined(CTR_INTERNAL)
	if (NativePerf_ConfigureFromArgs(argc, argv) != 0)
	{
		Platform_LogFlush();
		Platform_Shutdown();
		return NativeConsole_Return(1);
	}
#endif

	NativeUpdate_SavePromptAnswer();

#if NATIVE_DRAW3D_SUPPORTED
	NativeRenderer_EnableGamePresentation(1);
#endif
	Platform_InitScratchpad();
	Platform_RepairResidentPointers(0);

#if defined(CTR_INTERNAL)
	if (NativeReplayScheduler_ConfigureFromArgs(argc, argv) != 0)
	{
		Platform_LogFlush();
		Platform_Shutdown();
		return NativeConsole_Return(1);
	}
#else
	(void)argc;
	(void)argv;
#endif

	const int result = CTR_Main();

	Platform_Shutdown();
	return NativeConsole_Return(result);
}
