#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b0b38-0x800b0b7c
void CS_BoxScene_InstanceSplitLines(void)
{
	s16 split = D233.VertSplitLine;
	struct Thread *t = P32_GET(struct Thread *, P32_GET(struct GameTracker *, sdata->gGT)->threadBuckets[GHOST].thread);

	while (t != NULL)
	{
		P32_GET(struct Instance *, t->inst)->vertSplit = split;
		t = P32_GET(struct Thread *, t->siblingThread);
	}
}
