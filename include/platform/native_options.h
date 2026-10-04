#ifndef NATIVE_OPTIONS_H
#define NATIVE_OPTIONS_H

// Single declaration point for every user-facing setting.
//
// Before this header existed, one setting had to be declared and handled in six
// places: a global definition, an extern in a module header, a strcmp branch in
// load_config with inline range validation, a matching fprintf in save_config
// with the validation repeated, a menu row plus label table plus a case in
// RECTMENU_GetString, and the behaviour guard. The externs had also drifted:
// 28 game files carried their own hand-written `extern int gNative...` blocks,
// and four globals were declared twice.
//
// Declarations now live here. The description of each setting - its config.ini
// key, storage, accepted range, default and display strings - lives in one
// table in platform/native_options.c, and load_config, save_config and the
// options menu all read that table.
//
// Adding a setting should now mean: define the global in its owning module,
// add one row to s_nativeOptions, and add one row to the menu table.

#include <stdio.h>

#include <macros.h>

// ---------------------------------------------------------------------------
// Config-backed settings. Each is written back to config.ini under the key
// named in the registry, except where marked runtime-only.
// ---------------------------------------------------------------------------

// Display
extern int gNativeAspectRatio;    // enum NativeAspectRatio
extern int gNativeFovDegrees;     // 0 = retail, else 45..100
extern int gNativeProjectionMode; // enum NativeProjectionMode
extern int gNativeProjectionStrength;
extern int gNativeAntiAliasingMode; // enum NativeAntiAliasingMode
extern int gNativeDitheringEnabled;
extern int gNativeBorderlessEnabled;
extern int gNativePgxpMode; // enum NativePgxpMode
extern int gNativePgxpIntegerNclipEnabled;
extern int gNativeRendererMode; // enum NativeRendererMode
extern int gNativeColorDepth;   // enum NativeColorDepth
extern int gNativeDepthBufferEnabled;
extern int gNativeHdPauseMode; // 0 retail VRAM copy, 1 posterised, 2 smooth

// Interface
extern int gNativeModernMapEnabled;
extern int gNativeModernHudIconsEnabled;
extern int gNativeFont;    // enum NativeFont
extern int gNativeKartHue; // 0 = retail, else 1..NATIVE_KART_HUE_STEPS - 1
extern int gNativeDefaultCameraFar;
extern int gNativeDefaultHudSpeedometer;
extern int gNativeSkipMaskHints;

// Gameplay
extern int gNative60FpsEnabled; // frame-rate index, see native_framerate.h
extern int gNativeMirrorModeEnabled;
extern int gNativeAIRacersMode; // enum NativeAIRacersMode
extern int gNativeEngineSelectionEnabled;
extern int gNativeAdditionalUnlocksEnabled;
extern int gNativeSmoothedPhysicsEnabled;
extern int gNativeSmoothedAIEnabled;
extern int gNativeSmoothedCollisionEnabled;
extern int gNativeSmoothedSteeringEnabled;
extern int gNativeMaxLodEnabled;

// Texture filtering is stored in the shared bilinear flag rather than a
// gNative-prefixed global, so it is declared here too.
extern int g_cfg_bilinearFiltering;

// Cheats are persisted as one bitmask rather than one global per cheat.
extern u32 gNativeCheatConfigMask;

// Language is persisted like the other settings but stores in the shared
// cfg_language rather than a gNative-prefixed global. s_nativeLanguageChosen
// records that the first-boot language prompt has been answered.
extern int cfg_language;
extern s32 s_nativeLanguageChosen;

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------

// How a raw config.ini integer is interpreted and validated.
enum NativeOptionKind
{
	// Stored as 0/1; any non-zero raw value reads as 1.
	NATIVE_OPTION_BOOL,
	// Stored verbatim; accepted only when minInclusive <= value < maxExclusive.
	// Pair maxExclusive with a *_COUNT sentinel.
	NATIVE_OPTION_ENUM,
	// Stored verbatim; accepted only when minInclusive <= value <= maxExclusive.
	NATIVE_OPTION_RANGE,
	// decode() owns both validation and storage (legacy aliases, derived
	// values, and settings with side effects on apply).
	NATIVE_OPTION_CUSTOM
};

struct NativeOption
{
	const char *key;
	enum NativeOptionKind kind;
	// Storage for BOOL/ENUM/RANGE; NULL for CUSTOM, which writes its own target.
	int *value;
	int minInclusive;
	// Exclusive upper bound, for every kind. ENUM rows state it as the enum's
	// _COUNT sentinel; RANGE rows state one past the largest accepted value
	// (0..100 is written 0,101). Use NativeOption_MaxValue() for the largest
	// accepted value rather than subtracting here.
	int maxExclusive;
	// The value this setting is documented to start at, and the value written
	// when storage is found below minInclusive.
	//
	// This is documentation plus a save-time fallback, NOT the initialiser.
	// The authoritative default is the global's definition site, deliberately:
	// several defaults are genuinely platform-conditional and a single registry
	// row cannot express both. gNativeDepthBufferEnabled is 1 on PC and 0 on
	// Vita, and gNativePgxpMode is PERSPECTIVE where NATIVE_PGXP_SUPPORTED and
	// OFF where it is not. Moving defaults into the registry would flatten that.
	int defaultValue;
	// NATIVE_OPTION_CUSTOM only: validates and applies rawValue. Returns 1 when
	// the value was accepted.
	int (*decode)(int rawValue);
	// Returns the integer to write for this option, or NATIVE_OPTION_SKIP to
	// omit it. NULL writes *value (BOOL writes 0/1).
	int (*encode)(const struct NativeOption *option);
	// 1 = written back by NativeOptions_WriteAll. 0 marks a read-only legacy
	// alias that is accepted for compatibility but never re-emitted.
	unsigned char persistent;
};

// Returned by encode() to leave a key out of the written config.
#define NATIVE_OPTION_SKIP (-0x40000000)

// The registry, in the order keys are written to config.ini.
extern const struct NativeOption g_nativeOptions[];
extern const unsigned int g_nativeOptionCount;

// Finds an option by config.ini key. Returns NULL when no option owns the key.
const struct NativeOption *NativeOption_Find(const char *key);

// Largest value an ENUM or RANGE option accepts. maxExclusive is one past it,
// so this exists to keep the subtraction in one place instead of open-coding
// `maxExclusive - 1` at each clamp.
int NativeOption_MaxValue(const struct NativeOption *option);

// Validates and applies rawValue to option. Returns 1 when accepted.
int NativeOption_Apply(const struct NativeOption *option, int rawValue);

// Clears per-parse state shared by aliased keys. Call before each pass over
// config.ini so a second load resolves aliases the same way as the first.
void NativeOptions_BeginLoad(void);

// Writes every persistent option as "key=value\n" in registry order.
int NativeOptions_WriteAll(FILE *file);

// Largest config.ini key NativeConfig_ReadEntry reports. Longer keys are
// rejected rather than truncated. Well clear of the longest key the game
// writes: "default_hud_speedometer" (23) and "bind_pad_triangle" (17).
#define NATIVE_CONFIG_KEY_MAX   64

// Reads the next "key=value" line from file into key (at most keySize bytes)
// and outValue. Always consumes the whole line, so malformed input cannot
// affect the entries that follow it.
#define NATIVE_CONFIG_ENTRY     (1)  // a well-formed line was returned
#define NATIVE_CONFIG_MALFORMED (0)  // unusable line; skip it and keep reading
#define NATIVE_CONFIG_EOF       (-1) // nothing left to read
int NativeConfig_ReadEntry(FILE *file, char *key, int keySize, int *outValue);

#endif
