#ifndef CD_DRIVE_H
#define CD_DRIVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CD_DRIVE_FAD_ERR 0xffffffu

void cd_drive_read_toc(uint32_t *toc);
void cd_drive_start_analysis(void);
void cd_drive_halt(void);
int cd_drive_status(uint32_t *fad);
void cd_drive_play_fad(uint32_t startFad, uint32_t count);
void cd_drive_play_track(uint16_t track, int loop);
uint32_t cd_drive_volume(void);

#ifdef __cplusplus
}
#endif

#endif
