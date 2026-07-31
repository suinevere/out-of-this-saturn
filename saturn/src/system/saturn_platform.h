#ifndef SATURN_PLATFORM_H
#define SATURN_PLATFORM_H

#include <stdint.h>
#include "clock.h"
#include "pad.h"
#include "diag.h"
#include "boot.h"
#include "display.h"

#ifdef __cplusplus
extern "C" {
#endif

void sat_boot_init(void);

void sat_video_init(void);

void sat_video_set_palette(const uint8_t *colors);

void sat_video_present(const uint8_t *page);

void sat_video_sync(void);

uint32_t sat_input_read(void);

uint32_t sat_input_level(void);

void sat_loading_tick(void);

void sat_sleep_ms(uint32_t ms);

int sat_system_reset(void);

#ifdef __cplusplus
}
#endif
#endif
