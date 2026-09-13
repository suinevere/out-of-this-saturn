#include "part_music.h"
#include "checkpoints.h"
#include "opening.h"
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

int main(void)
{
    CHECK_EQ(partMusicCue(GAME_PART1), 0);

#if OPENING_USE_MODULE
    CHECK_EQ(partMusicCue(GAME_PART2), 0);
#else
    CHECK_EQ(partMusicCue(GAME_PART2), 25);
#endif
    CHECK_EQ(partMusicCue(GAME_PART3), 2);
    CHECK_EQ(partMusicCue(GAME_PART4), 5);
    CHECK_EQ(partMusicCue(GAME_PART5), 13);
    CHECK_EQ(partMusicCue(GAME_PART6), 21);
    CHECK_EQ(partMusicCue(GAME_PART7), 19);
    CHECK_EQ(partMusicCue(GAME_PART8), 24);
    CHECK_EQ(partMusicCue(GAME_PART9), 0);
    CHECK_EQ(partMusicCue(GAME_PART10), 0);

    CHECK_EQ(partMusicLoops(GAME_PART2), 0);
    CHECK_EQ(partMusicLoops(GAME_PART3), 1);
    CHECK_EQ(partMusicLoops(GAME_PART5), 1);
    CHECK_EQ(partMusicLoops(GAME_PART1), 0);

    CHECK_EQ(partMusicCue(0), 0);
    CHECK_EQ(partMusicCue(0x3E7F), 0);
    CHECK_EQ(partMusicCue(0x3E8A), 0);
    CHECK_EQ(partMusicCue(0xFFFF), 0);

    for (int c = 0; c < checkpointChapterCount(); c++) {
        unsigned short part = checkpointChapterPart(c);
        if (partMusicCue(part) == 0) {
            g_fail++;
            printf("FAIL chapter %d (part 0x%04X) has checkpoints but no cue\n",
                   c, part);
        }
    }

    CHECK_EQ(partMusicCovers(GAME_PART1), 1);
    CHECK_EQ(partMusicCovers(GAME_PART2), 1);
    CHECK_EQ(partMusicCovers(GAME_PART6), 1);
    CHECK_EQ(partMusicCovers(GAME_PART9), 1);
    CHECK_EQ(partMusicCovers(GAME_PART10), 1);

    CHECK_EQ(partMusicCovers(GAME_PART3), 0);
    CHECK_EQ(partMusicCovers(GAME_PART4), 0);
    CHECK_EQ(partMusicCovers(GAME_PART5), 0);
    CHECK_EQ(partMusicCovers(GAME_PART7), 0);
    CHECK_EQ(partMusicCovers(GAME_PART8), 0);

    CHECK_EQ(partMusicCovers(GAME_PART_FIRST - 1), 0);
    CHECK_EQ(partMusicCovers(GAME_PART_LAST + 1), 0);
    CHECK_EQ(partMusicCovers(0), 0);

    {
        int loops = -1;
#if OPENING_USE_MODULE
        CHECK_EQ(partMusicModuleCue(GAME_PART2, 0x07, &loops), 0);
        CHECK_EQ(loops, 0);
#else
        CHECK_EQ(partMusicModuleCue(GAME_PART2, 0x07, &loops), 25);
        CHECK_EQ(loops, 0);
#endif
        CHECK_EQ(partMusicModuleCue(GAME_PART8, 0x8A, &loops), 24);
        CHECK_EQ(loops, 1);
        CHECK_EQ(partMusicModuleCue(GAME_PART3, 0x07, &loops), 0);
        CHECK_EQ(partMusicModuleCue(GAME_PART2, 0x08, &loops), 0);
    }

    CHECK_EQ(partMusicResumeSkipMs(2), 4000u);
    CHECK_EQ(partMusicResumeSkipMs(partMusicCue(GAME_PART3)), 4000u);

    for (unsigned short cue = 3; cue <= 25; cue++) {
        if (partMusicResumeSkipMs(cue) != 0u) {
            g_fail++;
            printf("FAIL cue %u has a resume skip and should not\n", cue);
        }
    }
    CHECK_EQ(partMusicResumeSkipMs(0), 0u);
    CHECK_EQ(partMusicResumeSkipMs(1), 0u);

    printf(g_fail ? "part_music: %d FAILED\n" : "part_music: all passed\n", g_fail);
    return g_fail ? 1 : 0;
}
