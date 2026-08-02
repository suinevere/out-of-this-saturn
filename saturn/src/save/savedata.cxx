#include "savedata.h"
#include "serializer.h"

extern "C" {
#include <string.h>
}

static uint8_t s_saveBuf[SAVE_MAX_BYTES];

uint8_t *savedataBuffer(void)
{
	return s_saveBuf;
}

void savedataClear(SaveInfo *info)
{
	info->state = SAVE_FILE_OK;
	info->hasState = false;
	info->hasCode = false;
	info->statePartId = 0;
	info->stateDate = 0;
	info->stateLen = 0;
	info->codePartId = 0;
	info->codeDate = 0;
	info->codeWord = 0;
	info->reached = 0;
}

static void savedataPut16(uint8_t *buf, uint16_t v)
{
	buf[0] = (uint8_t)((v >> 8) & 0xFF);
	buf[1] = (uint8_t)(v & 0xFF);
}

static void savedataPut32(uint8_t *buf, uint32_t v)
{
	buf[0] = (uint8_t)((v >> 24) & 0xFF);
	buf[1] = (uint8_t)((v >> 16) & 0xFF);
	buf[2] = (uint8_t)((v >> 8) & 0xFF);
	buf[3] = (uint8_t)(v & 0xFF);
}

static uint16_t savedataGet16(const uint8_t *buf)
{
	return (uint16_t)(((uint16_t)buf[0] << 8) | (uint16_t)buf[1]);
}

static uint32_t savedataGet32(const uint8_t *buf)
{
	return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
	       ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
}

void savedataWriteHeader(uint8_t *buf, const SaveInfo *info)
{
	uint16_t present = 0;
	if (info->hasState) {
		present |= SAVE_HAS_STATE;
	}
	if (info->hasCode) {
		present |= SAVE_HAS_CODE;
	}

	memset(buf, 0, SAVE_HEADER_SIZE);
	buf[0] = 'A';
	buf[1] = 'W';
	buf[2] = 'S';
	buf[3] = 'V';
	savedataPut16(buf + 4, (uint16_t)Serializer::CUR_VER);
	savedataPut16(buf + 6, present);
	savedataPut16(buf + 8, info->statePartId);
	savedataPut32(buf + 10, info->stateDate);
	savedataPut16(buf + 14, info->stateLen);
	savedataPut16(buf + 16, info->codePartId);
	savedataPut32(buf + 18, info->codeDate);
	savedataPut16(buf + 22, info->codeWord);
	buf[24] = info->reached;
}

bool savedataReadHeader(const uint8_t *buf, uint16_t *ver, SaveInfo *info)
{
	if (buf[0] != 'A' || buf[1] != 'W' || buf[2] != 'S' || buf[3] != 'V') {
		return false;
	}

	const uint16_t present = savedataGet16(buf + 6);

	*ver = savedataGet16(buf + 4);
	info->state = SAVE_FILE_OK;
	info->hasState = (present & SAVE_HAS_STATE) != 0;
	info->hasCode = (present & SAVE_HAS_CODE) != 0;
	info->statePartId = savedataGet16(buf + 8);
	info->stateDate = savedataGet32(buf + 10);
	info->stateLen = savedataGet16(buf + 14);
	info->codePartId = savedataGet16(buf + 16);
	info->codeDate = savedataGet32(buf + 18);
	info->codeWord = savedataGet16(buf + 22);
	info->reached = buf[24];
	return true;
}

SaveFileState savedataProbe(uint32_t device, SaveInfo *out)
{
	savedataClear(out);
	out->state = SAVE_FILE_NONE;

	BackupEntry entry;
	const int dirRc = backup_dir(device, SAVE_FILE_NAME, &entry);
	if (dirRc != BACKUP_OK || !entry.exists) {
		return SAVE_FILE_NONE;
	}

	if (backup_read(device, SAVE_FILE_NAME, s_saveBuf, SAVE_MAX_BYTES)
	    != BACKUP_OK) {
		out->state = SAVE_FILE_DAMAGED;
		return SAVE_FILE_DAMAGED;
	}

	uint16_t ver = 0;
	if (!savedataReadHeader(s_saveBuf, &ver, out)) {
		savedataClear(out);
		out->state = SAVE_FILE_DAMAGED;
		return SAVE_FILE_DAMAGED;
	}

	if (ver != Serializer::CUR_VER) {
		out->state = SAVE_FILE_OLD_VERSION;
		return SAVE_FILE_OLD_VERSION;
	}

	if (out->hasState &&
	    (int32_t)out->stateLen > (int32_t)(SAVE_MAX_BYTES - SAVE_HEADER_SIZE)) {
		out->state = SAVE_FILE_DAMAGED;
		return SAVE_FILE_DAMAGED;
	}

	out->state = SAVE_FILE_OK;
	return SAVE_FILE_OK;
}

bool savedataHasAny(const SaveInfo *info)
{
	return info->state == SAVE_FILE_OK && (info->hasState || info->hasCode);
}

bool savedataNewestIsCode(const SaveInfo *info)
{
	if (!info->hasCode) {
		return false;
	}
	if (!info->hasState) {
		return true;
	}
	return info->codeDate >= info->stateDate;
}

uint32_t savedataBlocksNeeded(uint32_t blockSize, int32_t bytes)
{
	const uint32_t linkOverhead = 6;

	if (blockSize <= linkOverhead) {
		return 0;
	}
	if (bytes <= 0) {
		return 1;
	}

	const uint32_t usable = blockSize - linkOverhead;
	return 1 + (((uint32_t)bytes + usable - 1) / usable);
}

int savedataReserve(uint32_t device, BackupSpace *space)
{
	BackupSpace local;
	BackupSpace *sp = (space != 0) ? space : &local;
	memset(sp, 0, sizeof(*sp));

	BackupDevice dev;
	const int probeRc = backup_probe(device, &dev);
	if (probeRc != BACKUP_OK) {
		return probeRc;
	}

	BackupEntry entry;
	const int dirRc = backup_dir(device, SAVE_FILE_NAME, &entry);
	if (dirRc != BACKUP_OK) {
		return dirRc;
	}

	const int spaceRc = backup_space(device, SAVE_MAX_BYTES, sp);
	if (spaceRc != BACKUP_OK) {
		return spaceRc;
	}

	if (entry.exists && entry.size >= (uint32_t)SAVE_MAX_BYTES) {
		return BACKUP_OK;
	}

	if (!sp->fits && !entry.exists) {
		return BACKUP_ERR_NO_SPACE;
	}

	SaveInfo info;
	savedataClear(&info);

	if (entry.exists && savedataProbe(device, &info) != SAVE_FILE_OK) {
		savedataClear(&info);
	}

	const int32_t keep = info.hasState ? (int32_t)info.stateLen : 0;

	if (!entry.exists) {
		memset(s_saveBuf, 0, SAVE_MAX_BYTES);
	} else {
		memset(s_saveBuf + SAVE_HEADER_SIZE + keep, 0,
		       (size_t)(SAVE_MAX_BYTES - SAVE_HEADER_SIZE - keep));
	}

	savedataWriteHeader(s_saveBuf, &info);

	return backup_write(device, SAVE_FILE_NAME, "ANOTHERWLD", s_saveBuf,
	                     SAVE_MAX_BYTES, 1);
}

int savedataStoreProgress(uint32_t device, uint8_t reached)
{
	SaveInfo info;
	const SaveFileState state = savedataProbe(device, &info);

	if (state != SAVE_FILE_OK) {
		return (state == SAVE_FILE_NONE) ? BACKUP_ERR_NOT_FOUND
		                                 : BACKUP_ERR_BROKEN;
	}
	if (info.reached == reached) {
		return BACKUP_OK;
	}

	s_saveBuf[24] = reached;

	return backup_write(device, SAVE_FILE_NAME, "ANOTHERWLD", s_saveBuf,
	                     SAVE_MAX_BYTES, 1);
}

const char *savedataChapterName(uint16_t partId)
{
	switch (partId) {
	case 0x3E81: return "INTRO";
	case 0x3E82: return "THE ARRIVAL";
	case 0x3E83: return "THE JAIL";
	case 0x3E84: return "THE ESCAPE";
	case 0x3E85: return "THE CAVERNS";
	case 0x3E86: return "THE BATHS";
	case 0x3E87: return "THE CITY";
	case 0x3E88: return "THE ARENA";
	case 0x3E89: return "THE FINAL";
	default:     return "UNKNOWN";
	}
}
