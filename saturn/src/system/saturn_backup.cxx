#include <srl.hpp>
#include "sega_bup.h"
#include "backup.h"
#include "bup_devmap.h"

extern "C" {
#include <string.h>
}

#define SAT_BUP_STAT_PROBE 8192

static uint32_t s_bupWork[0x1000];

static uint32_t s_bupLib[0x1000];

static BupConfig s_bupCfg[3];

static int s_internalIdx = 0;
static int s_cartIdx = 1;

static int sat_bup_hw(uint32_t device)
{
    if (device == BACKUP_INTERNAL) {
        return s_internalIdx;
    }
    if (device == BACKUP_CART) {
        return s_cartIdx;
    }
    return BUP_DEVMAP_NONE;
}

static int sat_bup_map_error(int32_t rc)
{
    switch (rc) {
    case 0:                        return BACKUP_OK;
    case BUP_NON:                  return BACKUP_ERR_NONE;
    case BUP_UNFORMAT:             return BACKUP_ERR_UNFORMAT;
    case BUP_WRITE_PROTECT:        return BACKUP_ERR_PROTECTED;
    case BUP_NOT_ENOUGH_MEMORY:    return BACKUP_ERR_NO_SPACE;
    case BUP_NOT_FOUND:            return BACKUP_ERR_NOT_FOUND;
    case BUP_FOUND:                return BACKUP_ERR_EXISTS;
    case BUP_NO_MATCH:             return BACKUP_ERR_BROKEN;
    case BUP_BROKEN:               return BACKUP_ERR_BROKEN;
    default:                       return BACKUP_ERR_BROKEN;
    }
}

extern "C" void backup_init(void)
{
    int present[3];

    BUP_Init(s_bupLib, s_bupWork, s_bupCfg);

    for (int i = 0; i < 3; ++i) {
        BupStat st;
        const int32_t rc = BUP_Stat((uint32_t)i, SAT_BUP_STAT_PROBE, &st);
        present[i] = (rc != BUP_NON) ? 1 : 0;
    }
    bupDevmapResolve(present, 3, &s_internalIdx, &s_cartIdx);
}

extern "C" int backup_probe(uint32_t device, BackupDevice *out)
{
    BupStat st;
    memset(out, 0, sizeof(*out));

    const int hw = sat_bup_hw(device);
    if (hw == BUP_DEVMAP_NONE) {
        return BACKUP_ERR_NONE;
    }

    int32_t rc = BUP_Stat((uint32_t)hw, SAT_BUP_STAT_PROBE, &st);
    if (rc == BUP_NON) {
        return BACKUP_ERR_NONE;
    }
    out->present = 1;
    if (rc == BUP_UNFORMAT) {
        return BACKUP_ERR_UNFORMAT;
    }
    out->formatted = 1;
    if (rc == BUP_WRITE_PROTECT) {
        out->writeProtected = 1;
        return BACKUP_ERR_PROTECTED;
    }
    out->freeBytes = st.freesize;
    return BACKUP_OK;
}

extern "C" int backup_space(uint32_t device, int32_t size, BackupSpace *out)
{
    BupStat st;
    memset(out, 0, sizeof(*out));

    const int hw = sat_bup_hw(device);
    if (hw == BUP_DEVMAP_NONE) {
        return BACKUP_ERR_NONE;
    }

    const int32_t rc = BUP_Stat((uint32_t)hw, (uint32_t)size, &st);
    if (rc == BUP_NON) {
        return BACKUP_ERR_NONE;
    }
    if (rc == BUP_UNFORMAT) {
        return BACKUP_ERR_UNFORMAT;
    }
    if (rc == BUP_WRITE_PROTECT) {
        return BACKUP_ERR_PROTECTED;
    }

    out->blockSize = st.blocksize;
    out->freeBlocks = st.freeblock;
    out->fits = (st.datanum >= 1) ? 1 : 0;
    return BACKUP_OK;
}

extern "C" int backup_dir(uint32_t device, const char *name, BackupEntry *out)
{
    BupDir dir;
    memset(out, 0, sizeof(*out));
    memset(&dir, 0, sizeof(dir));

    const int hw = sat_bup_hw(device);
    if (hw == BUP_DEVMAP_NONE) {
        return BACKUP_ERR_NONE;
    }

    int32_t rc = BUP_Dir((uint32_t)hw, (uint8_t *)name, 1, &dir);
    if (rc < 0) {
        return sat_bup_map_error(rc);
    }
    if (rc == 0) {
        return BACKUP_OK;
    }
    out->exists = 1;
    out->size = dir.datasize;
    out->date = dir.date;
    return BACKUP_OK;
}

extern "C" int backup_read(uint32_t device, const char *name, void *dst,
                            int32_t size)
{
    BupDir dir;
    memset(&dir, 0, sizeof(dir));

    const int hw = sat_bup_hw(device);
    if (hw == BUP_DEVMAP_NONE) {
        return BACKUP_ERR_NONE;
    }

    int32_t dirRc = BUP_Dir((uint32_t)hw, (uint8_t *)name, 1, &dir);
    if (dirRc < 0) {
        return sat_bup_map_error(dirRc);
    }
    if (dirRc == 0) {
        return BACKUP_ERR_NOT_FOUND;
    }
    if (dir.datasize > (uint32_t)size) {
        return BACKUP_ERR_BROKEN;
    }

    int32_t rc = BUP_Read((uint32_t)hw, (uint8_t *)name, (uint8_t *)dst);
    return sat_bup_map_error(rc);
}

static void sat_bup_fill_dir(BupDir *dir, const char *name,
                             const char *comment, int32_t size)
{
    memset(dir, 0, sizeof(*dir));
    strncpy((char *)dir->filename, name, 11);
    strncpy((char *)dir->comment, comment, 10);
    dir->language = BUP_ENGLISH;
    dir->date = backup_date_now();
    dir->datasize = (uint32_t)size;
    dir->blocksize = 0;
}

extern "C" int backup_write(uint32_t device, const char *name,
                             const char *comment, const void *src,
                             int32_t size, int overwrite)
{
    BupDir dir;
    const int hw = sat_bup_hw(device);
    if (hw == BUP_DEVMAP_NONE) {
        return BACKUP_ERR_NONE;
    }

    sat_bup_fill_dir(&dir, name, comment, size);

    int32_t rc = BUP_Write((uint32_t)hw, &dir, (uint8_t *)src,
                           overwrite ? 1 : 0);

    if (overwrite && rc == BUP_FOUND) {
        BUP_Delete((uint32_t)hw, (uint8_t *)name);
        sat_bup_fill_dir(&dir, name, comment, size);
        rc = BUP_Write((uint32_t)hw, &dir, (uint8_t *)src, 1);
    }

    return sat_bup_map_error(rc);
}

extern "C" int backup_delete(uint32_t device, const char *name)
{
    const int hw = sat_bup_hw(device);
    if (hw == BUP_DEVMAP_NONE) {
        return BACKUP_ERR_NONE;
    }
    return sat_bup_map_error(BUP_Delete((uint32_t)hw, (uint8_t *)name));
}

extern "C" uint32_t backup_date_now(void)
{
    SRL::Types::DateTime now = SRL::Types::DateTime::Now();
    BupDate d = now.ToBackupUnitDate();
    return BUP_SetDate(&d);
}

extern "C" void backup_date_split(uint32_t date, int *month, int *day,
                                   int *hour, int *min)
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
