#include "cdda_classify.h"

cdda_action cdda_classify(int was_playing, int loop, int observed,
                          uint32_t fad, uint32_t start, uint32_t end)
{
    if (!was_playing && loop == 0 && observed &&
        start != 0 && end != 0 && fad >= end)
    {
        return CDDA_FORGET;
    }

    if (was_playing && start != 0 && end != 0 &&
        fad >= start && fad < end)
    {
        return (loop == 0) ? CDDA_RESUME : CDDA_RESUME_LOOP;
    }

    return CDDA_RESTART;
}
