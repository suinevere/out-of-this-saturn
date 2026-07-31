#ifndef VOICES_H
#define VOICES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void voice_play(uint8_t channel, const uint8_t *data, uint16_t len,
                uint16_t loopPos, uint16_t loopLen,
                uint16_t freq, uint8_t volume);
void voice_stop(uint8_t channel);
void voice_set_volume(uint8_t channel, uint8_t volume);
void voice_stop_all(void);
void voice_flush_samples(void);

#ifdef __cplusplus
}
#endif

#endif
