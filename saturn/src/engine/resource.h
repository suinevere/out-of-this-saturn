/* Raw - Another World Interpreter
 * Copyright (C) 2004 Gregory Montoir
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __RESOURCE_H__
#define __RESOURCE_H__

#include "intern.h"

#define MEMENTRY_STATE_END_OF_MEMLIST 0xFF
#define MEMENTRY_STATE_NOT_NEEDED 0 
#define MEMENTRY_STATE_LOADED 1
#define MEMENTRY_STATE_LOAD_ME 2

struct MemEntry {
	uint8_t state;
	uint8_t type;
	uint8_t *bufPtr;
	uint16_t unk4;
	uint8_t rankNum;
	uint8_t bankId;
	uint32_t bankOffset;
	uint16_t unkC;
	uint16_t packedSize;

	uint16_t unk10;
	uint16_t size;
};

struct Serializer;
struct Video;

struct Resource {

	enum ResType {
		RT_SOUND  = 0,
		RT_MUSIC  = 1,
		RT_POLY_ANIM = 2,

		RT_PALETTE    = 3,
		RT_BYTECODE = 4,
		RT_POLY_CINEMATIC   = 5
	};
	
	enum {
		MODULE_INSTRUMENTS = 15,
		MEM_BLOCK_SIZE = 600 * 1024
	};
	
	
	Video *video;
	const char *_dataDir;
	MemEntry _memList[150];
	uint16_t _numMemList;
	uint16_t currentPartId, requestedNextPart;
	uint8_t *_memPtrStart, *_scriptBakPtr, *_scriptCurPtr, *_vidBakPtr, *_vidCurPtr;
	bool _useSegVideo2;

	uint8_t *segPalettes;
	uint8_t *segBytecode;
	uint8_t *segCinematic;
	uint8_t *_segVideo2;

	Resource(Video *vid, const char *dataDir);
	
	void readBank(const MemEntry *me, uint8_t *dstBuf);
	void readEntries();
	void loadMarkedAsNeeded();
	void invalidateAll();
	void invalidateRes();	
	void loadPartsOrMemoryEntry(uint16_t num);
	bool loadModule(uint16_t resNum);
	void setupPart(uint16_t ptrId);
	void allocMemBlock();
	void freeMemBlock();
	
	void saveOrLoad(Serializer &ser);
};

#endif
