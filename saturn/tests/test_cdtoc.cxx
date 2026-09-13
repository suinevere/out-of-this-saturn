#include <cstdio>
#include <cstdint>
extern "C" {
#include "cdtoc.h"
}

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

static uint32_t entry(uint32_t ctrl, uint32_t fad)
{
    return (ctrl << 28) | (fad & 0x00FFFFFFu);
}

int main(void)
{
    uint32_t toc[CDTOC_WORDS];

    for (int i = 0; i < CDTOC_WORDS; i++) {
        toc[i] = 0xFFFFFFFFu;
    }

    toc[0] = entry(0x4, 150);
    toc[1] = entry(0x0, 1000);
    toc[2] = entry(0x0, 2000);
    toc[3] = entry(0x0, 3000);

    toc[CDTOC_FIRST_WORD]   = (0x4u << 28) | (1u << 16);
    toc[CDTOC_LAST_WORD]    = (0x0u << 28) | (4u << 16);
    toc[CDTOC_LEADOUT_WORD] = entry(0x0, 4000);

    CHECK_EQ(cdtoc_is_audio(toc, 1), 0);
    CHECK_EQ(cdtoc_is_audio(toc, 2), 1);
    CHECK_EQ(cdtoc_is_audio(toc, 4), 1);
    CHECK_EQ(cdtoc_is_audio(toc, 5), 0);

    CHECK_EQ(cdtoc_track_start(toc, 2), 1000);
    CHECK_EQ(cdtoc_track_start(toc, 4), 3000);
    CHECK_EQ(cdtoc_track_end(toc, 2), 2000);
    CHECK_EQ(cdtoc_track_end(toc, 3), 3000);
    CHECK_EQ(cdtoc_track_end(toc, 4), 4000);

    CHECK_EQ(cdtoc_max_audio_track(toc), 4);

    CHECK_EQ(cdtoc_track_start(toc, 0), 0);
    CHECK_EQ(cdtoc_track_start(toc, 100), 0);
    CHECK_EQ(cdtoc_is_audio(toc, 0), 0);

    printf(g_fail ? "cdtoc: %d FAILED\n" : "cdtoc: all passed\n", g_fail);
    return g_fail ? 1 : 0;
}
