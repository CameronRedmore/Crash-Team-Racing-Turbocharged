// Registry describing every user-facing setting: config.ini key, storage,
// accepted range, default, and whether the key is written back.
//
// load_config and save_config consume this table, so a setting's validation is
// stated once instead of being duplicated between a load branch and a save
// branch. The options menu reads the same rows for its labels.

#include <platform/native_options.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <macros.h>
#include <native_framerate.h>
#include <platform.h>
#include <platform/native_aspect.h>
#include <platform/native_draw3d.h>
#include <platform/native_font.h>
#include <platform/native_kart_color.h>
#include <platform/native_pgxp.h>
#include <platform/native_physics.h>
#include <platform/native_projection.h>

// ---------------------------------------------------------------------------
// Custom decoders: legacy aliases, derived values, and settings whose apply has
// side effects. Each returns 1 when the raw value was accepted.
// ---------------------------------------------------------------------------

// fov_degrees stores either 0 (retail framing) or a 45..100 degree override.
static int NativeOption_DecodeFov(int rawValue)
{
	if (rawValue != 0 && (rawValue < 45 || rawValue > 100))
	{
		rawValue = 0;
	}
	gNativeFovDegrees = rawValue;
	return 1;
}

static int NativeOption_EncodeFov(const struct NativeOption *option)
{
	(void)option;
	// Out-of-range storage falls back to retail framing rather than being
	// written out, matching the save-time guard in the original parser.
	if (gNativeFovDegrees != 0 && (gNativeFovDegrees < 45 || gNativeFovDegrees > 100))
	{
		return 0;
	}
	return gNativeFovDegrees;
}

// Legacy 0/1 spelling of frame_rate: 0 selects index 0 (30 FPS), non-zero
// selects index 1 (60 FPS).
static int NativeOption_DecodeLegacyFrameRate(int rawValue)
{
	gNative60FpsEnabled = (rawValue != 0) ? 1 : 0;
	return 1;
}

// frame_rate stores the rate itself; the global holds its index.
static int NativeOption_DecodeFrameRate(int rawValue)
{
	int index = NativeFrameRate_Index(rawValue);
	if (index < 0)
	{
		return 0;
	}
	gNative60FpsEnabled = index;
	return 1;
}

static int NativeOption_EncodeFrameRate(const struct NativeOption *option)
{
	(void)option;
	return NativeFrameRate_FromIndex(gNative60FpsEnabled);
}

// Legacy spelling of ai_racers from before the mode became a three-way choice.
static int NativeOption_DecodeLegacyAiRacers(int rawValue)
{
	gNativeAIRacersMode = rawValue ? NATIVE_AI_RACERS_EXTENDED_CUSTOM : NATIVE_AI_RACERS_EXTENDED;
	return 1;
}

// One setting, three historical keys. Highest precedence wins regardless of the
// order the keys appear in the file: modern_minimap > modern_map >
// precise_minimap. precise_minimap is accepted only so older configs keep their
// intent; it no longer has its own behaviour.
enum NativeMinimapAlias
{
	NATIVE_MINIMAP_ALIAS_NONE = 0,
	NATIVE_MINIMAP_ALIAS_PRECISE,
	NATIVE_MINIMAP_ALIAS_MAP,
	NATIVE_MINIMAP_ALIAS_MODERN
};
static enum NativeMinimapAlias s_minimapAliasApplied;

static int NativeOption_DecodeMinimapAlias(int rawValue, enum NativeMinimapAlias alias)
{
	if (alias >= s_minimapAliasApplied)
	{
		gNativeModernMapEnabled = (rawValue != 0);
		s_minimapAliasApplied = alias;
	}
	return 1;
}

static int NativeOption_DecodeModernMinimap(int rawValue)
{
	return NativeOption_DecodeMinimapAlias(rawValue, NATIVE_MINIMAP_ALIAS_MODERN);
}

static int NativeOption_DecodeLegacyModernMap(int rawValue)
{
	return NativeOption_DecodeMinimapAlias(rawValue, NATIVE_MINIMAP_ALIAS_MAP);
}

static int NativeOption_DecodeLegacyPreciseMinimap(int rawValue)
{
	return NativeOption_DecodeMinimapAlias(rawValue, NATIVE_MINIMAP_ALIAS_PRECISE);
}

#if !defined(__vita__) // the settings below exist only in the PC renderer
// Texture filtering shares the renderer's bilinear flag, so the stored bool and
// the config enum differ.
static int NativeOption_DecodeTextureFilter(int rawValue)
{
	g_cfg_bilinearFiltering = (rawValue == NATIVE_TEXTURE_FILTER_BILINEAR);
	return 1;
}

static int NativeOption_EncodeTextureFilter(const struct NativeOption *option)
{
	(void)option;
	return g_cfg_bilinearFiltering ? NATIVE_TEXTURE_FILTER_BILINEAR : NATIVE_TEXTURE_FILTER_NEAREST;
}

// The smoothed domains reset their floating-point state when they change, so
// they apply through the physics module rather than writing a global.
static int NativeOption_DecodeSmoothedAI(int rawValue)
{
	NativePhysics_SetDomain(NATIVE_PHYSICS_AI, rawValue);
	return 1;
}

static int NativeOption_DecodeSmoothedCollisions(int rawValue)
{
	NativePhysics_SetDomain(NATIVE_PHYSICS_COLLISION, rawValue);
	return 1;
}

static int NativeOption_DecodeSmoothedSteering(int rawValue)
{
	NativePhysics_SetDomain(NATIVE_PHYSICS_STEERING, rawValue);
	return 1;
}

static int NativeOption_DecodeSmoothedPhysics(int rawValue)
{
	NativePhysics_SetEnabled(rawValue);
	return 1;
}

#endif

static int NativeOption_DecodeLanguage(int rawValue)
{
	cfg_language = rawValue;
	s_nativeLanguageChosen = 1;
	return 1;
}

static int NativeOption_EncodeLanguage(const struct NativeOption *option)
{
	(void)option;
	return cfg_language;
}

// preset_seen is stored as "has the preset menu been answered" but the global
// tracks the inverse: a fresh install is still pending. Writing 1 clears the
// pending flag on the next boot.
static int NativeOption_DecodePresetSeen(int rawValue)
{
#if !defined(__vita__)
	gNativePresetPending = (rawValue == 0);
#else
	(void)rawValue;
#endif
	return 1;
}

static int NativeOption_EncodePresetSeen(const struct NativeOption *option)
{
	(void)option;
	return 1;
}

static int NativeOption_EncodeFrameRateIndex(const struct NativeOption *option)
{
	(void)option;
	return gNative60FpsEnabled != 0;
}

static int NativeOption_EncodeModernMap(const struct NativeOption *option)
{
	(void)option;
	return gNativeModernMapEnabled != 0;
}

static int NativeOption_EncodeSmoothedPhysics(const struct NativeOption *option)
{
	(void)option;
	return gNativeSmoothedPhysicsEnabled != 0;
}

static int NativeOption_EncodeSmoothedAI(const struct NativeOption *option)
{
	(void)option;
	return gNativeSmoothedAIEnabled != 0;
}

static int NativeOption_EncodeSmoothedCollisions(const struct NativeOption *option)
{
	(void)option;
	return gNativeSmoothedCollisionEnabled != 0;
}

static int NativeOption_EncodeSmoothedSteering(const struct NativeOption *option)
{
	(void)option;
	return gNativeSmoothedSteeringEnabled != 0;
}

// ---------------------------------------------------------------------------
// The registry. Order is the order keys are written to config.ini.
// ---------------------------------------------------------------------------

const struct NativeOption g_nativeOptions[] = {
    {
        .key = "language",
        .kind = NATIVE_OPTION_CUSTOM,
        .decode = NativeOption_DecodeLanguage,
        .encode = NativeOption_EncodeLanguage,
        .defaultValue = 2,
        .persistent = 1,
    },
    {
        .key = "aspect_ratio",
        .kind = NATIVE_OPTION_ENUM,
        .value = &gNativeAspectRatio,
        .minInclusive = NATIVE_ASPECT_4_3,
        .maxExclusive = NATIVE_ASPECT_COUNT,
        .defaultValue = NATIVE_ASPECT_16_9,
        .persistent = 1,
    },
    {
        .key = "fov_degrees",
        .kind = NATIVE_OPTION_CUSTOM,
        .decode = NativeOption_DecodeFov,
        .encode = NativeOption_EncodeFov,
        .defaultValue = 0,
        .persistent = 1,
    },
    {
        .key = "projection_mode",
        .kind = NATIVE_OPTION_ENUM,
        .value = &gNativeProjectionMode,
        .minInclusive = NATIVE_PROJECTION_PERSPECTIVE,
        .maxExclusive = NATIVE_PROJECTION_MODE_COUNT,
        .defaultValue = NATIVE_PROJECTION_PERSPECTIVE,
        .persistent = 1,
    },
    {
        .key = "projection_strength",
        .kind = NATIVE_OPTION_RANGE,
        .value = &gNativeProjectionStrength,
        .minInclusive = 0,
        .maxExclusive = 101,
        .defaultValue = 50,
        .persistent = 1,
    },
    {
        .key = "preset_seen",
        .kind = NATIVE_OPTION_CUSTOM,
        .decode = NativeOption_DecodePresetSeen,
        .encode = NativeOption_EncodePresetSeen,
        .defaultValue = 0,
        .persistent = 1,
    },
    {
        .key = "mirror_mode",
        .kind = NATIVE_OPTION_BOOL,
        .value = &gNativeMirrorModeEnabled,
        .defaultValue = 0,
        .persistent = 1,
    },
    {
        .key = "60fps",
        .kind = NATIVE_OPTION_CUSTOM,
        .decode = NativeOption_DecodeLegacyFrameRate,
        .encode = NativeOption_EncodeFrameRateIndex,
        .defaultValue = 0,
        .persistent = 1,
    },
    {
        .key = "frame_rate",
        .kind = NATIVE_OPTION_CUSTOM,
        .decode = NativeOption_DecodeFrameRate,
        .encode = NativeOption_EncodeFrameRate,
        .defaultValue = 30,
        .persistent = 1,
    },
    {
        .key = "default_camera_far",
        .kind = NATIVE_OPTION_BOOL,
        .value = &gNativeDefaultCameraFar,
        .defaultValue = 0,
        .persistent = 1,
    },
    {
        .key = "default_hud_speedometer",
        .kind = NATIVE_OPTION_BOOL,
        .value = &gNativeDefaultHudSpeedometer,
        .defaultValue = 0,
        .persistent = 1,
    },
    {
        .key = "ai_racers",
        .kind = NATIVE_OPTION_ENUM,
        .value = &gNativeAIRacersMode,
        .minInclusive = NATIVE_AI_RACERS_RETAIL,
        .maxExclusive = NATIVE_AI_RACERS_MODE_COUNT,
        .defaultValue = NATIVE_AI_RACERS_EXTENDED,
        .persistent = 1,
    },
    {
        .key = "skip_mask_hints",
        .kind = NATIVE_OPTION_BOOL,
        .value = &gNativeSkipMaskHints,
        .defaultValue = 0,
        .persistent = 1,
    },
    {
        .key = "engine_selection",
        .kind = NATIVE_OPTION_BOOL,
        .value = &gNativeEngineSelectionEnabled,
        .defaultValue = 0,
        .persistent = 1,
    },
    {
        .key = "additional_unlocks",
        .kind = NATIVE_OPTION_BOOL,
        .value = &gNativeAdditionalUnlocksEnabled,
        .defaultValue = 1,
        .persistent = 1,
    },
    // Legacy alias for ai_racers; accepted, never re-emitted.
    {
        .key = "custom_ai_racers",
        .kind = NATIVE_OPTION_CUSTOM,
        .decode = NativeOption_DecodeLegacyAiRacers,
        .persistent = 0,
    },
#if !defined(__vita__)
    {
        .key = "anti_aliasing",
        .kind = NATIVE_OPTION_ENUM,
        .value = &gNativeAntiAliasingMode,
        .minInclusive = NATIVE_AA_OFF,
        .maxExclusive = NATIVE_AA_MODE_COUNT,
        .defaultValue = NATIVE_AA_FXAA,
        .persistent = 1,
    },
    {
        .key = "dithering",
        .kind = NATIVE_OPTION_BOOL,
        .value = &gNativeDitheringEnabled,
        .defaultValue = 1,
        .persistent = 1,
    },
    {
        .key = "borderless",
        .kind = NATIVE_OPTION_BOOL,
        .value = &gNativeBorderlessEnabled,
        .defaultValue = 0,
        .persistent = 1,
    },
    {
        .key = "pgxp",
        .kind = NATIVE_OPTION_ENUM,
        .value = &gNativePgxpMode,
        .minInclusive = NATIVE_PGXP_MODE_OFF,
        .maxExclusive = NATIVE_PGXP_MODE_COUNT,
        .defaultValue = NATIVE_PGXP_MODE_PERSPECTIVE,
        .persistent = 1,
    },
#if NATIVE_DRAW3D_SUPPORTED
    {
        .key = "renderer",
        .kind = NATIVE_OPTION_ENUM,
        .value = &gNativeRendererMode,
        .minInclusive = NATIVE_RENDERER_CLASSIC,
        .maxExclusive = NATIVE_RENDERER_MODE_COUNT,
        .defaultValue = NATIVE_RENDERER_CLASSIC,
        .persistent = 1,
    },
#endif
    {
        .key = "color_depth",
        .kind = NATIVE_OPTION_ENUM,
        .value = &gNativeColorDepth,
        .minInclusive = NATIVE_COLOR_DEPTH_TRUE,
        .maxExclusive = NATIVE_COLOR_DEPTH_COUNT,
        .defaultValue = NATIVE_COLOR_DEPTH_TRUE,
        .persistent = 1,
    },
    {
        .key = "texture_filter",
        .kind = NATIVE_OPTION_CUSTOM,
        .decode = NativeOption_DecodeTextureFilter,
        .encode = NativeOption_EncodeTextureFilter,
        .defaultValue = NATIVE_TEXTURE_FILTER_NEAREST,
        .persistent = 1,
    },
    {
        .key = "pgxp_integer_nclip",
        .kind = NATIVE_OPTION_BOOL,
        .value = &gNativePgxpIntegerNclipEnabled,
        .defaultValue = 0,
        .persistent = 1,
    },
    {
        .key = "modern_minimap",
        .kind = NATIVE_OPTION_CUSTOM,
        .decode = NativeOption_DecodeModernMinimap,
        .encode = NativeOption_EncodeModernMap,
        .defaultValue = 0,
        .persistent = 1,
    },
    // Legacy aliases for modern_minimap; accepted, never re-emitted.
    {
        .key = "modern_map",
        .kind = NATIVE_OPTION_CUSTOM,
        .decode = NativeOption_DecodeLegacyModernMap,
        .persistent = 0,
    },
    {
        .key = "precise_minimap",
        .kind = NATIVE_OPTION_CUSTOM,
        .decode = NativeOption_DecodeLegacyPreciseMinimap,
        .persistent = 0,
    },
    {
        .key = "modern_hud_icons",
        .kind = NATIVE_OPTION_BOOL,
        .value = &gNativeModernHudIconsEnabled,
        .defaultValue = 0,
        .persistent = 1,
    },
    {
        .key = "font",
        .kind = NATIVE_OPTION_ENUM,
        .value = &gNativeFont,
        .minInclusive = NATIVE_FONT_ORIGINAL,
        .maxExclusive = NATIVE_FONT_COUNT,
        .defaultValue = NATIVE_FONT_ORIGINAL,
        .persistent = 1,
    },
    {
        .key = "kart_hue",
        .kind = NATIVE_OPTION_ENUM,
        .value = &gNativeKartHue,
        .minInclusive = 0,
        .maxExclusive = NATIVE_KART_HUE_STEPS,
        .defaultValue = 0,
        .persistent = 1,
    },
    {
        .key = "max_lod",
        .kind = NATIVE_OPTION_BOOL,
        .value = &gNativeMaxLodEnabled,
        .defaultValue = 1,
        .persistent = 1,
    },
    {
        .key = "depth_buffer",
        .kind = NATIVE_OPTION_BOOL,
        .value = &gNativeDepthBufferEnabled,
        .defaultValue = 1,
        .persistent = 1,
    },
    {
        // Clamped rather than rejected: older builds wrote out-of-range values.
        .key = "hd_pause_screen",
        .kind = NATIVE_OPTION_RANGE,
        .value = &gNativeHdPauseMode,
        .minInclusive = 0,
        .maxExclusive = 3,
        .defaultValue = 2,
        .persistent = 1,
    },
    {
        .key = "smoothed_physics",
        .kind = NATIVE_OPTION_CUSTOM,
        .decode = NativeOption_DecodeSmoothedPhysics,
        .encode = NativeOption_EncodeSmoothedPhysics,
        .defaultValue = 0,
        .persistent = 1,
    },
    {
        .key = "smoothed_ai",
        .kind = NATIVE_OPTION_CUSTOM,
        .decode = NativeOption_DecodeSmoothedAI,
        .encode = NativeOption_EncodeSmoothedAI,
        .defaultValue = 0,
        .persistent = 1,
    },
    {
        .key = "smoothed_collisions",
        .kind = NATIVE_OPTION_CUSTOM,
        .decode = NativeOption_DecodeSmoothedCollisions,
        .encode = NativeOption_EncodeSmoothedCollisions,
        .defaultValue = 1,
        .persistent = 1,
    },
    {
        .key = "smoothed_steering",
        .kind = NATIVE_OPTION_CUSTOM,
        .decode = NativeOption_DecodeSmoothedSteering,
        .encode = NativeOption_EncodeSmoothedSteering,
        .defaultValue = 0,
        .persistent = 1,
    },
#endif
};

const unsigned int g_nativeOptionCount = sizeof(g_nativeOptions) / sizeof(g_nativeOptions[0]);

void NativeOptions_BeginLoad(void)
{
	s_minimapAliasApplied = NATIVE_MINIMAP_ALIAS_NONE;
}

const struct NativeOption *NativeOption_Find(const char *key)
{
	if (key == NULL)
	{
		return NULL;
	}
	for (unsigned int i = 0; i < g_nativeOptionCount; i++)
	{
		if (strcmp(key, g_nativeOptions[i].key) == 0)
		{
			return &g_nativeOptions[i];
		}
	}
	return NULL;
}

int NativeOption_Apply(const struct NativeOption *option, int rawValue)
{
	if (option == NULL)
	{
		return 0;
	}

	switch (option->kind)
	{
	case NATIVE_OPTION_BOOL:
		if (option->value == NULL)
		{
			return 0;
		}
		*option->value = (rawValue != 0);
		return 1;

	case NATIVE_OPTION_ENUM:
		if (option->value == NULL || rawValue < option->minInclusive || rawValue >= option->maxExclusive)
		{
			return 0;
		}
		*option->value = rawValue;
		return 1;

	case NATIVE_OPTION_RANGE:
		if (option->value == NULL)
		{
			return 0;
		}
		// Clamped: the only RANGE rows are numeric overrides whose out-of-range
		// history is benign, and hd_pause_screen clamped in the original parser.
		*option->value =
		    (rawValue < option->minInclusive) ? option->minInclusive : ((rawValue > option->maxExclusive - 1) ? option->maxExclusive - 1 : rawValue);
		return 1;

	case NATIVE_OPTION_CUSTOM:
		return (option->decode != NULL) ? option->decode(rawValue) : 0;
	}

	return 0;
}

// The value to write for an option. ENUM and RANGE storage is clamped on the
// way out, matching the save-time guards in the original parser: a global can
// only leave its range through a bug, but a config file must never carry a
// value the loader would reject.
static int NativeOption_WriteValue(const struct NativeOption *option, int *outValue)
{
	if (option->encode != NULL)
	{
		int encoded = option->encode(option);
		if (encoded == NATIVE_OPTION_SKIP)
		{
			return 0;
		}
		*outValue = encoded;
		return 1;
	}

	if (option->value == NULL)
	{
		return 0;
	}

	switch (option->kind)
	{
	case NATIVE_OPTION_BOOL:
		*outValue = (*option->value != 0);
		return 1;
	case NATIVE_OPTION_ENUM:
	case NATIVE_OPTION_RANGE:
		*outValue = (*option->value < option->minInclusive) ? option->defaultValue
		                                                    : ((*option->value > option->maxExclusive - 1) ? option->maxExclusive - 1 : *option->value);
		return 1;
	case NATIVE_OPTION_CUSTOM:
		// A CUSTOM row must name its encoder; there is no way to infer storage.
		return 0;
	}
	return 0;
}

int NativeOptions_WriteAll(FILE *file)
{
	if (file == NULL)
	{
		return 0;
	}
	for (unsigned int i = 0; i < g_nativeOptionCount; i++)
	{
		int value;
		if (!g_nativeOptions[i].persistent || !NativeOption_WriteValue(&g_nativeOptions[i], &value))
		{
			continue;
		}
		if (fprintf(file, "%s=%d\n", g_nativeOptions[i].key, value) < 0)
		{
			return 0;
		}
	}
	return 1;
}

// Reads the next "key=value" line from file.
//
// Returns NATIVE_CONFIG_ENTRY with key/outValue filled on a well-formed line,
// NATIVE_CONFIG_MALFORMED when the line was unusable, or NATIVE_CONFIG_EOF at
// end of file. A malformed line is reported separately from end of file so the
// caller can skip it and keep going - collapsing the two would make a single
// bad line drop every setting after it.
//
// This replaced a single fscanf("%29[^=]=%d\n") in load_config, which had two
// faults that a hand-edited config.ini could hit:
//
//   - A scanf scan set does not stop at end of line, so a value that failed to
//     parse as a number left the file position mid-line and the *following*
//     line was swallowed as part of the key. "key=abc\nlanguage=2" read the
//     second entry as the key "abc\nlanguage", silently dropping language.
//   - Worse, load_config looped while the result was not EOF. When the scan
//     position sat on an '=' - a line starting with '=', or the ordinary typo
//     "key==1" - the %29[^=] conversion failed without consuming anything and
//     fscanf returned 0 forever. The loop never terminated and the game hung on
//     startup at full CPU before any window or asset load happened.
//
// An over-long key is rejected rather than truncated: truncation could produce
// a prefix that matches a real setting and silently change it.
int NativeConfig_ReadEntry(FILE *file, char *key, int keySize, int *outValue)
{
	char line[128];
	const char *keyStart;
	char *equals;
	char *parseEnd;
	char *trimEnd;
	long parsed;
	size_t keyLength;

	if ((file == NULL) || (key == NULL) || (outValue == NULL) || (keySize <= 0))
	{
		return NATIVE_CONFIG_EOF;
	}

	if (fgets(line, (int)sizeof(line), file) == NULL)
	{
		return NATIVE_CONFIG_EOF;
	}

	if (strchr(line, '\n') == NULL)
	{
		// fgets stopped early, so this line is longer than the buffer. Drain the
		// remainder instead of parsing it as a second entry.
		int character;
		while ((character = fgetc(file)) != EOF && character != '\n')
		{
		}
	}

	// Ignore leading whitespace when measuring the key, so " key=1" and
	// "key=1" mean the same thing.
	keyStart = line;
	while (isspace((unsigned char)*keyStart))
	{
		keyStart++;
	}

	equals = strchr(line, '=');
	if ((equals == NULL) || (equals < keyStart))
	{
		return NATIVE_CONFIG_MALFORMED;
	}

	// Trim whitespace the key picked up before the '='.
	trimEnd = equals;
	while ((trimEnd > keyStart) && isspace((unsigned char)trimEnd[-1]))
	{
		trimEnd--;
	}

	keyLength = (size_t)(trimEnd - keyStart);
	if ((keyLength == 0) || ((int)keyLength >= keySize))
	{
		return NATIVE_CONFIG_MALFORMED;
	}

	// strtol reports where the number ended, which is what makes a missing
	// value ("key=") and trailing garbage ("key=1abc") distinguishable from a
	// good read; %d accepted both and kept a stale value.
	errno = 0;
	parsed = strtol(equals + 1, &parseEnd, 10);
	if ((parseEnd == equals + 1) || (errno == ERANGE) || (parsed < INT_MIN) || (parsed > INT_MAX))
	{
		return NATIVE_CONFIG_MALFORMED;
	}
	while (isspace((unsigned char)*parseEnd))
	{
		parseEnd++;
	}
	if (*parseEnd != '\0')
	{
		return NATIVE_CONFIG_MALFORMED;
	}

	memcpy(key, keyStart, keyLength);
	key[keyLength] = '\0';
	*outValue = (int)parsed;
	return NATIVE_CONFIG_ENTRY;
}
