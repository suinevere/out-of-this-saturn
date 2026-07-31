#ifndef FADE_H
#define FADE_H

#ifdef __cplusplus
extern "C" {
#endif

#define FADE_DARK 0
#define FADE_LIT  256

void fade_set(int level);
int fade_level(void);
void fade_ramp(int target, int frames);
void fade_audio_follow(int on);

#ifdef __cplusplus
}
#endif

#endif
