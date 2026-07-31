#include <srl.hpp>
#include "saturn_scsp.h"
#include "scsp_voice.h"

#define SCSP_SLOT_BASE   0x25B00000u
#define SCSP_COMMON      0x25B00400u
#define SCSP_SOUND_RAM   0x25A00000u

#define SCSP_REG(a)      (*(volatile uint16_t *)(a))
#define SCSP_SLOT(n, o)  SCSP_REG(SCSP_SLOT_BASE + ((uint32_t)(n) * 0x20u) + (o))

#define SCSP_HEAP_BASE   0x040000u
#define SCSP_HEAP_LIMIT  0x080000u

#define SCSP_CHANNELS    4

#define SCSP_SLOTS_PER_CHANNEL 2
#define SCSP_SLOT_FIRST        24
#define SCSP_SLOT_A(ch)        ((uint8_t)(SCSP_SLOT_FIRST + (ch) * SCSP_SLOTS_PER_CHANNEL))
#define SCSP_SLOT_B(ch)        ((uint8_t)(SCSP_SLOT_FIRST + (ch) * SCSP_SLOTS_PER_CHANNEL + 1))
#define SCSP_TOTAL_SLOTS       (SCSP_CHANNELS * SCSP_SLOTS_PER_CHANNEL)

static uint8_t g_slot[SCSP_CHANNELS] = { 0, 0, 0, 0 };

#define SCSP_AR   31
#define SCSP_D1R  0
#define SCSP_D2R  0
#define SCSP_DL   0
#define SCSP_RR   31

static ScspCache g_cache;
static uint32_t  g_active  = 0;
static uint32_t  g_uploads = 0;

static void slotKeyOff(uint8_t slot)
{
    const uint16_t w = SCSP_SLOT(slot, 0x00);

    SCSP_SLOT(slot, 0x00) = (uint16_t)((w & ~0x0800u) | 0x1000u);
    g_active &= ~(1u << slot);
}

static void slotKeyOn(uint8_t slot)
{
    const uint16_t w = SCSP_SLOT(slot, 0x00);

    SCSP_SLOT(slot, 0x00) = (uint16_t)(w | 0x0800u | 0x1000u);
    g_active |= (1u << slot);
}

static void uploadToSoundRam(uint32_t offset, const uint8_t *src, uint32_t bytes)
{
    volatile uint16_t *dst = (volatile uint16_t *)(SCSP_SOUND_RAM + offset);
    uint32_t           i;

    for (i = 0; i + 1u < bytes; i += 2u)
    {
        *dst++ = (uint16_t)(((uint16_t)src[i] << 8) | src[i + 1u]);
    }

    if (i < bytes)
    {
        *dst = (uint16_t)((uint16_t)src[i] << 8);
    }

    g_uploads++;
}

void sat_scsp_init(void)
{
    uint8_t n;

    scsp_cache_init(&g_cache, SCSP_HEAP_BASE, SCSP_HEAP_LIMIT);
    g_active  = 0;
    g_uploads = 0;

    SCSP_REG(SCSP_COMMON + 0x00) =
        (uint16_t)((SCSP_REG(SCSP_COMMON + 0x00) & ~0x000Fu) | 0x000Fu);

    for (n = SCSP_SLOT_FIRST; n < SCSP_SLOT_FIRST + SCSP_TOTAL_SLOTS; n++)
    {
        slotKeyOff(n);

        SCSP_SLOT(n, 0x08) = (uint16_t)(((uint16_t)SCSP_D2R << 11) |
                                        ((uint16_t)SCSP_D1R << 6)  |
                                        (uint16_t)SCSP_AR);
        SCSP_SLOT(n, 0x0A) = (uint16_t)(((uint16_t)SCSP_DL << 5) |
                                        (uint16_t)SCSP_RR);
        SCSP_SLOT(n, 0x0C) = 0x00FF;

        SCSP_SLOT(n, 0x0E) = 0;
        SCSP_SLOT(n, 0x12) = 0;

        SCSP_SLOT(n, 0x16) = (uint16_t)(7u << 13);
    }

    for (n = 0; n < SCSP_CHANNELS; n++)
    {
        g_slot[n] = SCSP_SLOT_A(n);
    }
}

void sat_scsp_shutdown(void)
{
    voice_stop_all();
}

void voice_stop(uint8_t channel)
{
    if (channel < SCSP_CHANNELS)
    {
        slotKeyOff(SCSP_SLOT_A(channel));
        slotKeyOff(SCSP_SLOT_B(channel));
    }
}

void voice_stop_all(void)
{
    uint8_t n;

    for (n = SCSP_SLOT_FIRST; n < SCSP_SLOT_FIRST + SCSP_TOTAL_SLOTS; n++)
    {
        slotKeyOff(n);
    }
}

void sat_scsp_set_master(uint8_t vol)
{
    if (vol > 0x0F)
    {
        vol = 0x0F;
    }

    SCSP_REG(SCSP_COMMON + 0x00) =
        (uint16_t)((SCSP_REG(SCSP_COMMON + 0x00) & ~0x000Fu) | (uint16_t)vol);
}

void voice_set_volume(uint8_t channel, uint8_t volume)
{
    uint8_t slot;

    if (channel >= SCSP_CHANNELS)
    {
        return;
    }

    slot = g_slot[channel];

    SCSP_SLOT(slot, 0x0C) =
        (uint16_t)((SCSP_SLOT(slot, 0x0C) & ~0x00FFu) |
                   scsp_voice_tl(volume));
}

void voice_flush_samples(void)
{
    voice_stop_all();
    scsp_cache_reset(&g_cache);
}

void voice_play(uint8_t channel, const uint8_t *data, uint16_t len,
                   uint16_t loopPos, uint16_t loopLen,
                   uint16_t freq, uint8_t volume)
{
    ScspVoicePoints points;
    ScspCacheResult result;
    uint32_t        offset = 0;
    uint8_t         previous;
    uint8_t         slot;

    if (channel >= SCSP_CHANNELS || data == 0)
    {
        return;
    }

    result = scsp_cache_acquire(&g_cache, data, len, loopLen, &offset);

    if (result == SCSP_CACHE_TOO_BIG)
    {
        return;
    }

    if (result == SCSP_CACHE_MISS_AFTER_RESET)
    {
        voice_stop_all();
    }

    if (result != SCSP_CACHE_HIT)
    {
        uploadToSoundRam(offset, data, scsp_voice_upload_bytes(len, loopLen));
    }

    if (!scsp_voice_points(offset, len, loopPos, loopLen, &points))
    {
        slotKeyOff(g_slot[channel]);
        return;
    }

    previous = g_slot[channel];
    slot     = (previous == SCSP_SLOT_A(channel)) ? SCSP_SLOT_B(channel)
                                                  : SCSP_SLOT_A(channel);

    SCSP_SLOT(slot, 0x00) =
        (uint16_t)((points.loop ? (1u << 5) : 0u) |
                   (1u << 4) |
                   ((points.sa >> 16) & 0x000Fu));
    SCSP_SLOT(slot, 0x02) = (uint16_t)(points.sa & 0xFFFFu);
    SCSP_SLOT(slot, 0x04) = points.lsa;
    SCSP_SLOT(slot, 0x06) = points.lea;
    SCSP_SLOT(slot, 0x10) = scsp_voice_pitch(freq);
    SCSP_SLOT(slot, 0x0C) = (uint16_t)((SCSP_SLOT(slot, 0x0C) & ~0x00FFu) |
                                       scsp_voice_tl(volume));

    slotKeyOn(slot);
    slotKeyOff(previous);

    g_slot[channel] = slot;
}

uint32_t sat_scsp_debug_active(void)     { return g_active; }
uint32_t sat_scsp_debug_heap_used(void)  { return scsp_cache_used_bytes(&g_cache); }
uint32_t sat_scsp_debug_cache_used(void) { return scsp_cache_used_entries(&g_cache); }

uint32_t sat_scsp_debug_uploads(void)
{
    const uint32_t n = g_uploads;

    g_uploads = 0;
    return n;
}
