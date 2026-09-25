#ifndef HOST_VOICES_H
#define HOST_VOICES_H

#include <stdint.h>

#define HOST_VOICES_RATE 22050

void host_voices_mix(void *param, uint8_t *stream, int len);

#endif
