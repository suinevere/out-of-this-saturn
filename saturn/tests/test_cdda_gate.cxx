#include <cstdio>
extern "C" {
#include "cdda_gate.h"
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
    cdda_gate_reset();
    CHECK_EQ(cdda_gate_depth(), 0);

    CHECK_EQ(cdda_gate_enter(), 1);
    CHECK_EQ(cdda_gate_enter(), 0);
    CHECK_EQ(cdda_gate_enter(), 0);
    CHECK_EQ(cdda_gate_depth(), 3);
    CHECK_EQ(cdda_gate_exit(), 0);
    CHECK_EQ(cdda_gate_exit(), 0);
    CHECK_EQ(cdda_gate_exit(), 1);
    CHECK_EQ(cdda_gate_depth(), 0);

    CHECK_EQ(cdda_gate_exit(), 0);
    CHECK_EQ(cdda_gate_depth(), 0);

    cdda_gate_enter();
    cdda_gate_enter();
    cdda_gate_reset();
    CHECK_EQ(cdda_gate_depth(), 0);
    CHECK_EQ(cdda_gate_enter(), 1);
    cdda_gate_reset();

    printf(g_fail ? "cdda_gate: %d FAILED\n" : "cdda_gate: all passed\n", g_fail);
    return g_fail ? 1 : 0;
}
