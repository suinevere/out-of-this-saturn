#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "backup.h"
#include "boot.h"
#include "cd_drive.h"
#include "cdtoc.h"
#include "clock.h"
#include "diag.h"
#include "fade.h"
#include "timers.h"

#define HOST_BACKUP_FREE_BYTES 0x100000u
#define HOST_BACKUP_BLOCK_SIZE 64u

static void host_backup_path(const char *name, char *out, size_t size)
{
	snprintf(out, size, "%s.bup", name);
}

static FILE *host_backup_open(const char *name, const char *mode)
{
	char path[64];
	host_backup_path(name, path, sizeof(path));
	return fopen(path, mode);
}

extern "C" void backup_init(void)
{
}

extern "C" int backup_probe(uint32_t device, BackupDevice *out)
{
	memset(out, 0, sizeof(*out));
	if (device != BACKUP_INTERNAL) {
		return BACKUP_ERR_NONE;
	}
	out->present = 1;
	out->formatted = 1;
	out->freeBytes = HOST_BACKUP_FREE_BYTES;
	return BACKUP_OK;
}

extern "C" int backup_space(uint32_t device, int32_t size, BackupSpace *out)
{
	(void)size;
	memset(out, 0, sizeof(*out));
	if (device != BACKUP_INTERNAL) {
		return BACKUP_ERR_NONE;
	}
	out->blockSize = HOST_BACKUP_BLOCK_SIZE;
	out->freeBlocks = HOST_BACKUP_FREE_BYTES / HOST_BACKUP_BLOCK_SIZE;
	out->fits = 1;
	return BACKUP_OK;
}

extern "C" int backup_dir(uint32_t device, const char *name, BackupEntry *out)
{
	memset(out, 0, sizeof(*out));
	if (device != BACKUP_INTERNAL) {
		return BACKUP_ERR_NONE;
	}
	FILE *f = host_backup_open(name, "rb");
	if (!f) {
		return BACKUP_OK;
	}
	uint32_t date = 0;
	if (fread(&date, sizeof(date), 1, f) == 1) {
		fseek(f, 0, SEEK_END);
		out->exists = 1;
		out->size = (uint32_t)(ftell(f) - (long)sizeof(date));
		out->date = date;
	}
	fclose(f);
	return BACKUP_OK;
}

extern "C" int backup_read(uint32_t device, const char *name, void *dst, int32_t size)
{
	if (device != BACKUP_INTERNAL) {
		return BACKUP_ERR_NONE;
	}
	FILE *f = host_backup_open(name, "rb");
	if (!f) {
		return BACKUP_ERR_NOT_FOUND;
	}
	uint32_t date = 0;
	int rc = BACKUP_OK;
	if (fread(&date, sizeof(date), 1, f) != 1) {
		rc = BACKUP_ERR_BROKEN;
	} else {
		fseek(f, 0, SEEK_END);
		const long stored = ftell(f) - (long)sizeof(date);
		fseek(f, (long)sizeof(date), SEEK_SET);
		if (stored > size || fread(dst, 1, (size_t)stored, f) != (size_t)stored) {
			rc = BACKUP_ERR_BROKEN;
		}
	}
	fclose(f);
	return rc;
}

extern "C" int backup_write(uint32_t device, const char *name, const char *comment,
                            const void *src, int32_t size, int overwrite)
{
	(void)comment;
	if (device != BACKUP_INTERNAL) {
		return BACKUP_ERR_NONE;
	}
	if (!overwrite) {
		FILE *existing = host_backup_open(name, "rb");
		if (existing) {
			fclose(existing);
			return BACKUP_ERR_EXISTS;
		}
	}
	FILE *f = host_backup_open(name, "wb");
	if (!f) {
		return BACKUP_ERR_PROTECTED;
	}
	const uint32_t date = backup_date_now();
	int rc = BACKUP_OK;
	if (fwrite(&date, sizeof(date), 1, f) != 1 ||
	    fwrite(src, 1, (size_t)size, f) != (size_t)size) {
		rc = BACKUP_ERR_BROKEN;
	}
	fclose(f);
	return rc;
}

extern "C" int backup_delete(uint32_t device, const char *name)
{
	if (device != BACKUP_INTERNAL) {
		return BACKUP_ERR_NONE;
	}
	char path[64];
	host_backup_path(name, path, sizeof(path));
	return remove(path) == 0 ? BACKUP_OK : BACKUP_ERR_NOT_FOUND;
}

static int host_is_leap(int year)
{
	return (year % 4) == 0;
}

extern "C" uint32_t backup_date_now(void)
{
	const time_t now = time(0);
	const struct tm *t = localtime(&now);
	if (!t) {
		return 0;
	}
	static const int len[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
	uint32_t days = 0;
	for (int y = 1980; y < t->tm_year + 1900; ++y) {
		days += host_is_leap(y) ? 366u : 365u;
	}
	for (int m = 0; m < t->tm_mon; ++m) {
		days += (m == 1 && host_is_leap(t->tm_year + 1900)) ? 29u : (uint32_t)len[m];
	}
	days += (uint32_t)(t->tm_mday - 1);
	return days * 1440u + (uint32_t)(t->tm_hour * 60 + t->tm_min);
}

extern "C" void backup_date_split(uint32_t date, int *month, int *day, int *hour, int *min)
{
	static const int len[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
	uint32_t days = date / 1440u;
	const uint32_t rem = date % 1440u;
	int year = 1980;
	for (;;) {
		const uint32_t inYear = host_is_leap(year) ? 366u : 365u;
		if (days < inYear) {
			break;
		}
		days -= inYear;
		year++;
	}
	int mo = 0;
	for (;;) {
		const uint32_t inMonth = (mo == 1 && host_is_leap(year)) ? 29u : (uint32_t)len[mo];
		if (days < inMonth) {
			break;
		}
		days -= inMonth;
		mo++;
	}
	if (month) *month = mo + 1;
	if (day)   *day = (int)days + 1;
	if (hour)  *hour = (int)(rem / 60u);
	if (min)   *min = (int)(rem % 60u);
}

extern "C" void boot_bios(void)
{
	exit(0);
}

extern "C" int chainboot_available(void)
{
	return 0;
}

extern "C" void chainboot_run(void)
{
}

extern "C" void cd_drive_read_toc(uint32_t *toc)
{
	memset(toc, 0xff, CDTOC_WORDS * sizeof(uint32_t));
}

extern "C" void cd_drive_start_analysis(void)
{
}

extern "C" void cd_drive_halt(void)
{
}

extern "C" int cd_drive_status(uint32_t *fad)
{
	*fad = CD_DRIVE_FAD_ERR;
	return 0;
}

extern "C" void cd_drive_play_fad(uint32_t startFad, uint32_t count)
{
	(void)startFad;
	(void)count;
}

extern "C" void cd_drive_play_track(uint16_t track, int loop)
{
	(void)track;
	(void)loop;
}

extern "C" uint32_t cd_drive_volume(void)
{
	return 0;
}

extern "C" uint32_t clock_ms(void)
{
	return SDL_GetTicks();
}

extern "C" void diag_row(int row, const char *fmt, int a, int b, int c, int d, int e)
{
	(void)row;
	(void)fmt;
	(void)a;
	(void)b;
	(void)c;
	(void)d;
	(void)e;
}

static int s_fadeLevel = FADE_LIT;

extern "C" void fade_set(int level)
{
	s_fadeLevel = level;
}

extern "C" int fade_level(void)
{
	return s_fadeLevel;
}

extern "C" void fade_ramp(int target, int frames)
{
	(void)frames;
	s_fadeLevel = target;
}

extern "C" void fade_audio_follow(int on)
{
	(void)on;
}

extern "C" void timers_pump(void)
{
}
