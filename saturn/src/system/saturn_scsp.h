#ifndef SATURN_SCSP_H
#define SATURN_SCSP_H

#include <stdint.h>
#include "voices.h"

#ifdef __cplusplus
extern "C" {
#endif

void sat_scsp_init(void);
void sat_scsp_shutdown(void);

void sat_scsp_set_master(uint8_t vol);

uint32_t sat_scsp_debug_active(void);
uint32_t sat_scsp_debug_heap_used(void);
uint32_t sat_scsp_debug_cache_used(void);
uint32_t sat_scsp_debug_uploads(void);

#ifdef __cplusplus
}
#endif
#endif
