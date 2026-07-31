#ifndef SATURN_AUDIO_H
#define SATURN_AUDIO_H

#include <stdint.h>
#include "timers.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*SatAudioCallback)(void *param, uint8_t *stream, int len);
typedef uint32_t (*SatTimerCallback)(uint32_t delay, void *param);

uint32_t sat_audio_sample_rate(void);

void sat_audio_start(SatAudioCallback callback, void *param);

void sat_audio_stop(void);

int  sat_timer_add(uint32_t delay, SatTimerCallback callback, void *param);
void sat_timer_remove(int timerId);

#ifdef __cplusplus
}
#endif
#endif
