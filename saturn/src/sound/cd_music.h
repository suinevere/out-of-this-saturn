#ifndef CD_MUSIC_H
#define CD_MUSIC_H

#ifdef __cplusplus
extern "C" {
#endif

void cd_music_init(void);

void cd_music_set_part(unsigned short partId);

void cd_music_set_part_resumed(unsigned short partId);

void cd_music_set_cue(int cue, int loop, unsigned int delayMs);

int cd_music_available(void);

int cd_music_audible(void);

unsigned int cd_music_ms_since_cue(void);

int cd_music_track_ms(void);

void cd_music_part_began(unsigned short partId);

void cd_music_tick(void);

void cd_music_frame_shown(void);

void cd_music_suspend(void);

void cd_music_restore(void);

void cd_music_pause(void);

void cd_music_resume(void);

void cd_music_stop(void);

int cd_music_is_playing(void);

#ifdef __cplusplus
}
#endif

#endif
