#include "cue_index.h"
#include "part_cues.h"
#include "parts.h"

unsigned char g_cueFilter[CUE_INDEX_FILTER_SIZE];

struct CueRow
{
	const unsigned char *pc;
	short                cue;
	short                loops;
	short                pos;
};

static CueRow s_rows[PART_CUES_COUNT];
static int    s_count = 0;

void cueIndexSetPart(unsigned short partId, const unsigned char *segBytecode)
{
	int i;

	for (i = 0; i < CUE_INDEX_FILTER_SIZE; i++)
	{
		g_cueFilter[i] = 0;
	}

	s_count = 0;

	if (segBytecode == 0)
	{
		return;
	}

#define CUE_ROW(part_, address_, cue_, loops_, pos_)                          \
	if ((part_) == partId)                                                    \
	{                                                                         \
		s_rows[s_count].pc = segBytecode + (address_);                        \
		s_rows[s_count].cue = (cue_);                                         \
		s_rows[s_count].loops = (loops_);                                     \
		s_rows[s_count].pos = (pos_);                                         \
		s_count++;                                                            \
	}

	PART_CUES_LIST(CUE_ROW)

#undef CUE_ROW

	for (i = 1; i < s_count; i++)
	{
		CueRow row = s_rows[i];
		int    key = (int)(((uintptr_t)row.pc) & 0xFFu);
		int    j = i - 1;

		while (j >= 0 && (int)(((uintptr_t)s_rows[j].pc) & 0xFFu) > key)
		{
			s_rows[j + 1] = s_rows[j];
			j--;
		}

		s_rows[j + 1] = row;
	}

	for (i = s_count - 1; i >= 0; i--)
	{
		g_cueFilter[((uintptr_t)s_rows[i].pc) & 0xFFu] = (unsigned char)(i + 1);
	}
}

int cueIndexAt(const unsigned char *pc, int *cue, int *loops, int *pos)
{
	const unsigned int byte = ((uintptr_t)pc) & 0xFFu;
	int i = (int)g_cueFilter[byte];

	if (i == 0)
	{
		return 0;
	}

	for (i--; i < s_count; i++)
	{
		if ((((uintptr_t)s_rows[i].pc) & 0xFFu) != byte)
		{
			break;
		}

		if (s_rows[i].pc == pc)
		{
			*cue = s_rows[i].cue;
			*loops = s_rows[i].loops;
			*pos = s_rows[i].pos;
			return 1;
		}
	}

	return 0;
}

int cueIndexCount(void)
{
	return s_count;
}
