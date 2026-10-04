#ifndef NATIVE_MINIMAP_H
#define NATIVE_MINIMAP_H

#include <macros.h>

struct PrimMem;
struct UIMap;

// Projection before HUD translation, widescreen scaling and the 3P offset.
void NativeMinimap_Project(const struct UIMap *map, double x, double z, double *mapX, double *mapY);
// Translate the entire HUD map together, retaining its 4:3 edge margin.
float NativeMinimap_GetAnchorOffsetX(const struct UIMap *map);
int NativeMinimap_DrawLive(struct PrimMem *primMem, u32 *ot, u32 colorID);
int NativeMinimap_DrawPreview(int levelID, int right, int bottom, int width, int height, struct PrimMem *primMem, u32 *ot, u32 colorID);
void NativeMinimap_InvalidateLive(void);
// Explicit enable prepares the disk cache; level loads prepare custom geometry.
void NativeMinimap_Prepare(void);
void NativeMinimap_PrepareLive(void);
// World-space midpoint of the road end for the selected hub transition.
int NativeMinimap_GetHubRoutePosition(int hubLevelID, int triggerID, s32 worldPosition[3]);
void NativeMinimap_ReleaseGpu(void);

#endif
