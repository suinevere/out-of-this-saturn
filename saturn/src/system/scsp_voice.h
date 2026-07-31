#ifndef SCSP_VOICE_H
#define SCSP_VOICE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint16_t scsp_voice_pitch(uint32_t sampleRate);

uint8_t scsp_voice_tl(uint8_t volume);

typedef struct
{
    uint32_t sa;
    uint16_t lsa;
    uint16_t lea;
    uint8_t  loop;
} ScspVoicePoints;

uint32_t scsp_voice_upload_bytes(uint16_t len, uint16_t loopLen);

int scsp_voice_points(uint32_t base, uint16_t len, uint16_t loopPos,
                      uint16_t loopLen, ScspVoicePoints *out);

#define SCSP_CACHE_ENTRIES 32

typedef struct
{
    const uint8_t *key;
    uint16_t       len;
    uint16_t       loopLen;
    uint32_t       offset;
} ScspCacheEntry;

typedef struct
{
    uint32_t       base;
    uint32_t       limit;
    uint32_t       next;
    uint32_t       count;
    ScspCacheEntry entries[SCSP_CACHE_ENTRIES];
} ScspCache;

typedef enum
{
    SCSP_CACHE_HIT              = 0,
    SCSP_CACHE_MISS             = 1,
    SCSP_CACHE_MISS_AFTER_RESET = 2,
    SCSP_CACHE_TOO_BIG          = 3
} ScspCacheResult;

void scsp_cache_init(ScspCache *cache, uint32_t base, uint32_t limit);
void scsp_cache_reset(ScspCache *cache);

ScspCacheResult scsp_cache_acquire(ScspCache *cache, const uint8_t *data,
                                   uint16_t len, uint16_t loopLen,
                                   uint32_t *offsetOut);

uint32_t scsp_cache_used_bytes(const ScspCache *cache);
uint32_t scsp_cache_used_entries(const ScspCache *cache);

#ifdef __cplusplus
}
#endif
#endif
