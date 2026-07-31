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

#ifndef __ENGINE_H__
#define __ENGINE_H__

#include "intern.h"
#include "vm.h"
#include "mixer.h"
#include "sfxplayer.h"
#include "resource.h"
#include "video.h"
#include "savedata.h"
#include "game_session.h"
#include "front_end.h"

struct System;

struct Engine final : GameSession {
	System *sys;
	FrontEnd *_front;
	VirtualMachine vm;
	Mixer mixer;
	Resource res;
	SfxPlayer player;
	Video video;
	const char *_dataDir, *_saveDir;
	int _lastSaveError;
	bool _lastSaveNoBackground;

	Engine(System *stub, const char *dataDir, const char *saveDir);
	~Engine();

	void setFrontEnd(FrontEnd *front) { _front = front; }

	void run();

	bool softReset();
	void init();
	void finish();

	uint32_t saveDevice;

	bool reserveSaveDevice(BackupSpace *space);

	bool saveState();

	bool loadSave();

	void probeSave(SaveInfo *out);

	int lastSaveError() const { return _lastSaveError; }
	bool saveDeathCheckpoint();

	bool lastSaveDroppedBackground() const { return _lastSaveNoBackground; }

	bool loadingHold;

	uint16_t pendingCode;

	bool hasPendingCode() const { return pendingCode != 0; }

	int reached;

	bool offRecord;

	bool cheatUnlocked;

	int storedReached;

	void raiseProgress(int idx);

	void flushProgress();

	void beginLoadingHold();

	int deathSaveError;

	void startNewGame();

	void startAtCheckpoint(int idx);

	bool runIntroAttract();

	bool _titleMusic;

	void startTitleMusic();

	void tickTitleMusic();

	void stopTitleMusic();

	int reachedCheckpoints() const { return reached; }
	bool isCheatUnlocked() const { return cheatUnlocked; }
	void unlockCheat() { cheatUnlocked = true; }
	void silenceAudio();
	void pauseAudio();
	void resumeAudio();
	void setDeathRetry(bool retry) { vm.deathRetry = retry; }
	void restoreGamePalette() { video.changePal(video.currentPaletteId); }
	const uint8_t *frontPage() const { return video._curPagePtr2; }
};

#endif
