/* Raw - Another World Interpreter
 * Copyright (C) 2004 Gregory Montoir
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <SDL.h>
#include "sys.h"
#include "util.h"
#include "pad.h"
#include "keymap.h"
#include "display.h"
#include "host_voices.h"

struct SDLStub : System {
	typedef void (SDLStub::*ScaleProc)(uint16_t *dst, uint16_t dstPitch, const uint16_t *src, uint16_t srcPitch, uint16_t w, uint16_t h);

	enum {
		SCREEN_W = 320,
		SCREEN_H = 200,
		SOUND_SAMPLE_RATE = HOST_VOICES_RATE
	};

	int DEFAULT_SCALE = 3;

	SDL_Surface *_screen = nullptr;
	SDL_Window * _window = nullptr;
	SDL_Renderer * _renderer = nullptr;
	uint8_t _scale = DEFAULT_SCALE;
	uint32_t _pad = 0;

	virtual ~SDLStub() {}
	virtual void init(const char *title);
	virtual void destroy();
	virtual void setPalette(const uint8_t *buf);
	virtual void updateDisplay(const uint8_t *src);
	virtual void processEvents();
	virtual void sleep(uint32_t duration);
	virtual uint32_t getTimeStamp();
	virtual void startAudio(AudioCallback callback, void *param);
	virtual void stopAudio();
	virtual uint32_t getOutputSampleRate();
	virtual int addTimer(uint32_t delay, TimerCallback callback, void *param);
	virtual void removeTimer(int timerId);
	virtual void *createMutex();
	virtual void destroyMutex(void *mutex);
	virtual void lockMutex(void *mutex);
	virtual void unlockMutex(void *mutex);

	void prepareGfxMode();
	void applyPad();
	void cleanupGfxMode();
	void switchGfxMode();
};

void SDLStub::init(const char *title) {
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
	SDL_ShowCursor(SDL_DISABLE);

	SDL_ShowCursor( SDL_ENABLE );
	SDL_CaptureMouse(SDL_TRUE);

	memset(&input, 0, sizeof(input));
  _scale = DEFAULT_SCALE;
	prepareGfxMode();
}

void SDLStub::destroy() {
	cleanupGfxMode();
	SDL_Quit();
}

static SDL_Color palette[NUM_COLORS];
static uint8_t s_paletteRaw[NUM_COLORS * 2];
void SDLStub::setPalette(const uint8_t *p) {
  memcpy(s_paletteRaw, p, sizeof(s_paletteRaw));
  for (int i = 0; i < NUM_COLORS; ++i)
  {
    uint8_t c1 = *(p + 0);
    uint8_t c2 = *(p + 1);
    palette[i].r = (((c1 & 0x0F) << 2) | ((c1 & 0x0F) >> 2)) << 2;
    palette[i].g = (((c2 & 0xF0) >> 2) | ((c2 & 0xF0) >> 6)) << 2;
    palette[i].b = (((c2 & 0x0F) >> 2) | ((c2 & 0x0F) << 2)) << 2;
    palette[i].a = 0xFF;
    p += 2;
  }
  SDL_SetPaletteColors(_screen->format->palette, palette, 0, NUM_COLORS);
}

void SDLStub::prepareGfxMode() {
  int w = SCREEN_W;
  int h = SCREEN_H;

  _window = SDL_CreateWindow("Out of this World", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w * _scale, h * _scale, SDL_WINDOW_SHOWN);
  _renderer = SDL_CreateRenderer(_window, -1, 0);
  _screen = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 8, 0, 0, 0, 0);
  if (!_screen) {
    error("SDLStub::prepareGfxMode() unable to allocate _screen buffer");
  }
  SDL_SetPaletteColors(_screen->format->palette, palette, 0, NUM_COLORS);
}

void SDLStub::updateDisplay(const uint8_t *src) {
  uint16_t height = SCREEN_H;
	uint8_t* p = (uint8_t*)_screen->pixels;

	while (height--) {
		for (int i = 0; i < SCREEN_W / 2; ++i) {
			p[i * 2 + 0] = *(src + i) >> 4;
			p[i * 2 + 1] = *(src + i) & 0xF;
		}
		p += _screen->pitch;
    src += SCREEN_W/2;
	}

  SDL_Texture* texture = SDL_CreateTextureFromSurface(_renderer, _screen);
  SDL_RenderCopy(_renderer, texture, nullptr, nullptr);
  SDL_RenderPresent(_renderer);
  SDL_DestroyTexture(texture);

}

static uint32_t padBitForKey(SDL_Keycode key) {
	switch (key) {
	case SDLK_UP:     return PAD_BIT_UP;
	case SDLK_DOWN:   return PAD_BIT_DOWN;
	case SDLK_LEFT:   return PAD_BIT_LEFT;
	case SDLK_RIGHT:  return PAD_BIT_RIGHT;
	case SDLK_RETURN: return PAD_BIT_PAUSE;
	case SDLK_SPACE:  return PAD_BIT_A;
	case SDLK_z:      return PAD_BIT_A;
	case SDLK_x:      return PAD_BIT_B;
	case SDLK_c:      return PAD_BIT_C;
	case SDLK_a:      return PAD_BIT_X;
	case SDLK_s:      return PAD_BIT_Y;
	case SDLK_d:      return PAD_BIT_Z;
	case SDLK_q:      return PAD_BIT_L;
	case SDLK_w:      return PAD_BIT_R;
	default:          return 0;
	}
}

void SDLStub::applyPad() {
	input.padMask = _pad;
	input.dirMask = 0;
	if (_pad & PAD_BIT_UP)    input.dirMask |= PlayerInput::DIR_UP;
	if (_pad & PAD_BIT_DOWN)  input.dirMask |= PlayerInput::DIR_DOWN;
	if (_pad & PAD_BIT_LEFT)  input.dirMask |= PlayerInput::DIR_LEFT;
	if (_pad & PAD_BIT_RIGHT) input.dirMask |= PlayerInput::DIR_RIGHT;
	bool jump = false;
	bool action = false;
	bool run = false;
	keymapApply(keymapActive(), _pad, &action, &jump, &run);
	input.jump   = jump;
	input.button = action;
	input.run    = run;
	input.pause  = (_pad & PAD_BIT_PAUSE) != 0;
	input.menuConfirm = (_pad & (PAD_BIT_A | PAD_BIT_C)) != 0;
	input.menuCancel  = (_pad & PAD_BIT_B) != 0;
	input.menuLeft    = (_pad & PAD_BIT_L) != 0;
	input.menuRight   = (_pad & PAD_BIT_R) != 0;
}

void SDLStub::processEvents() {
	SDL_Event ev;
	while(SDL_PollEvent(&ev)) {
		switch (ev.type) {
		case SDL_QUIT:
			input.quit = true;
			break;
		case SDL_KEYUP:
			_pad &= ~padBitForKey(ev.key.keysym.sym);
			break;
		case SDL_KEYDOWN:
			if (ev.key.repeat) {
				break;
			}
			if (ev.key.keysym.sym == SDLK_ESCAPE) {
				input.quit = true;
				break;
			}
			if (ev.key.keysym.sym == SDLK_BACKSPACE) {
				input.resetRequest = true;
				break;
			}
			if (ev.key.keysym.sym == SDLK_TAB) {
				_scale = _scale + 1;
				if (_scale > 4) { _scale = 1; }
				switchGfxMode();
				break;
			}
			if (ev.key.keysym.sym == SDLK_F1) {
				input.code = true;
				break;
			}
			input.lastChar = ev.key.keysym.sym;
			_pad |= padBitForKey(ev.key.keysym.sym);
			break;
		default:
			break;
		}
	}
	applyPad();
}

void SDLStub::sleep(uint32_t duration) {
	SDL_Delay(duration);
}

uint32_t SDLStub::getTimeStamp() {
	return SDL_GetTicks();	
}

void SDLStub::startAudio(AudioCallback callback, void *param) {
	SDL_AudioSpec desired;
	memset(&desired, 0, sizeof(desired));

	desired.freq = SOUND_SAMPLE_RATE;
	desired.format = AUDIO_U8;
	desired.channels = 1;
	desired.samples = 2048;
	(void)callback;
	desired.callback = host_voices_mix;
	desired.userdata = param;
	if (SDL_OpenAudio(&desired, NULL) == 0) {
		SDL_PauseAudio(0);
	} else {
		error("SDLStub::startAudio() unable to open sound device");
	}
}

void SDLStub::stopAudio() {
	SDL_CloseAudio();
}

uint32_t SDLStub::getOutputSampleRate() {
	return SOUND_SAMPLE_RATE;
}

int SDLStub::addTimer(uint32_t delay, TimerCallback callback, void *param) {
	return SDL_AddTimer(delay, (SDL_TimerCallback)callback, param);
}

void SDLStub::removeTimer(int timerId) {
	SDL_RemoveTimer(timerId);
}

void *SDLStub::createMutex() {
	return SDL_CreateMutex();
}

void SDLStub::destroyMutex(void *mutex) {
	SDL_DestroyMutex((SDL_mutex *)mutex);
}

void SDLStub::lockMutex(void *mutex) {
	SDL_mutexP((SDL_mutex *)mutex);
}

void SDLStub::unlockMutex(void *mutex) {
	SDL_mutexV((SDL_mutex *)mutex);
}

void SDLStub::cleanupGfxMode() {
	if (_screen) {
		SDL_FreeSurface(_screen);
    _screen = 0;
	}

	if (_window) {
	  SDL_DestroyWindow(_window);
	  _window = nullptr;
	}

	if (_screen) {
	  SDL_FreeSurface(_screen);
	  _screen = nullptr;
	}
}

void SDLStub::switchGfxMode() {
  cleanupGfxMode();
	prepareGfxMode();
}

extern "C" void display_get_palette(uint8_t *out) {
	if (out) {
		memcpy(out, s_paletteRaw, sizeof(s_paletteRaw));
	}
}

SDLStub sysImplementation;
System *stub = &sysImplementation;

