#include <cstdio>
extern "C" {
#include "cdda_want.h"
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
    CHECK_EQ(cdda_want(2, 2, 2), CDDA_WANT_KEEP);
    CHECK_EQ(cdda_want(0, 0, 0), CDDA_WANT_KEEP);
    CHECK_EQ(cdda_want(25, 25, 25), CDDA_WANT_KEEP);

    CHECK_EQ(cdda_want(25, 25, 0), CDDA_WANT_KEEP);
    CHECK_EQ(cdda_want(2, 2, 0), CDDA_WANT_KEEP);

    CHECK_EQ(cdda_want(0, 25, 25), CDDA_WANT_STOP);
    CHECK_EQ(cdda_want(0, 2, 2), CDDA_WANT_STOP);

    CHECK_EQ(cdda_want(0, 25, 0), CDDA_WANT_CLEAR);

    CHECK_EQ(cdda_want(2, 25, 25), CDDA_WANT_PLAY);
    CHECK_EQ(cdda_want(2, 25, 0), CDDA_WANT_PLAY);
    CHECK_EQ(cdda_want(5, 2, 2), CDDA_WANT_PLAY);

    CHECK_EQ(cdda_want(25, 0, 0), CDDA_WANT_PLAY);
    CHECK_EQ(cdda_want(2, 0, 0), CDDA_WANT_PLAY);

    CHECK_EQ(cdda_want(1, 0, 0), CDDA_WANT_PLAY);

    printf(g_fail ? "cdda_want: %d FAILED\n" : "cdda_want: all passed\n", g_fail);
    return g_fail ? 1 : 0;
}
