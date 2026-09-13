#include "cdda_arm.h"

cdda_arm_action cdda_arm(int pending, int depth, int idle_frames,
                         int settle_frames)
{
    if (!pending || depth > 0)
    {
        return CDDA_ARM_WAIT;
    }

    return (idle_frames >= settle_frames) ? CDDA_ARM_NOW : CDDA_ARM_WAIT;
}

uint32_t cdda_delay_for_pos(int pos)
{
    switch (pos)
    {
    case 1:
        return CDDA_DELAY_POS1_MS;
    case 2:
        return CDDA_DELAY_POS2_MS;
    case 3:
        return CDDA_DELAY_POS3_MS;
    default:
        return CDDA_DELAY_POS0_MS;
    }
}

int cdda_loop_expired(int tail, int playing, uint32_t fad, uint32_t end)
{
    if (!tail || playing || end == 0)
    {
        return 0;
    }

    return (fad >= end) ? 1 : 0;
}
