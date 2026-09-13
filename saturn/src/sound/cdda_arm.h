#ifndef CDDA_ARM_H
#define CDDA_ARM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CDDA_ARM_SETTLE_FRAMES 2

#define CDDA_DELAY_POS0_MS 1750u
#define CDDA_DELAY_POS1_MS 1750u
#define CDDA_DELAY_POS2_MS 1750u
#define CDDA_DELAY_POS3_MS 1750u

#define CDDA_DELAY_INTRO_MS 0u

#define CDDA_GAP_NONE 0xFFFFFFFFu

uint32_t cdda_delay_for_pos(int pos);

#define CDDA_FALLBACK_MS 4000u

typedef enum
{
    CDDA_ARM_WAIT = 0,
    CDDA_ARM_NOW  = 1
} cdda_arm_action;

cdda_arm_action cdda_arm(int pending, int depth, int idle_frames,
                         int settle_frames);

int cdda_loop_expired(int tail, int playing, uint32_t fad, uint32_t end);

#ifdef __cplusplus
}
#endif

#endif
