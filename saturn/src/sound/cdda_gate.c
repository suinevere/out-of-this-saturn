#include "cdda_gate.h"

static int g_depth = 0;

int cdda_gate_enter(void)
{
    return (++g_depth == 1) ? 1 : 0;
}

int cdda_gate_exit(void)
{
    if (g_depth <= 0)
    {
        g_depth = 0;
        return 0;
    }

    return (--g_depth == 0) ? 1 : 0;
}

int cdda_gate_depth(void)
{
    return g_depth;
}

void cdda_gate_reset(void)
{
    g_depth = 0;
}
