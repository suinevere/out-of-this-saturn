#include "checkpoints.h"

struct Checkpoint {
	const char    *word;
	unsigned short id;
	unsigned short part;
	unsigned short ordinal;
};

static const Checkpoint s_checkpoints[CHECKPOINT_COUNT] = {
	{ "LDKD", 0x15E, 0x3E82, 10 },

	{ "HTDC", 0x15F, 0x3E83, 20 },

	{ "CLLD", 0x160, 0x3E84, 30 },
	{ "LBKG", 0x164, 0x3E84, 31 },
	{ "XDDJ", 0x163, 0x3E84, 33 },
	{ "FXLC", 0x161, 0x3E84, 35 },
	{ "KRFK", 0x162, 0x3E84, 37 },
	{ "KLFB", 0x165, 0x3E84, 39 },
	{ "TTCT", 0x166, 0x3E84, 41 },
	{ "XJRT", 0x16D, 0x3E84, 45 },
	{ "HBHK", 0x16F, 0x3E84, 47 },

	{ "CKJL", 0x16A, 0x3E85, 50 },

	{ "LFCK", 0x16B, 0x3E86, 60 },
	{ "TFBB", 0x172, 0x3E86, 64 },
	{ "TXHF", 0x173, 0x3E86, 66 }
};

struct Chapter {
	unsigned short part;
	int            len;
};

static const Chapter s_chapters[] = {
	{ 0x3E82, 1 },
	{ 0x3E83, 1 },
	{ 0x3E84, 9 },
	{ 0x3E85, 1 },
	{ 0x3E86, 3 }
};

static const int s_chapterCount =
    (int)(sizeof(s_chapters) / sizeof(s_chapters[0]));

unsigned short checkpointStringId(int i)
{
	if (i < 0 || i >= CHECKPOINT_COUNT) {
		return 0;
	}
	return s_checkpoints[i].id;
}

int checkpointOfStringId(unsigned short id)
{
	for (int i = 0; i < CHECKPOINT_COUNT; ++i) {
		if (s_checkpoints[i].id == id) {
			return i;
		}
	}
	return -1;
}

unsigned short checkpointOrdinal(int i)
{
	if (i < 0 || i >= CHECKPOINT_COUNT) {
		return 0;
	}
	return s_checkpoints[i].ordinal;
}

int checkpointOfOrdinal(unsigned short ordinal)
{
	for (int i = 0; i < CHECKPOINT_COUNT; ++i) {
		if (s_checkpoints[i].ordinal == ordinal) {
			return i;
		}
	}
	return -1;
}

const char *checkpointWord(int i)
{
	if (i < 0 || i >= CHECKPOINT_COUNT) {
		return "";
	}
	return s_checkpoints[i].word;
}

int checkpointChapterCount(void)
{
	return s_chapterCount;
}

unsigned short checkpointChapterPart(int c)
{
	if (c < 0 || c >= s_chapterCount) {
		return 0;
	}
	return s_chapters[c].part;
}

int checkpointChapterFirst(int c)
{
	int first = 0;

	if (c < 0 || c >= s_chapterCount) {
		return CHECKPOINT_COUNT;
	}
	for (int k = 0; k < c; ++k) {
		first += s_chapters[k].len;
	}
	return first;
}

int checkpointChapterLen(int c)
{
	if (c < 0 || c >= s_chapterCount) {
		return 0;
	}
	return s_chapters[c].len;
}

int checkpointChapterOf(int i)
{
	int first = 0;

	if (i < 0 || i >= CHECKPOINT_COUNT) {
		return -1;
	}
	for (int c = 0; c < s_chapterCount; ++c) {
		if (i < first + s_chapters[c].len) {
			return c;
		}
		first += s_chapters[c].len;
	}
	return -1;
}
