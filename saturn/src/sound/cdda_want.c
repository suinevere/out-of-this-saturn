#include "cdda_want.h"

cdda_want_action cdda_want(int wanted, int last_wanted, int playing)
{
    if (wanted == last_wanted)
    {
        return CDDA_WANT_KEEP;
    }

    if (wanted != 0)
    {
        return CDDA_WANT_PLAY;
    }

    return (playing != 0) ? CDDA_WANT_STOP : CDDA_WANT_CLEAR;
}
