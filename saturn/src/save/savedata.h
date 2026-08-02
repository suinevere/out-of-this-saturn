#ifndef SAVEDATA_H
#define SAVEDATA_H

#include "backup.h"
#include "intern.h"

#define SAVE_FILE_NAME "AW_SAVE"

enum {
	SAVE_HEADER_SIZE = 48,
	SAVE_MAX_BYTES   = 8192
};

enum {
	SAVE_HAS_STATE = 1 << 0,
	SAVE_HAS_CODE  = 1 << 1
};

enum {
	SAVE_FRAME_NONE     = 0,
	SAVE_FRAME_RLE      = 1,
	SAVE_FRAME_DELTA    = 2,
	SAVE_FRAME_DELTA_H2 = 3,
	SAVE_FRAME_DELTA_H4 = 4,
	SAVE_FRAME_DELTA_H8 = 5
};

inline int savedataFrameRowStep(int kind)
{
	if (kind == SAVE_FRAME_DELTA) {
		return 1;
	}
	if (kind == SAVE_FRAME_DELTA_H2) {
		return 2;
	}
	if (kind == SAVE_FRAME_DELTA_H4) {
		return 4;
	}
	if (kind == SAVE_FRAME_DELTA_H8) {
		return 8;
	}
	return 0;
}

enum SaveFileState {
	SAVE_FILE_NONE,
	SAVE_FILE_OK,
	SAVE_FILE_DAMAGED,
	SAVE_FILE_OLD_VERSION
};

struct SaveInfo {
	SaveFileState state;
	bool          hasState;
	bool          hasCode;
	uint16_t      statePartId;
	uint32_t      stateDate;
	uint16_t      stateLen;
	uint16_t      codePartId;
	uint32_t      codeDate;
	uint16_t      codeWord;
	uint8_t       reached;
};

void savedataClear(SaveInfo *info);

void savedataWriteHeader(uint8_t *buf, const SaveInfo *info);

bool savedataReadHeader(const uint8_t *buf, uint16_t *ver, SaveInfo *info);

SaveFileState savedataProbe(uint32_t device, SaveInfo *out);

bool savedataHasAny(const SaveInfo *info);

bool savedataNewestIsCode(const SaveInfo *info);

uint32_t savedataBlocksNeeded(uint32_t blockSize, int32_t bytes);

int savedataReserve(uint32_t device, BackupSpace *space);

int savedataStoreProgress(uint32_t device, uint8_t reached);

uint8_t *savedataBuffer(void);

const char *savedataChapterName(uint16_t partId);

#endif
