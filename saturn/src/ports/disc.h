#ifndef DISC_H
#define DISC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DiscFile DiscFile;

DiscFile *disc_open(const char *name);
void disc_close(DiscFile *file);
int32_t disc_size(DiscFile *file);
int32_t disc_read(DiscFile *file, int32_t pos, void *dst, int32_t size);

#ifdef __cplusplus
}
#endif

#endif
