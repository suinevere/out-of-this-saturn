#include <cstdio>
extern "C" {
#include "cdda_arm.h"
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

int main(void)
{
    const int settle = CDDA_ARM_SETTLE_FRAMES;

    CHECK_EQ(cdda_arm(0, 0, 999, settle), CDDA_ARM_WAIT);

    CHECK_EQ(cdda_arm(1, 1, 999, settle), CDDA_ARM_WAIT);
    CHECK_EQ(cdda_arm(1, 3, 999, settle), CDDA_ARM_WAIT);

    CHECK_EQ(cdda_arm(1, 0, 0, settle), CDDA_ARM_WAIT);
    CHECK_EQ(cdda_arm(1, 0, settle - 1, settle), CDDA_ARM_WAIT);

    CHECK_EQ(cdda_arm(1, 0, settle, settle), CDDA_ARM_NOW);
    CHECK_EQ(cdda_arm(1, 0, settle + 40, settle), CDDA_ARM_NOW);

    CHECK_EQ(cdda_arm(1, 0, 0, 0), CDDA_ARM_NOW);

    CHECK_EQ(cdda_loop_expired(0, 0, 5000, 4000), 0);

    CHECK_EQ(cdda_loop_expired(1, 1, 3500, 4000), 0);

    CHECK_EQ(cdda_loop_expired(1, 0, 3500, 4000), 0);

    CHECK_EQ(cdda_loop_expired(1, 0, 4000, 4000), 1);
    CHECK_EQ(cdda_loop_expired(1, 0, 4100, 4000), 1);

    CHECK_EQ(cdda_loop_expired(1, 0, 4100, 0), 0);

    CHECK_EQ(cdda_delay_for_pos(0), CDDA_DELAY_POS0_MS);
    CHECK_EQ(cdda_delay_for_pos(1), CDDA_DELAY_POS1_MS);
    CHECK_EQ(cdda_delay_for_pos(2), CDDA_DELAY_POS2_MS);
    CHECK_EQ(cdda_delay_for_pos(3), CDDA_DELAY_POS3_MS);

    CHECK_EQ(cdda_delay_for_pos(4), CDDA_DELAY_POS0_MS);
    CHECK_EQ(cdda_delay_for_pos(-1), CDDA_DELAY_POS0_MS);
    CHECK_EQ(cdda_delay_for_pos(255), CDDA_DELAY_POS0_MS);

    CHECK_EQ(cdda_delay_for_pos(0), cdda_delay_for_pos(3));
    CHECK_EQ(cdda_delay_for_pos(1), cdda_delay_for_pos(3));
    CHECK_EQ(cdda_delay_for_pos(2), cdda_delay_for_pos(3));
    CHECK_EQ(cdda_delay_for_pos(3) > 0, 1);
    CHECK_EQ(CDDA_DELAY_INTRO_MS, 0u);

    printf(g_fail ? "cdda_arm: %d FAILED\n" : "cdda_arm: all passed\n", g_fail);
    return g_fail ? 1 : 0;
}
