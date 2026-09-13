#include "cue_index.h"
#include "part_cues.h"
#include "parts.h"

#include <stdio.h>

static int g_fail = 0;

#define CHECK_EQ(actual, expected)                                            \
    do {                                                                      \
        long long a_ = (long long)(actual);                                   \
        long long e_ = (long long)(expected);                                 \
        if (a_ != e_) {                                                       \
            g_fail++;                                                         \
            printf("FAIL %s:%d  %s\n  actual   = %lld\n  expected = %lld\n",  \
                   __FILE__, __LINE__, #actual, a_, e_);                      \
        }                                                                     \
    } while (0)

static unsigned char s_segment[0x10000 + 16];
static const unsigned char *s_base = s_segment + 3;

static int rows_of(unsigned short partId)
{
    int n = 0;

#define COUNT_ROW(part_, address_, cue_, loops_, pos_) if ((part_) == partId) { n++; }
    PART_CUES_LIST(COUNT_ROW)
#undef COUNT_ROW

    return n;
}

static void check_part(unsigned short partId)
{
    cueIndexSetPart(partId, s_base);
    CHECK_EQ(cueIndexCount(), rows_of(partId));

#define CHECK_ROW(part_, address_, cue_, loops_, pos_)                        \
    if ((part_) == partId) {                                                  \
        const unsigned char *at = s_base + (address_);                        \
        int gotCue = -1;                                                      \
        int gotLoops = -1;                                                    \
        int gotPos = -1;                                                      \
        CHECK_EQ(g_cueFilter[((uintptr_t)at) & 0xFFu] != 0, 1);               \
        CHECK_EQ(cueIndexAt(at, &gotCue, &gotLoops, &gotPos), 1);             \
        CHECK_EQ(gotCue, (cue_));                                             \
        CHECK_EQ(gotLoops, (loops_));                                         \
        CHECK_EQ(gotPos, (pos_));                                             \
    }
    PART_CUES_LIST(CHECK_ROW)
#undef CHECK_ROW
}

int main(void)
{
    int cue = -1;
    int loops = -1;
    int pos = -1;
    int i;
    int set;

    CHECK_EQ(PART_CUES_COUNT > 0, 1);

    check_part(GAME_PART2);
    check_part(GAME_PART3);
    check_part(GAME_PART4);
    check_part(GAME_PART5);
    check_part(GAME_PART6);
    check_part(GAME_PART7);
    check_part(GAME_PART8);

    cueIndexSetPart(GAME_PART3, s_base);
    CHECK_EQ(cueIndexAt(s_base + 0x0003, &cue, &loops, &pos), 1);
    CHECK_EQ(cue, 2);
    CHECK_EQ(loops, 1);
    CHECK_EQ(cueIndexAt(s_base + 0x00D6, &cue, &loops, &pos), 1);
    CHECK_EQ(cue, 0);
    CHECK_EQ(loops, 0);

    CHECK_EQ(cueIndexAt(s_base + 0x0004, &cue, &loops, &pos), 0);

    cueIndexSetPart(GAME_PART1, s_base);
    CHECK_EQ(cueIndexCount(), 0);
    set = 0;
    for (i = 0; i < CUE_INDEX_FILTER_SIZE; i++) {
        set += (g_cueFilter[i] != 0);
    }
    CHECK_EQ(set, 0);
    CHECK_EQ(cueIndexAt(s_base + 0x0003, &cue, &loops, &pos), 0);

    cueIndexSetPart(GAME_PART3, 0);
    CHECK_EQ(cueIndexCount(), 0);
    CHECK_EQ(cueIndexAt(s_base + 0x0003, &cue, &loops, &pos), 0);

    cueIndexSetPart(GAME_PART3, s_segment + 4);
    CHECK_EQ(cueIndexAt(s_base + 0x0003, &cue, &loops, &pos), 0);
    CHECK_EQ(cueIndexAt(s_segment + 4 + 0x0003, &cue, &loops, &pos), 1);
    CHECK_EQ(cue, 2);

    printf(g_fail ? "cue_index: %d FAILED\n" : "cue_index: all passed\n",
           g_fail);
    return g_fail ? 1 : 0;
}
