#ifndef PLATFORM_H
#define PLATFORM_H

struct PlatformMempackArena
{
	void *base;
	void *start;
	void *endOfMemory;
	int size;
	int backingSize;
};

void Platform_Init(const char *title, int width, int height);
void Platform_Shutdown(void);
void Platform_InitScratchpad(void);
const struct PlatformMempackArena *Platform_InitMempackArena(void);
const struct PlatformMempackArena *Platform_GetMempackArena(void);
void Platform_BeginFrame(void);
int Platform_BeginScene(void);
void Platform_EndScene(void);
void Platform_EndFrame(void);
void Platform_PresentVRAMDisplay(void);
// Shows a centred message on a black screen right now, for long loads that block
// the game thread. The next normal frame replaces it. No-op on Vita.
void Platform_ShowBusyMessage(const char *title, const char *detail, int percent);
// Runs task on a worker thread while this thread shows the message with a
// progress bar (*progress holds 0-100, may be NULL) and keeps the window alive.
// The task must not touch OpenGL. Vita runs it inline.
void Platform_RunBusyTask(const char *title, const char *detail, int (*task)(void *), void *arg, volatile int *progress);
void Platform_PinVRAMDisplayFrames(int frameCount);
void Platform_PinVRAMDisplayRect(int x, int y, int w, int h, int frameCount);
void Platform_PinTextureDisplay(unsigned int texture, int contentHeight, int displayHeight, int frameCount);
int Platform_GetVBlankCount(void);
void Platform_WaitUntilVBlank(int targetVBlank);
void Platform_SetVBlankPacingScale(int speedNumerator, int speedDenominator);
void Platform_PollHostEvents(void);
int Platform_PollInput(void);
int Platform_InputStartPressed(void);
#if defined(CTR_NATIVE) && !defined(__vita__)
void Platform_SetBorderless(int enabled);
#endif

#if defined(CTR_NATIVE)
enum NativeAIRacersMode
{
	NATIVE_AI_RACERS_RETAIL = 0,
	NATIVE_AI_RACERS_EXTENDED,
	NATIVE_AI_RACERS_EXTENDED_CUSTOM,
	NATIVE_AI_RACERS_MODE_COUNT,
};

int NikoGetEnterKey(void);
#endif

#endif
