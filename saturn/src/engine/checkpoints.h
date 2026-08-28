#ifndef CHECKPOINTS_H
#define CHECKPOINTS_H

enum {
	CHECKPOINT_COUNT        = 15,
	CHECKPOINT_STRING_FIRST = 0x15E,
	CHECKPOINT_STRING_LAST  = 0x174
};

unsigned short checkpointStringId(int i);

int checkpointOfStringId(unsigned short id);

unsigned short checkpointOrdinal(int i);

int checkpointOfOrdinal(unsigned short ordinal);

const char *checkpointWord(int i);

int checkpointChapterCount(void);

unsigned short checkpointChapterPart(int c);

int checkpointChapterFirst(int c);

int checkpointChapterLen(int c);

int checkpointChapterOf(int i);

#endif
