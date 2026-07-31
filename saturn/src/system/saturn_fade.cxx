#include <srl.hpp>
#include "saturn_fade.h"
#include "saturn_platform.h"
#include "saturn_scsp.h"

#define FADE_OFFSET_FLOOR 255

#define FADE_MVOL_MAX 15

static int g_level = FADE_LIT;

static int g_audioFollows = 1;

extern "C" void sat_fade_init(void)
{
    slColOffsetAUse(NBG0ON | SPRON);
    fade_set(FADE_LIT);
}

extern "C" void fade_set(int level)
{
    if (level < FADE_DARK)
    {
        level = FADE_DARK;
    }
    else if (level > FADE_LIT)
    {
        level = FADE_LIT;
    }

    g_level = level;

    const int16_t offset =
        (int16_t)(-((FADE_OFFSET_FLOOR * (FADE_LIT - level)) / FADE_LIT));

    slColOffsetA(offset, offset, offset);
    sat_scsp_set_master((uint8_t)(g_audioFollows
                                      ? (level * FADE_MVOL_MAX) / FADE_LIT
                                      : FADE_MVOL_MAX));
}

extern "C" void fade_audio_follow(int on)
{
    g_audioFollows = on ? 1 : 0;
    fade_set(g_level);
}

extern "C" int fade_level(void)
{
    return g_level;
}

extern "C" void fade_ramp(int target, int frames)
{
    if (frames <= 0)
    {
        fade_set(target);
        sat_video_sync();
        return;
    }

    const int start = g_level;

    for (int i = 1; i <= frames; i++)
    {
        fade_set(start + ((target - start) * i) / frames);
        sat_video_sync();
    }
}
