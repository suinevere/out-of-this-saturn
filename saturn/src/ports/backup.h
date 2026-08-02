#ifndef BACKUP_H
#define BACKUP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BACKUP_INTERNAL 1
#define BACKUP_CART     2

#define BACKUP_OK              0
#define BACKUP_ERR_NONE        1
#define BACKUP_ERR_UNFORMAT    2
#define BACKUP_ERR_PROTECTED   3
#define BACKUP_ERR_NO_SPACE    4
#define BACKUP_ERR_NOT_FOUND   5
#define BACKUP_ERR_BROKEN      6
#define BACKUP_ERR_EXISTS      7

typedef struct {
    int      present;
    int      formatted;
    int      writeProtected;
    uint32_t freeBytes;
} BackupDevice;

typedef struct {
    int      exists;
    uint32_t size;
    uint32_t date;
} BackupEntry;

void backup_init(void);

int backup_probe(uint32_t device, BackupDevice *out);

typedef struct {
    uint32_t blockSize;
    uint32_t freeBlocks;
    int      fits;
} BackupSpace;

int backup_space(uint32_t device, int32_t size, BackupSpace *out);

int backup_dir(uint32_t device, const char *name, BackupEntry *out);

int backup_read(uint32_t device, const char *name, void *dst, int32_t size);

int backup_write(uint32_t device, const char *name, const char *comment,
                  const void *src, int32_t size, int overwrite);

int backup_delete(uint32_t device, const char *name);

uint32_t backup_date_now(void);

void backup_date_split(uint32_t date, int *month, int *day, int *hour, int *min);

#ifdef __cplusplus
}
#endif
#endif
