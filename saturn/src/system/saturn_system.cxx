#include "sys.h"
#include "saturn_platform.h"
#include "keymap.h"
#include "saturn_audio.h"
#include "saturn_fade.h"

struct SaturnSystem : System {
	bool _resetChordHeld = false;

	virtual ~SaturnSystem() {}

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
};

void SaturnSystem::init(const char *title) {
	(void)title;
	memset(&input, 0, sizeof(input));
	sat_video_init();
	sat_fade_init();
}

void SaturnSystem::destroy() {
}

void SaturnSystem::setPalette(const uint8_t *buf) {
	sat_video_set_palette(buf);
}

void SaturnSystem::updateDisplay(const uint8_t *src) {
	sat_video_present(src);

	const bool chord =
	    (sat_input_level() & PAD_RESET_CHORD) == PAD_RESET_CHORD;

	if (chord && !_resetChordHeld) {
		input.resetRequest = true;
	}
	_resetChordHeld = chord;
}

void SaturnSystem::processEvents() {
	const uint32_t pad = sat_input_read();

	input.padMask = pad;
	input.dirMask = 0;
	if (pad & PAD_BIT_UP)    input.dirMask |= PlayerInput::DIR_UP;
	if (pad & PAD_BIT_DOWN)  input.dirMask |= PlayerInput::DIR_DOWN;
	if (pad & PAD_BIT_LEFT)  input.dirMask |= PlayerInput::DIR_LEFT;
	if (pad & PAD_BIT_RIGHT) input.dirMask |= PlayerInput::DIR_RIGHT;

	bool jump = false;
	bool action = false;
	bool run = false;
	keymapApply(keymapActive(), pad, &action, &jump, &run);
	input.jump   = jump;
	input.button = action;
	input.run    = run;
	input.pause  = (pad & PAD_BIT_PAUSE) != 0;

	input.menuConfirm = (pad & (PAD_BIT_A | PAD_BIT_C)) != 0;
	input.menuCancel  = (pad & PAD_BIT_B) != 0;
	input.menuLeft    = (pad & PAD_BIT_L) != 0;
	input.menuRight   = (pad & PAD_BIT_R) != 0;

	input.quit = false;
	input.code = false;
}

void SaturnSystem::sleep(uint32_t duration) {
	sat_sleep_ms(duration);
}

uint32_t SaturnSystem::getTimeStamp() {
	return clock_ms();
}

void SaturnSystem::startAudio(AudioCallback callback, void *param) {
	(void)callback;
	(void)param;
	sat_audio_start(0, 0);
}

void SaturnSystem::stopAudio() {
	sat_audio_stop();
}

uint32_t SaturnSystem::getOutputSampleRate() {
	return sat_audio_sample_rate();
}

int SaturnSystem::addTimer(uint32_t delay, TimerCallback callback, void *param) {
	return sat_timer_add(delay, (SatTimerCallback)callback, param);
}

void SaturnSystem::removeTimer(int timerId) {
	sat_timer_remove(timerId);
}

static int s_mutexToken;

void *SaturnSystem::createMutex() {
	return &s_mutexToken;
}

void SaturnSystem::destroyMutex(void *mutex) {
	(void)mutex;
}

void SaturnSystem::lockMutex(void *mutex) {
	(void)mutex;
}

void SaturnSystem::unlockMutex(void *mutex) {
	(void)mutex;
}

static SaturnSystem sysImplementation;
System *stub = &sysImplementation;
