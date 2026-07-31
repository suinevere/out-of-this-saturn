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

#include "engine.h"
#include "file.h"
#include "serializer.h"
#include "sys.h"
#include "parts.h"
#include "savedata.h"
#include "checkpoints.h"
#include "menu_input.h"
#include "opening.h"
#include "page_rle.h"
#include "fade.h"
#include "settings.h"
#include "keymap.h"
#include "clock.h"
#include "diag.h"
#include "cd_music.h"
#include "boot.h"
#include "part_music.h"

#define OPENING_DIAG 0

#if OPENING_DIAG
#define OPENING_SAY(row, fmt, a, b, c, d, e) diag_row(row, fmt, a, b, c, d, e)
#else
#define OPENING_SAY(row, fmt, a, b, c, d, e) ((void)0)
#endif

#define VM_DEATH_PROMPT_HOLD_MS 2000

#define ENGINE_CODE_SETTLE_FRAMES 60
#define ENGINE_CODE_CHAR_GAP      8
#define ENGINE_CODE_GIVE_UP       150
#define ENGINE_CODE_CONFIRM       0x0D

enum {
	ENGINE_CODE_OFF = 0,
	ENGINE_CODE_SETTLING,
	ENGINE_CODE_TYPING
};

Engine::Engine(System *paramSys, const char *dataDir, const char *saveDir)
	: sys(paramSys), _front(0), vm(&mixer, &res, &player, &video, sys), mixer(sys), res(&video, dataDir),
	player(&mixer, &res, sys), video(&res, sys), _dataDir(dataDir), _saveDir(saveDir), _lastSaveError(BACKUP_OK),
	_lastSaveNoBackground(false), _titleMusic(false) {
}

static const char *engineCodeWord(uint16_t stringId) {
	const StrEntry *se = Video::_stringsTableEng;

	while (se->id != END_OF_STRING_DICTIONARY && se->id != stringId) {
		++se;
	}

	return (se->id == END_OF_STRING_DICTIONARY) ? 0 : se->str;
}

void Engine::run() {
	BOOT_SAY(9);

	BOOT_SAY(10);

	while (!sys->input.quit) {

		offRecord = false;
		flushProgress();
		if (!_front->runTitle()) {
			continue;
		}

		bool playing = true;
		bool pauseLatched = false;
		int lit = 0;

		int codeStage = ENGINE_CODE_OFF;
		int codeTimer = 0;
		int codeChar = 0;
		int codeWaited = 0;
		bool codeConfirmed = false;
		const char *codeText = 0;

		while (playing && !sys->input.quit) {

			if (vm.deathScreen && !video._holdDisplay) {
				fade_ramp(FADE_DARK, 2);
				video._holdDisplay = true;
			}

			const bool partChange = (res.requestedNextPart != 0 &&
			                         !video._holdDisplay);
			if (partChange) {
				_front->showLoading(OPENING_FADE_SKIP_FIELDS);
			}

			vm.checkThreadRequests();

			if (partChange) {
				beginLoadingHold();
				flushProgress();
			}

			vm.inp_updatePlayer();

			if (pendingCode != 0 && codeStage == ENGINE_CODE_OFF) {
				codeText = engineCodeWord(pendingCode);
				pendingCode = 0;
				if (codeText != 0) {
					vm.codeEntryActive = true;
					vm.initForPart(GAME_PART_LAST);
					res.requestedNextPart = 0;

					codeStage = ENGINE_CODE_SETTLING;
					codeTimer = ENGINE_CODE_SETTLE_FRAMES;
					codeChar = 0;
					codeWaited = 0;
					codeConfirmed = false;
					beginLoadingHold();
				}
			}

			if (codeStage == ENGINE_CODE_SETTLING) {
				if (--codeTimer <= 0) {
					codeStage = ENGINE_CODE_TYPING;
					codeTimer = 0;
				}
			} else if (codeStage == ENGINE_CODE_TYPING) {
				if (res.currentPartId != GAME_PART_LAST) {
					codeStage = ENGINE_CODE_OFF;
					vm.codeEntryActive = false;
					beginLoadingHold();
				} else if (codeTimer > 0) {
					--codeTimer;
				} else if (codeText[codeChar] != 0) {
					sys->input.lastChar =
					    (char)(codeText[codeChar] | 0x20);
					++codeChar;
					codeTimer = ENGINE_CODE_CHAR_GAP;
				} else if (!codeConfirmed) {
					sys->input.lastChar = ENGINE_CODE_CONFIRM;
					codeConfirmed = true;
					codeTimer = ENGINE_CODE_CHAR_GAP;
				} else if (++codeWaited > ENGINE_CODE_GIVE_UP) {
					codeStage = ENGINE_CODE_OFF;
					vm.codeEntryActive = false;
					loadingHold = false;
					video._holdDisplay = false;
					playing = false;
					continue;
				}
			}

			if (sys->input.resetRequest) {
				sys->input.resetRequest = false;
				video._holdDisplay = false;
				softReset();
				playing = false;
				continue;
			}

			if (!sys->input.pause) {
				pauseLatched = false;
			} else if (!pauseLatched) {
				pauseLatched = true;
				lit = OPENING_FADE_VM_FRAMES;
				fade_set(FADE_LIT);
				playing = _front->runPause();

				if (_front->loadedSlot()) {
					lit = 0;
				}
				continue;
			}

			vm.hostFrame();

			raiseProgress(checkpointOfOrdinal(
			    (uint16_t)vm.vmVariables[VM_VARIABLE_CHECKPOINT]));
			cd_music_tick();

			if (loadingHold && codeStage == ENGINE_CODE_OFF) {
				if (video._displayRequested) {
					loadingHold = false;
					fade_ramp(FADE_DARK, OPENING_FADE_SKIP_FIELDS);
					video.changePal(video.currentPaletteId);
					video._holdDisplay = false;
					lit = 0;
				} else {
					_front->holdLoading();
				}
			} else if (loadingHold) {
				_front->holdLoading();
			}

			if (vm.deathPrompt || deathSaveError != BACKUP_OK) {
				const bool scriptWaiting = vm.deathPrompt;
				const int pending = deathSaveError;
				vm.deathPrompt = false;
				deathSaveError = BACKUP_OK;
				vm.deathPromptUntil = sys->getTimeStamp() + VM_DEATH_PROMPT_HOLD_MS;
				lit = 0;
				flushProgress();
				playing = _front->runDeath(pending, scriptWaiting);
				vm.deathScreen = false;

				video._holdDisplay = false;
				continue;
			}

			if (!video._holdDisplay && lit < OPENING_FADE_VM_FRAMES) {
				lit++;
				fade_set((FADE_LIT * lit) / OPENING_FADE_VM_FRAMES);
			}
		}

		video._holdDisplay = false;
		loadingHold = false;
		pendingCode = 0;
	}

}

Engine::~Engine(){

	finish();
	sys->destroy();
}

bool Engine::softReset() {
	player.stop();
	cd_music_stop();
	mixer.stopAll();

#if defined(OOTW_SATURN) && CHAINBOOT_ENABLED
	if (chainboot_available()) {
		chainboot_run();
	}
#endif

	return false;
}

void Engine::init() {

	loadingHold = false;
	pendingCode = 0;
	deathSaveError = BACKUP_OK;
	_lastSaveNoBackground = false;
	saveDevice = 0;

	sys->init("Out Of This World");
	BOOT_SAY(2);

#ifdef OOTW_SATURN
	backup_init();
	{
		Settings s;
		settingsDefaults(&s);
		settingsLoad(&s);
		keymapSetActive(&s.map);
	}
#endif
	BOOT_SAY(3);

	video.init();
	BOOT_SAY(4);

#ifdef OOTW_SATURN
	{
		BackupSpace space;
		if (!reserveSaveDevice(&space)) {
			_front->showNoSpace(&space);
		}
	}
#endif
	BOOT_SAY(5);

	SaveInfo info;
	reached = 0;
	offRecord = false;
	cheatUnlocked = false;
	if (saveDevice != 0 && savedataProbe(saveDevice, &info) == SAVE_FILE_OK) {
		reached = info.reached;
		if (reached > CHECKPOINT_COUNT) {
			reached = CHECKPOINT_COUNT;
		}
	}
	storedReached = reached;

	res.allocMemBlock();
	BOOT_SAY(6);

	res.readEntries();
	BOOT_SAY(7);

	vm.init();

	mixer.init();

	player.init();

	cd_music_init();
	BOOT_SAY(8);
}

void Engine::finish() {
	player.free();
	mixer.free();
	res.freeMemBlock();
}

void Engine::probeSave(SaveInfo *out) {
	if (saveDevice == 0) {
		savedataClear(out);
		out->state = SAVE_FILE_NONE;
		return;
	}

	savedataProbe(saveDevice, out);
}

bool Engine::reserveSaveDevice(BackupSpace *space) {
	BackupSpace internalSpace;
	BackupSpace cartSpace;

	memset(&internalSpace, 0, sizeof(internalSpace));
	memset(&cartSpace, 0, sizeof(cartSpace));

	saveDevice = 0;

	const int internalRc = savedataReserve(BACKUP_INTERNAL, &internalSpace);
	if (internalRc == BACKUP_OK) {
		saveDevice = BACKUP_INTERNAL;
		*space = internalSpace;
		return true;
	}

	const int cartRc = savedataReserve(BACKUP_CART, &cartSpace);
	if (cartRc == BACKUP_OK) {
		saveDevice = BACKUP_CART;
		*space = cartSpace;
		return true;
	}

	*space = (internalSpace.blockSize != 0) ? internalSpace : cartSpace;
	return false;
}

bool Engine::saveState() {
	if (saveDevice == 0) {
		_lastSaveError = BACKUP_ERR_NO_SPACE;
		return false;
	}

	SaveInfo info;
	probeSave(&info);
	if (info.state != SAVE_FILE_OK) {
		savedataClear(&info);
	}

	uint8_t *buf = savedataBuffer();
	memset(buf, 0, SAVE_MAX_BYTES);

	File f;
	f.openMemory(buf + SAVE_HEADER_SIZE, SAVE_MAX_BYTES - SAVE_HEADER_SIZE, true);
	Serializer s(&f, Serializer::SM_SAVE, res._memPtrStart);
	vm.saveOrLoad(s);
	res.saveOrLoad(s);
	video.saveOrLoad(s);
	player.saveOrLoad(s);
	mixer.saveOrLoad(s);

	if (f.ioErr()) {
		_lastSaveError = ENGINE_SAVE_ERR_TOO_LARGE;
		return false;
	}

	const int32_t framePos = (int32_t)(SAVE_HEADER_SIZE + f.tell()) + 4;
	const int32_t frameCap = (int32_t)SAVE_MAX_BYTES - framePos;

	static const int s_frameSteps[4] = { 1, 2, 4, 8 };
	static const uint16_t s_frameKinds[4] = { SAVE_FRAME_DELTA,
	                                          SAVE_FRAME_DELTA_H2,
	                                          SAVE_FRAME_DELTA_H4,
	                                          SAVE_FRAME_DELTA_H8 };

	int32_t frameLen = -1;
	uint16_t frameKind = SAVE_FRAME_NONE;

	for (int i = 0; i < 4 && frameLen < 0; ++i) {
		frameLen = pageDeltaEncode(video._pages[0], Video::VID_PAGE_SIZE,
		                           Video::VID_PAGE_STRIDE, s_frameSteps[i],
		                           buf + framePos, frameCap);
		if (frameLen > 0) {
			frameKind = s_frameKinds[i];
		}
	}

	if (frameLen < 0) {
		frameLen = 0;
		frameKind = SAVE_FRAME_NONE;
	}
	_lastSaveNoBackground = (frameLen == 0);
	f.writeUint16BE(frameKind);
	f.writeUint16BE((uint16_t)frameLen);

	info.hasState = true;
	info.statePartId = res.currentPartId;
	info.stateDate = backup_date_now();
	info.stateLen = (uint16_t)(f.tell() + frameLen);
	savedataWriteHeader(buf, &info);

	_lastSaveError = backup_write(saveDevice, SAVE_FILE_NAME, "ANOTHERWLD",
	                               buf, SAVE_MAX_BYTES, 1);
	return _lastSaveError == BACKUP_OK;
}

bool Engine::saveDeathCheckpoint() {
	if (vm.deathCodeString == 0) {
		_lastSaveError = ENGINE_SAVE_ERR_NO_CHECKPOINT;
		return false;
	}
	if (saveDevice == 0) {
		_lastSaveError = BACKUP_ERR_NO_SPACE;
		return false;
	}

	SaveInfo info;
	probeSave(&info);

	uint8_t *buf = savedataBuffer();

	if (info.state != SAVE_FILE_OK) {
		savedataClear(&info);
		memset(buf, 0, SAVE_MAX_BYTES);
	}

	info.hasCode = true;
	info.codePartId = res.currentPartId;
	info.codeDate = backup_date_now();
	info.codeWord = vm.deathCodeString;
	savedataWriteHeader(buf, &info);

	_lastSaveNoBackground = false;
	_lastSaveError = backup_write(saveDevice, SAVE_FILE_NAME, "ANOTHERWLD",
	                               buf, SAVE_MAX_BYTES, 1);
	return _lastSaveError == BACKUP_OK;
}

bool Engine::loadSave() {
	if (saveDevice == 0) {
		_lastSaveError = BACKUP_ERR_NOT_FOUND;
		return false;
	}

	SaveInfo info;
	const SaveFileState state = savedataProbe(saveDevice, &info);

	if (state == SAVE_FILE_NONE) {
		_lastSaveError = BACKUP_ERR_NOT_FOUND;
		return false;
	}
	if (state != SAVE_FILE_OK || !savedataHasAny(&info)) {
		_lastSaveError = BACKUP_ERR_BROKEN;
		return false;
	}

	player.stop();
	mixer.stopAll();

	if (savedataNewestIsCode(&info)) {
		vm.vmVariables[VM_VARIABLE_CHECKPOINT] = 0;
		if (checkpointOfStringId(info.codeWord) + 1 > reached) {
			offRecord = true;
		}
		pendingCode = info.codeWord;
		_lastSaveError = BACKUP_OK;
		return true;
	}

	uint8_t *buf = savedataBuffer();

	File f;
	f.openMemory(buf + SAVE_HEADER_SIZE, SAVE_MAX_BYTES - SAVE_HEADER_SIZE, false);
	Serializer s(&f, Serializer::SM_LOAD, res._memPtrStart, Serializer::CUR_VER);
	vm.saveOrLoad(s);
	res.saveOrLoad(s);
	video.saveOrLoad(s);
	player.saveOrLoad(s);
	mixer.saveOrLoad(s);

	if (f.ioErr()) {
		_lastSaveError = BACKUP_ERR_BROKEN;
		return false;
	}

	cd_music_set_part_resumed(res.currentPartId);

	const uint16_t frameKind = f.readUint16BE();
	const uint16_t frameLen = f.readUint16BE();

	if (!f.ioErr() && frameKind != SAVE_FRAME_NONE && frameLen != 0) {
		const int32_t framePos = (int32_t)(SAVE_HEADER_SIZE + f.tell());
		bool decoded = false;

		if (framePos + frameLen <= (int32_t)SAVE_MAX_BYTES) {
			const int rowStep = savedataFrameRowStep((int)frameKind);
			if (rowStep > 0) {
				decoded = pageDeltaDecode(buf + framePos, frameLen,
				                          video._pages[0], Video::VID_PAGE_SIZE,
				                          Video::VID_PAGE_STRIDE, rowStep);
			} else {
				decoded = pageRleDecode(buf + framePos, frameLen,
				                        video._pages[0], Video::VID_PAGE_SIZE);
			}
		}

		if (decoded) {
			for (int i = 1; i < 4; ++i) {
				memcpy(video._pages[i], video._pages[0], Video::VID_PAGE_SIZE);
			}
		}
	}

	_lastSaveError = BACKUP_OK;
	return true;
}

void Engine::raiseProgress(int idx) {
	if (offRecord || cheatUnlocked || idx < 0) {
		return;
	}
	if (idx + 1 > reached) {
		reached = idx + 1;
	}
}

void Engine::flushProgress() {
	if (cheatUnlocked) {
		return;
	}
	if (saveDevice == 0 || reached == storedReached) {
		return;
	}
	if (savedataStoreProgress(saveDevice, (uint8_t)reached) == BACKUP_OK) {
		storedReached = reached;
	}
}

void Engine::beginLoadingHold() {
	video._displayRequested = false;
	video._holdDisplay = true;
	loadingHold = true;
}

void Engine::startNewGame() {
	vm.vmVariables[VM_VARIABLE_CHECKPOINT] = 0;
#ifdef BYPASS_PROTECTION
	vm.initForPart(GAME_PART2);
#else
	vm.initForPart(GAME_PART1);
#endif
}

void Engine::startAtCheckpoint(int idx) {
	player.stop();
	mixer.stopAll();

	vm.vmVariables[VM_VARIABLE_CHECKPOINT] = 0;

	if (idx > 0 && idx + 1 > reached) {
		offRecord = true;
	}
	pendingCode = checkpointStringId(idx);
}

bool Engine::runIntroAttract() {
	BOOT_SAY(11);
	_front->showLoading(OPENING_FADE_SKIP_FIELDS);

	vm.initForPart(GAME_PART2);
	res.requestedNextPart = 0;
	BOOT_SAY(12);

	beginLoadingHold();

	bool finished = false;
	int lit = 0;
	bool vmParked = false;
	uint32_t audibleAt = 0;
	int vmFrames = 0;
	int askedOnEntry = -1;
	const uint32_t holdBegan = clock_ms();

	OPENING_SAY(24, "open: enter", 0, 0, 0, 0, 0);
	int diagTick = 0;

	while (!sys->input.quit) {
		if (res.requestedNextPart != 0) {
			finished = true;
			break;
		}

		vm.checkThreadRequests();
		vm.inp_updatePlayer();

		if (menuInputBits(sys) != 0) {
			break;
		}

		if (!vmParked) {
			vm.hostFrame();

			if (loadingHold) {
				vmFrames++;
			}
		}

		cd_music_tick();

		if (loadingHold) {
			const bool capped =
			    (uint32_t)(clock_ms() - holdBegan) >= OPENING_CDDA_HOLD_CAP_MS;
			const bool asked = cd_music_is_playing() != 0;

			if (askedOnEntry < 0) {
				askedOnEntry = asked ? 1 : 0;
			}

			if (cd_music_audible() && audibleAt == 0) {
				audibleAt = clock_ms();

				if (audibleAt == 0) {
					audibleAt = 1;
				}
			}

			const int32_t trackNow = cd_music_track_ms();
			const bool sceneDue =
			    OPENING_USE_MODULE ||
			    trackNow >= (int32_t)OPENING_CDDA_TRACK_START_MS;

			if (++diagTick >= 60) {
				diagTick = 0;
				OPENING_SAY(25, "open: req%d hold%d ask%d aud%d ms%d",
				            video._displayRequested ? 1 : 0,
				            video._holdDisplay ? 1 : 0,
				            asked ? 1 : 0,
				            cd_music_audible() ? 1 : 0,
				            (int)cd_music_ms_since_cue());
			}

			vmParked = video._displayRequested && asked && !sceneDue && !capped;

			const bool ready =
			    OPENING_USE_MODULE ||
			    !cd_music_available() || capped || (asked && sceneDue);

			if (video._displayRequested && ready) {
				OPENING_SAY(19, "open: vmf%d askIn%d", vmFrames, askedOnEntry,
				            0, 0, 0);

				OPENING_SAY(26, "open: pic %d after ask, music up %d",
				            (int)cd_music_ms_since_cue(),
				            audibleAt != 0
				                ? (int)(clock_ms() - audibleAt)
				                : -1,
				            0, 0, 0);
				loadingHold = false;
				vmParked = false;

				fade_audio_follow(0);
				fade_ramp(FADE_DARK, OPENING_FADE_SKIP_FIELDS);
				video.changePal(video.currentPaletteId);
				video._holdDisplay = false;
				lit = 0;
			} else {
				_front->holdLoading();
			}
			continue;
		}

		if (lit < OPENING_FADE_VM_FRAMES) {
			lit++;
			fade_set((FADE_LIT * lit) / OPENING_FADE_VM_FRAMES);
		}
	}

	res.requestedNextPart = 0;
	loadingHold = false;
	video._holdDisplay = false;

	fade_audio_follow(1);

	player.stop();
	mixer.stopAll();

	cd_music_stop();

	fade_ramp(FADE_DARK,
	              finished ? OPENING_FADE_FIELDS : OPENING_FADE_SKIP_FIELDS);

	return finished;
}

#define TITLE_MUSIC_RES 0x07

#define TITLE_MUSIC_DELAY 15700

#define TITLE_MUSIC_SKIP_ROWS 64

void Engine::startTitleMusic() {
	if (_titleMusic || cd_music_available()) {
		return;
	}

	silenceAudio();
	if (!res.loadModule(TITLE_MUSIC_RES)) {
		return;
	}

	_titleMusic = true;
	player.playModule(TITLE_MUSIC_RES, TITLE_MUSIC_DELAY, TITLE_MUSIC_SKIP_ROWS);
}

void Engine::tickTitleMusic() {
	if (_titleMusic && player.resNum() == 0) {
		player.playModule(TITLE_MUSIC_RES, TITLE_MUSIC_DELAY, TITLE_MUSIC_SKIP_ROWS);
	}
}

void Engine::stopTitleMusic() {
	if (!_titleMusic) {
		return;
	}

	_titleMusic = false;
	silenceAudio();
	res.invalidateRes();
}

void Engine::silenceAudio() {
	player.stop();
	mixer.stopAll();
}

void Engine::pauseAudio() {
	player.pause();
	mixer.stopAll();
}

void Engine::resumeAudio() {
	player.resume();
}
