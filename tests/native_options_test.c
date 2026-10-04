// Registry parity test.
//
// The settings plumbing moved from a 38-branch strcmp chain in load_config and
// 36 hand-written fprintf lines in save_config to one table. This test pins the
// behaviour that refactor had to preserve: the exact ordered key list save_config
// emitted before the change, every key load_config accepted, each key's accepted
// range, and the precedence of the three historical minimap aliases.
//
// The expected key order below is copied from save_config as it stood at the
// fork point of the refactor; if a row is added, removed or reordered this test
// fails and the config file's compatibility is a deliberate decision rather
// than an accident.

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <macros.h>
#include <platform/native_options.h>
#include <platform/native_physics.h>

// ---------------------------------------------------------------------------
// Storage the registry points at. In the game these live in their owning
// modules; here the test owns them so the registry can be exercised alone.
// ---------------------------------------------------------------------------

int gNativeAspectRatio;
int gNativeFovDegrees;
int gNativeProjectionMode;
int gNativeProjectionStrength;
int gNativeAntiAliasingMode;
int gNativeDitheringEnabled;
int gNativeBorderlessEnabled;
int gNativePgxpMode;
int gNativePgxpIntegerNclipEnabled;
int gNativeRendererMode;
int gNativeColorDepth;
int gNativeDepthBufferEnabled;
int gNativeHdPauseMode;
int gNativeModernMapEnabled;
int gNativeModernHudIconsEnabled;
int gNativeFont;
int gNativeKartHue;
int gNativeDefaultCameraFar;
int gNativeDefaultHudSpeedometer;
int gNativeSkipMaskHints;
int gNative60FpsEnabled;
int gNativeMirrorModeEnabled;
int gNativeAIRacersMode;
int gNativeEngineSelectionEnabled;
int gNativeAdditionalUnlocksEnabled;
int gNativeSmoothedPhysicsEnabled;
int gNativeSmoothedAIEnabled;
int gNativeSmoothedCollisionEnabled;
int gNativeSmoothedSteeringEnabled;
int gNativeMaxLodEnabled;
int g_cfg_bilinearFiltering;
u32 gNativeCheatConfigMask;
int gNativePresetPending;
int cfg_language;
s32 s_nativeLanguageChosen;

// Faithful to NativePhysics_SetDomain's value semantics: store the flag and
// drop cached per-driver state. The real reset touches the physics arena, which
// this test does not link.
static int s_physicsResets;
void NativePhysics_SetDomain(enum NativePhysicsDomain domain, int enabled)
{
	int *modes[] = {&gNativeSmoothedPhysicsEnabled, &gNativeSmoothedAIEnabled, &gNativeSmoothedCollisionEnabled, &gNativeSmoothedSteeringEnabled};
	if ((unsigned)domain >= sizeof(modes) / sizeof(modes[0]))
	{
		return;
	}
	*modes[domain] = enabled != 0;
	s_physicsResets++;
}
void NativePhysics_SetEnabled(int enabled)
{
	NativePhysics_SetDomain(NATIVE_PHYSICS_PLAYER, enabled);
}

#include "../platform/native_options.c"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Applies one "key=value" line the way load_config's loop does.
static int ApplyLine(const char *key, int value)
{
	const struct NativeOption *option = NativeOption_Find(key);
	return option ? NativeOption_Apply(option, value) : 0;
}

static void ResetToDefaults(void)
{
	for (unsigned int i = 0; i < g_nativeOptionCount; i++)
	{
		const struct NativeOption *option = &g_nativeOptions[i];
		if (option->value != NULL)
		{
			*option->value = option->defaultValue;
		}
	}
	gNativeFovDegrees = 0;
	gNative60FpsEnabled = 0;
	gNativeAIRacersMode = NATIVE_AI_RACERS_EXTENDED;
	gNativePresetPending = 0;
	g_cfg_bilinearFiltering = 0;
	gNativeModernMapEnabled = 0;
	gNativeSmoothedPhysicsEnabled = 0;
	gNativeSmoothedAIEnabled = 0;
	gNativeSmoothedCollisionEnabled = 1;
	gNativeSmoothedSteeringEnabled = 0;
	s_nativeLanguageChosen = 0;
	s_physicsResets = 0;
	NativeOptions_BeginLoad();
}

// save_config's key order at the point the registry replaced it. Cheat and
// binding keys follow these and are emitted by main.c, not the registry.
static const char *const s_expectedWriteOrder[] = {"language",
                                                   "aspect_ratio",
                                                   "fov_degrees",
                                                   "projection_mode",
                                                   "projection_strength",
                                                   "preset_seen",
                                                   "mirror_mode",
                                                   "60fps",
                                                   "frame_rate",
                                                   "default_camera_far",
                                                   "default_hud_speedometer",
                                                   "ai_racers",
                                                   "skip_mask_hints",
                                                   "engine_selection",
                                                   "additional_unlocks",
                                                   "anti_aliasing",
                                                   "dithering",
                                                   "borderless",
                                                   "pgxp",
                                                   "renderer",
                                                   "color_depth",
                                                   "texture_filter",
                                                   "pgxp_integer_nclip",
                                                   "modern_minimap",
                                                   "modern_hud_icons",
                                                   "font",
                                                   "kart_hue",
                                                   "max_lod",
                                                   "depth_buffer",
                                                   "hd_pause_screen",
                                                   "smoothed_physics",
                                                   "smoothed_ai",
                                                   "smoothed_collisions",
                                                   "smoothed_steering"};

// Keys load_config accepted before the refactor that are not written back.
static const char *const s_legacyOnlyKeys[] = {"custom_ai_racers", "modern_map", "precise_minimap"};

static void test_write_order_matches_previous_save_config(void)
{
	char path[] = "/tmp/ctr_options_order.ini";
	FILE *file = fopen(path, "w");
	assert(file != NULL);
	assert(NativeOptions_WriteAll(file));
	fclose(file);

	file = fopen(path, "r");
	assert(file != NULL);
	char line[128];
	unsigned int written = 0;
	while (fgets(line, sizeof(line), file) != NULL)
	{
		char *equals = strchr(line, '=');
		assert(equals != NULL);
		*equals = 0;
		assert(written < (unsigned int)(sizeof(s_expectedWriteOrder) / sizeof(s_expectedWriteOrder[0])));
		assert(strcmp(line, s_expectedWriteOrder[written]) == 0);
		written++;
	}
	fclose(file);
	remove(path);

	assert(written == sizeof(s_expectedWriteOrder) / sizeof(s_expectedWriteOrder[0]));
}

static void test_legacy_aliases_are_accepted_but_not_written(void)
{
	for (unsigned int i = 0; i < sizeof(s_legacyOnlyKeys) / sizeof(s_legacyOnlyKeys[0]); i++)
	{
		const struct NativeOption *option = NativeOption_Find(s_legacyOnlyKeys[i]);
		assert(option != NULL);
		assert(!option->persistent);
	}
}

static void test_bool_keys_normalise_and_round_trip(void)
{
	ResetToDefaults();
	const char *const boolKeys[] = {
	    "mirror_mode", "default_camera_far", "default_hud_speedometer", "skip_mask_hints",  "engine_selection", "additional_unlocks",
	    "dithering",   "borderless",         "pgxp_integer_nclip",      "modern_hud_icons", "max_lod",          "depth_buffer"};
	for (unsigned int i = 0; i < sizeof(boolKeys) / sizeof(boolKeys[0]); i++)
	{
		const struct NativeOption *option = NativeOption_Find(boolKeys[i]);
		assert(option != NULL && option->kind == NATIVE_OPTION_BOOL && option->value != NULL);
		assert(option->defaultValue == 0 || option->defaultValue == 1);

		assert(ApplyLine(boolKeys[i], 0));
		assert(*option->value == 0);
		// Any non-zero raw value collapses to 1, matching the old `(value != 0)`.
		assert(ApplyLine(boolKeys[i], 7));
		assert(*option->value == 1);
		assert(ApplyLine(boolKeys[i], -3));
		assert(*option->value == 1);
	}
}

static void test_enum_keys_reject_out_of_range_and_keep_previous(void)
{
	struct
	{
		const char *key;
		int lastValid;
	} enums[] = {
	    {"aspect_ratio", NATIVE_ASPECT_COUNT - 1},
	    {"projection_mode", NATIVE_PROJECTION_MODE_COUNT - 1},
	    {"anti_aliasing", NATIVE_AA_MODE_COUNT - 1},
	    {"pgxp", NATIVE_PGXP_MODE_COUNT - 1},
	    {"renderer", NATIVE_RENDERER_MODE_COUNT - 1},
	    {"color_depth", NATIVE_COLOR_DEPTH_COUNT - 1},
	    {"font", NATIVE_FONT_COUNT - 1},
	    {"kart_hue", NATIVE_KART_HUE_STEPS - 1},
	    {"ai_racers", NATIVE_AI_RACERS_MODE_COUNT - 1},
	};

	for (unsigned int i = 0; i < sizeof(enums) / sizeof(enums[0]); i++)
	{
		ResetToDefaults();
		const struct NativeOption *option = NativeOption_Find(enums[i].key);
		assert(option != NULL && option->kind == NATIVE_OPTION_ENUM);

		// Highest accepted value.
		assert(ApplyLine(enums[i].key, enums[i].lastValid));
		assert(*option->value == enums[i].lastValid);

		// Out of range on both ends: rejected, previous value retained. This is
		// what the old `cond ? value : fallback` produced for a valid default.
		int before = *option->value;
		assert(!ApplyLine(enums[i].key, enums[i].lastValid + 1));
		assert(*option->value == before);
		assert(!ApplyLine(enums[i].key, -1));
		assert(*option->value == before);
	}
}

static void test_numeric_overrides(void)
{
	ResetToDefaults();

	// fov_degrees: 0 means retail framing, otherwise 45..100 inclusive. Anything
	// else collapses to 0 rather than being rejected.
	assert(ApplyLine("fov_degrees", 0) && gNativeFovDegrees == 0);
	assert(ApplyLine("fov_degrees", 45) && gNativeFovDegrees == 45);
	assert(ApplyLine("fov_degrees", 100) && gNativeFovDegrees == 100);
	assert(ApplyLine("fov_degrees", 44) && gNativeFovDegrees == 0);
	assert(ApplyLine("fov_degrees", 101) && gNativeFovDegrees == 0);
	assert(ApplyLine("fov_degrees", -1) && gNativeFovDegrees == 0);

	// projection_strength is a 0..100 range.
	assert(ApplyLine("projection_strength", 0) && gNativeProjectionStrength == 0);
	assert(ApplyLine("projection_strength", 100) && gNativeProjectionStrength == 100);

	// hd_pause_screen clamped rather than rejected, as in the old parser.
	assert(ApplyLine("hd_pause_screen", -5) && gNativeHdPauseMode == 0);
	assert(ApplyLine("hd_pause_screen", 9) && gNativeHdPauseMode == 2);
	assert(ApplyLine("hd_pause_screen", 1) && gNativeHdPauseMode == 1);
}

static void test_frame_rate_keys_share_one_index(void)
{
	// "60fps" is the legacy bool spelling; "frame_rate" stores the rate itself.
	// Both write the same index, and the later key in the file wins.
	ResetToDefaults();
	assert(ApplyLine("60fps", 1));
	assert(gNative60FpsEnabled == 1);
	assert(ApplyLine("60fps", 0));
	assert(gNative60FpsEnabled == 0);

	assert(ApplyLine("frame_rate", 144));
	assert(gNative60FpsEnabled == 4);
	assert(ApplyLine("frame_rate", 240));
	assert(gNative60FpsEnabled == 5);

	// An unsupported rate leaves the index alone.
	assert(!ApplyLine("frame_rate", 75));
	assert(gNative60FpsEnabled == 5);

	// frame_rate writes the rate back, 60fps writes the bool.
	ResetToDefaults();
	ApplyLine("frame_rate", 90);
	char path[] = "/tmp/ctr_options_framerate.ini";
	FILE *file = fopen(path, "w");
	assert(file != NULL && NativeOptions_WriteAll(file));
	fclose(file);
	file = fopen(path, "r");
	assert(file != NULL);
	char line[128];
	int sawRate60 = 0, sawRate90 = 0;
	while (fgets(line, sizeof(line), file) != NULL)
	{
		if (strcmp(line, "60fps=1\n") == 0)
		{
			sawRate60 = 1;
		}
		if (strcmp(line, "frame_rate=90\n") == 0)
		{
			sawRate90 = 1;
		}
	}
	fclose(file);
	remove(path);
	assert(sawRate60 && sawRate90);
}

static void test_minimap_alias_precedence(void)
{
	// modern_minimap > modern_map > precise_minimap, regardless of file order.
	ResetToDefaults();
	ApplyLine("modern_map", 1);
	ApplyLine("modern_minimap", 0);
	assert(gNativeModernMapEnabled == 0);

	ResetToDefaults();
	ApplyLine("modern_minimap", 1);
	ApplyLine("modern_map", 0);
	assert(gNativeModernMapEnabled == 1);

	ResetToDefaults();
	ApplyLine("precise_minimap", 1);
	ApplyLine("modern_map", 0);
	assert(gNativeModernMapEnabled == 0);

	ResetToDefaults();
	ApplyLine("precise_minimap", 1);
	assert(gNativeModernMapEnabled == 1);

	// A second load pass resolves the aliases the same way as the first.
	ResetToDefaults();
	ApplyLine("modern_minimap", 1);
	ApplyLine("modern_map", 0);
	NativeOptions_BeginLoad();
	ApplyLine("modern_map", 0);
	assert(gNativeModernMapEnabled == 0);
}

static void test_texture_filter_stores_bool_but_writes_enum(void)
{
	ResetToDefaults();
	assert(ApplyLine("texture_filter", NATIVE_TEXTURE_FILTER_BILINEAR));
	assert(g_cfg_bilinearFiltering == 1);
	assert(ApplyLine("texture_filter", NATIVE_TEXTURE_FILTER_NEAREST));
	assert(g_cfg_bilinearFiltering == 0);
	// Anything that is not the bilinear enum value reads as nearest.
	assert(ApplyLine("texture_filter", 99));
	assert(g_cfg_bilinearFiltering == 0);
}

static void test_smoothed_domains_apply_through_physics(void)
{
	ResetToDefaults();
	assert(ApplyLine("smoothed_physics", 1));
	assert(gNativeSmoothedPhysicsEnabled == 1);
	assert(ApplyLine("smoothed_ai", 1));
	assert(gNativeSmoothedAIEnabled == 1);
	assert(ApplyLine("smoothed_collisions", 0));
	assert(gNativeSmoothedCollisionEnabled == 0);
	assert(ApplyLine("smoothed_steering", 1));
	assert(gNativeSmoothedSteeringEnabled == 1);
	// Each apply resets cached solver state, as the real setters do.
	assert(s_physicsResets == 4);
}

static void test_legacy_custom_ai_racers_maps_to_mode(void)
{
	ResetToDefaults();
	assert(ApplyLine("custom_ai_racers", 1));
	assert(gNativeAIRacersMode == NATIVE_AI_RACERS_EXTENDED_CUSTOM);
	assert(ApplyLine("custom_ai_racers", 0));
	assert(gNativeAIRacersMode == NATIVE_AI_RACERS_EXTENDED);
}

static void test_language_and_preset_seen(void)
{
	ResetToDefaults();
	// A config file that exists means the language prompt was already answered.
	assert(ApplyLine("language", 1));
	assert(cfg_language == 1);
	assert(s_nativeLanguageChosen == 1);

	// preset_seen is written as 1; a missing key leaves the install pending.
	assert(ApplyLine("preset_seen", 1));
	assert(gNativePresetPending == 0);
	assert(ApplyLine("preset_seen", 0));
	assert(gNativePresetPending == 1);
}

static void test_unknown_key_is_not_consumed(void)
{
	// load_config falls through to the cheat and binding handlers, so the
	// registry must not claim keys it does not own.
	assert(NativeOption_Find("cheat_wumpa") == NULL);
	assert(NativeOption_Find("bind_kb_cross") == NULL);
	assert(NativeOption_Find("not_a_setting") == NULL);
	assert(!ApplyLine("not_a_setting", 1));
	assert(NativeOption_Find(NULL) == NULL);
}

static void test_every_persistent_row_can_be_written(void)
{
	// A CUSTOM row without an encoder would silently vanish from config.ini.
	for (unsigned int i = 0; i < g_nativeOptionCount; i++)
	{
		const struct NativeOption *option = &g_nativeOptions[i];
		if (!option->persistent)
		{
			continue;
		}
		int value;
		assert(NativeOption_WriteValue(option, &value));
	}
}

static void test_registry_is_well_formed(void)
{
	for (unsigned int i = 0; i < g_nativeOptionCount; i++)
	{
		const struct NativeOption *option = &g_nativeOptions[i];
		assert(option->key != NULL && option->key[0] != 0);
		assert(strlen(option->key) < NATIVE_CONFIG_KEY_MAX); // NativeConfig_ReadEntry's key buffer

		switch (option->kind)
		{
		case NATIVE_OPTION_BOOL:
		case NATIVE_OPTION_ENUM:
		case NATIVE_OPTION_RANGE:
			assert(option->value != NULL);
			assert(option->decode == NULL);
			break;
		case NATIVE_OPTION_CUSTOM:
			assert(option->decode != NULL);
			break;
		}

		if (option->kind == NATIVE_OPTION_ENUM || option->kind == NATIVE_OPTION_RANGE)
		{
			assert(option->minInclusive < option->maxExclusive);
			assert(option->defaultValue >= option->minInclusive && option->defaultValue <= NativeOption_MaxValue(option));
		}

		for (unsigned int j = i + 1; j < g_nativeOptionCount; j++)
		{
			assert(strcmp(option->key, g_nativeOptions[j].key) != 0);
		}
	}
}

// ---------------------------------------------------------------------------
// config.ini parsing. These drive NativeConfig_ReadEntry, the loop load_config
// actually runs, rather than NativeOption_Apply alone. Before this the real
// parse path had no coverage at all: --version returns before load_config, so
// no test ever reached it.
//
// tmpfile() is used instead of fmemopen because the tests also build under
// MSVC and MinGW on Windows, and fmemopen is POSIX-only.
// ---------------------------------------------------------------------------

#define PARSE_MAX_ENTRIES   64
// A parser that fails to consume input spins forever rather than returning.
// Capping the pass turns that regression into a failed assert instead of a
// hung CI job.
#define PARSE_ITERATION_CAP 1000

typedef struct ParseResult
{
	int entries;
	int stalled;
	char keys[PARSE_MAX_ENTRIES][NATIVE_CONFIG_KEY_MAX];
	int values[PARSE_MAX_ENTRIES];
} ParseResult;

// Runs the same read loop load_config runs, dispatching into the registry.
static ParseResult ParseConfigText(const char *text)
{
	ParseResult result;
	FILE *file;
	memset(&result, 0, sizeof(result));

	file = tmpfile();
	assert(file != NULL);
	assert(fwrite(text, 1, strlen(text), file) == strlen(text));
	rewind(file);

	NativeOptions_BeginLoad();
	for (;;)
	{
		char key[NATIVE_CONFIG_KEY_MAX];
		int value;
		int read;
		if (result.entries + 1 > PARSE_ITERATION_CAP)
		{
			result.stalled = 1;
			break;
		}
		read = NativeConfig_ReadEntry(file, key, (int)sizeof(key), &value);
		if (read == NATIVE_CONFIG_EOF)
		{
			break;
		}
		if (read == NATIVE_CONFIG_MALFORMED)
		{
			continue;
		}
		result.entries++;
		if (result.entries <= PARSE_MAX_ENTRIES)
		{
			const struct NativeOption *option = NativeOption_Find(key);
			memcpy(result.keys[result.entries - 1], key, sizeof(key));
			result.values[result.entries - 1] = value;
			if (option != NULL)
			{
				NativeOption_Apply(option, value);
			}
		}
	}
	fclose(file);
	return result;
}

static void test_parse_reads_a_normal_file(void)
{
	ResetToDefaults();
	ParseResult r = ParseConfigText("language=2\nfov_degrees=90\nskip_mask_hints=1\n");
	assert(!r.stalled);
	assert(r.entries == 3);
	assert(strcmp(r.keys[0], "language") == 0 && r.values[0] == 2);
	assert(strcmp(r.keys[1], "fov_degrees") == 0 && r.values[1] == 90);
	assert(gNativeSkipMaskHints == 1);
}

// Regression: a line whose scan position starts on '=' made fscanf("%29[^=]...")
// fail without consuming input, so load_config looped while the result was not
// EOF and the game hung on startup at full CPU.
static void test_parse_survives_a_line_starting_with_equals(void)
{
	ResetToDefaults();
	ParseResult r = ParseConfigText("=\nlanguage=3\n");
	assert(!r.stalled);
	assert(r.entries == 1);
	assert(strcmp(r.keys[0], "language") == 0 && r.values[0] == 3);
	assert(cfg_language == 3); // the valid key after the bad line still applies

	ResetToDefaults();
	r = ParseConfigText(" = 7\nfov_degrees=60\n");
	assert(!r.stalled);
	assert(gNativeFovDegrees == 60);
}

// The ordinary hand-edit typo: one '=' too many.
static void test_parse_survives_double_equals(void)
{
	ResetToDefaults();
	ParseResult r = ParseConfigText("language=0\nkey==1\nfov_degrees=80\n");
	assert(!r.stalled);
	assert(gNativeFovDegrees == 80);
	assert(cfg_language == 0);
}

// The old scan set spanned newlines, so a bad value swallowed the following
// line into its key and the next setting was silently dropped.
static void test_parse_bad_value_does_not_swallow_the_next_line(void)
{
	ResetToDefaults();
	ParseResult r = ParseConfigText("key=abc\nlanguage=4\n");
	assert(!r.stalled);
	assert(r.entries == 1);
	assert(strcmp(r.keys[0], "language") == 0);
	assert(cfg_language == 4);

	ResetToDefaults();
	r = ParseConfigText("language=\nfov_degrees=70\n");
	assert(!r.stalled);
	assert(gNativeFovDegrees == 70);
}

// Truncating a long key could yield a prefix that matches a real setting.
static void test_parse_rejects_overlong_key_without_matching_a_real_one(void)
{
	char longKey[NATIVE_CONFIG_KEY_MAX + 8];
	char text[NATIVE_CONFIG_KEY_MAX + 40];

	ResetToDefaults();
	gNativeFovDegrees = 0;
	// Longer than the key buffer, and ending in a real key name so a truncated
	// read would land on fov_degrees.
	memset(longKey, 'z', sizeof(longKey) - 1);
	longKey[sizeof(longKey) - 1] = '\0';
	sprintf(text, "%s=90\n", longKey);
	ParseResult r = ParseConfigText(text);
	assert(!r.stalled);
	assert(r.entries == 0);
	assert(gNativeFovDegrees == 0); // untouched

	// A long line must not be re-read as a second entry either.
	ResetToDefaults();
	r = ParseConfigText("zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz=1\nlanguage=5\n");
	assert(!r.stalled);
	assert(cfg_language == 5);
}

static void test_parse_rejects_bad_values_and_keeps_the_previous(void)
{
	ResetToDefaults();
	ParseResult r = ParseConfigText("fov_degrees=1abc\nfov_degrees=99999999999999999999\nfov_degrees=60\n");
	assert(!r.stalled);
	// Only the clean line is reported; the others are skipped whole.
	assert(r.entries == 1);
	assert(gNativeFovDegrees == 60);

	// A negative value is well-formed even when the registry later rejects it.
	ResetToDefaults();
	r = ParseConfigText("fov_degrees=-5\n");
	assert(!r.stalled);
	assert(r.entries == 1 && r.values[0] == -5);
}

static void test_parse_terminates_on_empty_and_garbage_files(void)
{
	const char *cases[] = {"", "\n", "\n\n\n", "no equals here\n", "=", "==\n", "=\n=\n=\n", "a=1", "# note=x\n[a=b]\n"};
	for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		ResetToDefaults();
		ParseResult r = ParseConfigText(cases[i]);
		assert(!r.stalled);
	}
}

// Whatever save_config writes, the parser must read straight back.
static void test_parse_round_trips_what_write_all_emits(void)
{
	char written[8192];
	size_t length;
	FILE *out = tmpfile();
	assert(out != NULL);

	ResetToDefaults();
	assert(NativeOptions_WriteAll(out));
	fflush(out);
	length = (size_t)ftell(out);
	assert(length > 0 && length < sizeof(written));
	rewind(out);
	assert(fread(written, 1, length, out) == length);
	fclose(out);
	written[length] = '\0';

	ParseResult r = ParseConfigText(written);
	assert(!r.stalled);

	// Every key written must be readable, and must be a registry key.
	int writtenKeys = 0;
	for (unsigned int i = 0; i < g_nativeOptionCount; i++)
	{
		if (g_nativeOptions[i].persistent)
		{
			writtenKeys++;
		}
	}
	assert(r.entries == writtenKeys);
	for (int i = 0; i < r.entries; i++)
	{
		assert(NativeOption_Find(r.keys[i]) != NULL);
	}
}

// The registry's defaultValue is documentation plus a save-time fallback, not
// the initialiser. Several defaults are genuinely platform-conditional at the
// definition site -- gNativeDepthBufferEnabled is 1 on PC and 0 on Vita,
// gNativePgxpMode is PERSPECTIVE where NATIVE_PGXP_SUPPORTED is set and OFF
// where it is not -- so a single registry row cannot be the source of truth
// without flattening that. What can be pinned is that every recorded default is
// a value the option actually accepts, and that saving it reproduces it.
static void test_registry_default_is_accepted_and_round_trips(void)
{
	ResetToDefaults();
	for (unsigned int i = 0; i < g_nativeOptionCount; i++)
	{
		const struct NativeOption *option = &g_nativeOptions[i];
		if (option->kind != NATIVE_OPTION_ENUM && option->kind != NATIVE_OPTION_RANGE)
		{
			continue;
		}
		assert(option->value != NULL);

		*option->value = option->defaultValue;
		assert(NativeOption_Apply(option, option->defaultValue));
		assert(*option->value == option->defaultValue);

		int written = 0;
		assert(NativeOption_WriteValue(option, &written));
		assert(written == option->defaultValue);
	}
	ResetToDefaults();
}

// Storage below minInclusive is saved as the default; storage above the maximum
// is clamped to the maximum rather than replaced. That asymmetry came from the
// original parser and is only visible if it is pinned.
static void test_below_minimum_saves_the_default_not_the_stored_value(void)
{
	const struct NativeOption *option = NativeOption_Find("projection_strength");
	int written = 0;

	assert(option != NULL);
	assert(option->minInclusive == 0);
	assert(NativeOption_MaxValue(option) == 100);

	ResetToDefaults();
	*option->value = -7;
	assert(NativeOption_WriteValue(option, &written));
	assert(written == option->defaultValue);

	*option->value = 500;
	assert(NativeOption_WriteValue(option, &written));
	assert(written == NativeOption_MaxValue(option));

	ResetToDefaults();
}

int main(void)
{
	ResetToDefaults();
	test_registry_is_well_formed();
	test_write_order_matches_previous_save_config();
	test_legacy_aliases_are_accepted_but_not_written();
	test_bool_keys_normalise_and_round_trip();
	test_enum_keys_reject_out_of_range_and_keep_previous();
	test_numeric_overrides();
	test_frame_rate_keys_share_one_index();
	test_minimap_alias_precedence();
	test_texture_filter_stores_bool_but_writes_enum();
	test_smoothed_domains_apply_through_physics();
	test_legacy_custom_ai_racers_maps_to_mode();
	test_language_and_preset_seen();
	test_unknown_key_is_not_consumed();
	test_every_persistent_row_can_be_written();
	test_parse_reads_a_normal_file();
	test_parse_survives_a_line_starting_with_equals();
	test_parse_survives_double_equals();
	test_parse_bad_value_does_not_swallow_the_next_line();
	test_parse_rejects_overlong_key_without_matching_a_real_one();
	test_parse_rejects_bad_values_and_keeps_the_previous();
	test_parse_terminates_on_empty_and_garbage_files();
	test_parse_round_trips_what_write_all_emits();
	test_registry_default_is_accepted_and_round_trips();
	test_below_minimum_saves_the_default_not_the_stored_value();
	printf("native_options: all checks passed (%u settings)\n", g_nativeOptionCount);
	return 0;
}
