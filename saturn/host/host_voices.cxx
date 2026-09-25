#include <SDL.h>
#include <string.h>
#include "host_voices.h"
#include "voices.h"

#define HOST_VOICE_COUNT 4

struct HostVoice {
	const uint8_t *data;
	uint32_t len;
	uint32_t loopPos;
	uint32_t loopLen;
	uint32_t pos;
	uint32_t inc;
	uint8_t volume;
	bool active;
};

static HostVoice s_voices[HOST_VOICE_COUNT];

void host_voices_mix(void *param, uint8_t *stream, int len)
{
	(void)param;
	for (int i = 0; i < len; ++i) {
		int acc = 0;
		for (int v = 0; v < HOST_VOICE_COUNT; ++v) {
			HostVoice *ch = &s_voices[v];
			if (!ch->active) {
				continue;
			}
			uint32_t p = ch->pos >> 8;
			if (ch->loopLen != 0) {
				if (p >= ch->loopPos + ch->loopLen) {
					ch->pos = ch->loopPos << 8;
					p = ch->loopPos;
				}
			} else if (p >= ch->len) {
				ch->active = false;
				continue;
			}
			acc += ((int8_t)ch->data[p] * ch->volume) / 0x40;
			ch->pos += ch->inc;
		}
		if (acc > 127) {
			acc = 127;
		} else if (acc < -128) {
			acc = -128;
		}
		stream[i] = (uint8_t)(acc + 128);
	}
}

extern "C" void voice_play(uint8_t channel, const uint8_t *data, uint16_t len,
                           uint16_t loopPos, uint16_t loopLen,
                           uint16_t freq, uint8_t volume)
{
	if (channel >= HOST_VOICE_COUNT) {
		return;
	}
	SDL_LockAudio();
	HostVoice *ch = &s_voices[channel];
	ch->data = data;
	ch->len = len;
	ch->loopPos = loopPos;
	ch->loopLen = loopLen;
	ch->pos = 0;
	ch->inc = ((uint32_t)freq << 8) / HOST_VOICES_RATE;
	ch->volume = volume;
	ch->active = data != 0 && len != 0;
	SDL_UnlockAudio();
}

extern "C" void voice_stop(uint8_t channel)
{
	if (channel >= HOST_VOICE_COUNT) {
		return;
	}
	SDL_LockAudio();
	s_voices[channel].active = false;
	SDL_UnlockAudio();
}

extern "C" void voice_set_volume(uint8_t channel, uint8_t volume)
{
	if (channel >= HOST_VOICE_COUNT) {
		return;
	}
	SDL_LockAudio();
	s_voices[channel].volume = volume;
	SDL_UnlockAudio();
}

extern "C" void voice_stop_all(void)
{
	SDL_LockAudio();
	memset(s_voices, 0, sizeof(s_voices));
	SDL_UnlockAudio();
}

extern "C" void voice_flush_samples(void)
{
	voice_stop_all();
}
