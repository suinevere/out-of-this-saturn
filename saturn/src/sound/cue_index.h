#ifndef CUE_INDEX_H
#define CUE_INDEX_H

#include <stdint.h>

#define CUE_INDEX_FILTER_SIZE 256

extern unsigned char g_cueFilter[CUE_INDEX_FILTER_SIZE];

#ifdef __cplusplus
extern "C" {
#endif

void cueIndexSetPart(unsigned short partId, const unsigned char *segBytecode);

int cueIndexAt(const unsigned char *pc, int *cue, int *loops, int *pos);

int cueIndexCount(void);

#ifdef __cplusplus
}
#endif

#endif
