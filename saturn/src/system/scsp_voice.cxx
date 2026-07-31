#include "scsp_voice.h"

#define SCSP_BASE_RATE 44100

static int32_t sglLog(uint32_t n)
{
    int32_t bits = 0;

    while (n != 0)
    {
        bits++;
        n >>= 1;
    }

    return bits;
}

uint16_t scsp_voice_pitch(uint32_t sampleRate)
{
    int32_t octave;
    int32_t shiftFreq;
    int32_t fns;

    if (sampleRate == 0)
    {
        return 0;
    }

    octave    = sglLog((uint32_t)SCSP_BASE_RATE / (sampleRate + 1u));
    shiftFreq = (int32_t)SCSP_BASE_RATE >> octave;
    fns       = (((int32_t)sampleRate - shiftFreq) << 10) / shiftFreq;

    if (fns > 1023 && octave > 0)
    {
        octave--;
        shiftFreq = (int32_t)SCSP_BASE_RATE >> octave;
        fns       = (((int32_t)sampleRate - shiftFreq) << 10) / shiftFreq;
    }

    if (fns < 0)
    {
        fns = 0;
    }

    if (fns > 1023)
    {
        fns = 1023;
    }

    return (uint16_t)((((-octave) & 0x0F) << 11) | (fns & 0x03FF));
}

static const uint8_t g_tlTable[64] = {
    255,  96,  80,  71,  64,  59,  54,  51,
     48,  45,  43,  40,  38,  37,  35,  33,
     32,  30,  29,  28,  27,  25,  24,  23,
     22,  21,  20,  20,  19,  18,  17,  16,
     16,  15,  14,  14,  13,  12,  12,  11,
     11,  10,   9,   9,   8,   8,   7,   7,
      6,   6,   5,   5,   4,   4,   4,   3,
      3,   2,   2,   2,   1,   1,   0,   0
};

uint8_t scsp_voice_tl(uint8_t volume)
{
    if (volume > 63)
    {
        volume = 63;
    }

    return g_tlTable[volume];
}

uint32_t scsp_voice_upload_bytes(uint16_t len, uint16_t loopLen)
{
    uint32_t total = (uint32_t)len + (uint32_t)loopLen;

    return (total + 1u) & ~1u;
}

int scsp_voice_points(uint32_t base, uint16_t len, uint16_t loopPos,
                      uint16_t loopLen, ScspVoicePoints *out)
{
    const uint32_t total = (uint32_t)len + (uint32_t)loopLen;

    uint32_t lsa;
    uint32_t lea;
    uint8_t  loop;

    if (out == 0 || total == 0)
    {
        return 0;
    }

    if (loopLen != 0)
    {
        lsa  = loopPos;
        lea  = (uint32_t)loopPos + (uint32_t)loopLen - 1u;
        loop = 1;
    }
    else
    {
        lsa  = 0;
        lea  = (uint32_t)len - 1u;
        loop = 0;
    }

    if (lea >= total)
    {
        return 0;
    }

    if (lea > 0xFFFFu || lsa > 0xFFFFu)
    {
        return 0;
    }

    out->sa   = base;
    out->lsa  = (uint16_t)lsa;
    out->lea  = (uint16_t)lea;
    out->loop = loop;

    return 1;
}

void scsp_cache_init(ScspCache *cache, uint32_t base, uint32_t limit)
{
    if (cache == 0)
    {
        return;
    }

    cache->base  = base;
    cache->limit = limit;
    cache->next  = base;
    cache->count = 0;
}

void scsp_cache_reset(ScspCache *cache)
{
    if (cache == 0)
    {
        return;
    }

    cache->next  = cache->base;
    cache->count = 0;
}

ScspCacheResult scsp_cache_acquire(ScspCache *cache, const uint8_t *data,
                                   uint16_t len, uint16_t loopLen,
                                   uint32_t *offsetOut)
{
    const uint32_t need = scsp_voice_upload_bytes(len, loopLen);

    uint32_t i;
    int      didReset = 0;

    if (cache == 0 || offsetOut == 0 || need == 0)
    {
        return SCSP_CACHE_TOO_BIG;
    }

    if (need > cache->limit - cache->base)
    {
        return SCSP_CACHE_TOO_BIG;
    }

    for (i = 0; i < cache->count; i++)
    {
        const ScspCacheEntry *e = &cache->entries[i];

        if (e->key == data && e->len == len && e->loopLen == loopLen)
        {
            *offsetOut = e->offset;
            return SCSP_CACHE_HIT;
        }
    }

    if (cache->next + need > cache->limit ||
        cache->count >= SCSP_CACHE_ENTRIES)
    {
        scsp_cache_reset(cache);
        didReset = 1;
    }

    cache->entries[cache->count].key     = data;
    cache->entries[cache->count].len     = len;
    cache->entries[cache->count].loopLen = loopLen;
    cache->entries[cache->count].offset  = cache->next;
    cache->count++;

    *offsetOut  = cache->next;
    cache->next += need;

    return didReset ? SCSP_CACHE_MISS_AFTER_RESET : SCSP_CACHE_MISS;
}

uint32_t scsp_cache_used_bytes(const ScspCache *cache)
{
    return (cache == 0) ? 0 : (cache->next - cache->base);
}

uint32_t scsp_cache_used_entries(const ScspCache *cache)
{
    return (cache == 0) ? 0 : cache->count;
}
