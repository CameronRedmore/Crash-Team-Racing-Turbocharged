#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80031734-0x80031744.
void LIST_Clear(struct LinkedList *L)
{
	P32_SET(L->first, 0);
	P32_SET(L->last, 0);
	L->count = 0;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80031744-0x80031788.
void LIST_AddFront(struct LinkedList *L, struct Item *I)
{
	if (I == 0)
	{
		return;
	}

	P32_SET(I->prev, 0);

	struct Item *oldFirst = P32_GET(struct Item *, L->first);
	P32_SET(I->next, oldFirst);

	if (oldFirst != 0)
	{
		P32_SET(P32_GET(struct Item *, L->first)->prev, I);
	}
	else
	{
		P32_SET(L->last, I);
	}

	P32_SET(L->first, I);
	L->count = L->count + 1;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x80031788-0x800317cc.
void LIST_AddBack(struct LinkedList *L, struct Item *I)
{
	if (I == 0)
	{
		return;
	}

	P32_SET(I->next, 0);

	struct Item *oldLast = P32_GET(struct Item *, L->last);
	P32_SET(I->prev, oldLast);

	if (oldLast != 0)
	{
		P32_SET(P32_GET(struct Item *, L->last)->next, I);
	}
	else
	{
		P32_SET(L->first, I);
	}

	P32_SET(L->last, I);
	L->count = L->count + 1;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800317cc-0x800317d8.
void *LIST_GetNextItem(struct Item *I)
{
	return P32_GET(struct Item *, I->next);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800317d8-0x800317e4.
void *LIST_GetFirstItem(struct LinkedList *L)
{
	return P32_GET(struct Item *, L->first);
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800317e4-0x8003186c.
struct Item *LIST_RemoveMember(struct LinkedList *L, struct Item *I)
{
	if (I == 0)
	{
		return 0;
	}

	if (P32_GET(struct Item *, L->first) != 0)
	{
		if (P32_GET(struct Item *, I->prev) != 0)
		{
			P32_SET(P32_GET(struct Item *, I->prev)->next, P32_GET(struct Item *, I->next));
		}
		else
		{
			P32_SET(L->first, P32_GET(struct Item *, I->next));
		}

		if (P32_GET(struct Item *, I->next) != 0)
		{
			P32_SET(P32_GET(struct Item *, I->next)->prev, P32_GET(struct Item *, I->prev));
		}
		else
		{
			P32_SET(L->last, P32_GET(struct Item *, I->prev));
		}

		L->count = L->count - 1;
	}

	P32_SET(I->next, 0);
	P32_SET(I->prev, 0);

	return I;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003186c-0x800318ec.
struct Item *LIST_RemoveFront(struct LinkedList *L)
{
	struct Item *I = P32_GET(struct Item *, L->first);

	if (I == 0)
	{
		return 0;
	}

	if (P32_GET(struct Item *, I->prev) != 0)
	{
		P32_SET(P32_GET(struct Item *, I->prev)->next, P32_GET(struct Item *, I->next));
	}
	else
	{
		P32_SET(L->first, P32_GET(struct Item *, I->next));
	}

	if (P32_GET(struct Item *, I->next) != 0)
	{
		P32_SET(P32_GET(struct Item *, I->next)->prev, P32_GET(struct Item *, I->prev));
	}
	else
	{
		P32_SET(L->last, P32_GET(struct Item *, I->prev));
	}

	L->count = L->count - 1;
	P32_SET(I->next, 0);
	P32_SET(I->prev, 0);

	return I;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800318ec-0x8003197c.
struct Item *LIST_RemoveBack(struct LinkedList *L)
{
	struct Item *I = P32_GET(struct Item *, L->last);

	if (I == 0)
	{
		return 0;
	}

	if (P32_GET(struct Item *, L->first) != 0)
	{
		if (P32_GET(struct Item *, I->prev) != 0)
		{
			P32_SET(P32_GET(struct Item *, I->prev)->next, P32_GET(struct Item *, I->next));
		}
		else
		{
			P32_SET(L->first, P32_GET(struct Item *, I->next));
		}

		if (P32_GET(struct Item *, I->next) != 0)
		{
			P32_SET(P32_GET(struct Item *, I->next)->prev, P32_GET(struct Item *, I->prev));
		}
		else
		{
			P32_SET(L->last, P32_GET(struct Item *, I->prev));
		}

		L->count = L->count - 1;
	}

	P32_SET(I->next, 0);
	P32_SET(I->prev, 0);

	return I;
}

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x8003197c-0x800319e8.
void LIST_Init(struct LinkedList *L, struct Item *item, int itemSize, int numItems)
{
	while (numItems > 0)
	{
		LIST_AddBack(L, item);

		numItems--;
		item = (struct Item *)((s32)item + itemSize);
	}
}
