#include <cstring>
#include "backup.h"

#define STUB_MAX_FILES 8
#define STUB_MAX_BYTES 8192
#define STUB_BLOCK_SIZE 64

typedef struct {
    char     name[12];
    uint8_t  data[STUB_MAX_BYTES];
    int32_t  size;
    uint32_t date;
    int      used;
} StubFile;

static BackupDevice s_dev[3];
static StubFile  s_files[3][STUB_MAX_FILES];
static uint32_t  s_blockSize[3];

void stub_bup_reset(void)
{
    memset(s_dev, 0, sizeof(s_dev));
    memset(s_files, 0, sizeof(s_files));
    for (int i = 0; i < 3; ++i) {
        s_blockSize[i] = STUB_BLOCK_SIZE;
    }
    s_dev[BACKUP_INTERNAL].present = 1;
    s_dev[BACKUP_INTERNAL].formatted = 1;
    s_dev[BACKUP_INTERNAL].freeBytes = 29000;
}

void stub_bup_set_device(uint32_t device, int present, int formatted,
                         int writeProtected, uint32_t freeBytes)
{
    s_dev[device].present = present;
    s_dev[device].formatted = formatted;
    s_dev[device].writeProtected = writeProtected;
    s_dev[device].freeBytes = freeBytes;
}

void stub_bup_set_block_size(uint32_t device, uint32_t blockSize)
{
    s_blockSize[device] = blockSize;
}

int backup_space(uint32_t device, int32_t size, BackupSpace *out)
{
    memset(out, 0, sizeof(*out));
    if (!s_dev[device].present) return BACKUP_ERR_NONE;
    if (!s_dev[device].formatted) return BACKUP_ERR_UNFORMAT;
    if (s_dev[device].writeProtected) return BACKUP_ERR_PROTECTED;

    const uint32_t blockSize = s_blockSize[device];
    const uint32_t usable = (blockSize > 6) ? (blockSize - 6) : 1;
    const uint32_t needed = 1 + (((uint32_t)size + usable - 1) / usable);

    out->blockSize = blockSize;
    out->freeBlocks = s_dev[device].freeBytes / blockSize;
    out->fits = (needed <= out->freeBlocks) ? 1 : 0;
    return BACKUP_OK;
}

static StubFile *stub_find(uint32_t device, const char *name)
{
    for (int i = 0; i < STUB_MAX_FILES; ++i) {
        if (s_files[device][i].used &&
            strcmp(s_files[device][i].name, name) == 0) {
            return &s_files[device][i];
        }
    }
    return 0;
}

void stub_bup_add_file(uint32_t device, const char *name, const void *data,
                       int32_t size, uint32_t date)
{
    for (int i = 0; i < STUB_MAX_FILES; ++i) {
        if (!s_files[device][i].used) {
            s_files[device][i].used = 1;
            strncpy(s_files[device][i].name, name, 11);
            s_files[device][i].name[11] = 0;
            memcpy(s_files[device][i].data, data, size);
            s_files[device][i].size = size;
            s_files[device][i].date = date;
            return;
        }
    }
}

void backup_init(void) {}

int backup_probe(uint32_t device, BackupDevice *out)
{
    *out = s_dev[device];
    if (!out->present) {
        return BACKUP_ERR_NONE;
    }
    if (!out->formatted) {
        return BACKUP_ERR_UNFORMAT;
    }
    return BACKUP_OK;
}

int backup_dir(uint32_t device, const char *name, BackupEntry *out)
{
    StubFile *f = stub_find(device, name);
    memset(out, 0, sizeof(*out));
    if (f) {
        out->exists = 1;
        out->size = (uint32_t)f->size;
        out->date = f->date;
    }
    return BACKUP_OK;
}

int backup_read(uint32_t device, const char *name, void *dst, int32_t size)
{
    StubFile *f = stub_find(device, name);
    if (!f) {
        return BACKUP_ERR_NOT_FOUND;
    }
    if (f->size > size) {
        return BACKUP_ERR_BROKEN;
    }
    memcpy(dst, f->data, f->size);
    return BACKUP_OK;
}

int backup_write(uint32_t device, const char *name, const char *comment,
                  const void *src, int32_t size, int overwrite)
{
    (void)comment;
    if (!s_dev[device].present) return BACKUP_ERR_NONE;
    if (!s_dev[device].formatted) return BACKUP_ERR_UNFORMAT;
    if (s_dev[device].writeProtected) return BACKUP_ERR_PROTECTED;
    if ((uint32_t)size > s_dev[device].freeBytes) return BACKUP_ERR_NO_SPACE;
    if (size > STUB_MAX_BYTES) return BACKUP_ERR_NO_SPACE;

    StubFile *f = stub_find(device, name);
    if (f && !overwrite) {
        return BACKUP_ERR_EXISTS;
    }
    if (f) {
        memcpy(f->data, src, size);
        f->size = size;
        return BACKUP_OK;
    }
    stub_bup_add_file(device, name, src, size, 0);
    return BACKUP_OK;
}

int backup_delete(uint32_t device, const char *name)
{
    StubFile *f = stub_find(device, name);
    if (!f) {
        return BACKUP_ERR_NOT_FOUND;
    }
    f->used = 0;
    return BACKUP_OK;
}

uint32_t backup_date_now(void) { return 0; }

void backup_date_split(uint32_t date, int *month, int *day, int *hour, int *min)
{
    static const int len[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

    uint32_t days = date / 1440u;
    uint32_t rem = date % 1440u;

    int year = 1980;
    for (;;) {
        int inYear = ((year % 4) == 0) ? 366 : 365;
        if (days < (uint32_t)inYear) {
            break;
        }
        days -= (uint32_t)inYear;
        year++;
    }

    int mo = 0;
    for (;;) {
        int inMonth = len[mo];
        if (mo == 1 && (year % 4) == 0) {
            inMonth = 29;
        }
        if (days < (uint32_t)inMonth) {
            break;
        }
        days -= (uint32_t)inMonth;
        mo++;
    }

    if (month) *month = mo + 1;
    if (day)   *day = (int)days + 1;
    if (hour)  *hour = (int)(rem / 60u);
    if (min)   *min = (int)(rem % 60u);
}
