#include <srl.hpp>
#include "saturn_audio.h"
#include "saturn_platform.h"
#include "saturn_scsp.h"

static bool g_running = false;

#define MAX_TIMERS 4

struct TimerSlot
{
    bool             used;
    uint32_t         delay;
    uint32_t         due;
    SatTimerCallback callback;
    void            *param;
};

static TimerSlot g_timers[MAX_TIMERS];

extern "C" uint32_t sat_audio_sample_rate(void)
{
    return 44100;
}

extern "C" void sat_audio_start(SatAudioCallback callback, void *param)
{
    (void)callback;
    (void)param;

    if (g_running)
    {
        return;
    }

    sat_scsp_init();

    g_running = true;
}

extern "C" void sat_audio_stop(void)
{
    if (!g_running)
    {
        return;
    }

    sat_scsp_shutdown();

    g_running = false;
}

extern "C" void timers_pump(void)
{
    const uint32_t now = clock_ms();

    for (int32_t i = 0; i < MAX_TIMERS; i++)
    {
        TimerSlot *slot = &g_timers[i];

        if (slot->used && now >= slot->due)
        {
            const uint32_t next = slot->callback(slot->delay, slot->param);

            if (next == 0)
            {
                slot->used = false;
            }
            else
            {
                slot->delay = next;
                slot->due   = now + next;
            }
        }
    }
}

extern "C" int sat_timer_add(uint32_t delay, SatTimerCallback callback, void *param)
{
    if (callback == nullptr)
    {
        return 0;
    }

    for (int32_t i = 0; i < MAX_TIMERS; i++)
    {
        if (!g_timers[i].used)
        {
            g_timers[i].used     = true;
            g_timers[i].delay    = delay;
            g_timers[i].due      = clock_ms() + delay;
            g_timers[i].callback = callback;
            g_timers[i].param    = param;
            return i + 1;
        }
    }

    return 0;
}

extern "C" void sat_timer_remove(int timerId)
{
    const int32_t index = timerId - 1;

    if (index >= 0 && index < MAX_TIMERS)
    {
        g_timers[index].used = false;
    }
}
