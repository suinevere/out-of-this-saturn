#ifndef GAME_SESSION_H
#define GAME_SESSION_H

#include <stdint.h>
#include "savedata.h"

enum {
	ENGINE_SAVE_ERR_TOO_LARGE = 100,
	ENGINE_SAVE_WARN_NO_BACKGROUND = 101,
	ENGINE_SAVE_ERR_NO_CHECKPOINT = 102
};

struct GameSession {
	virtual void startNewGame() = 0;
	virtual void startAtCheckpoint(int idx) = 0;
	virtual bool loadSave() = 0;
	virtual bool saveState() = 0;
	virtual bool saveDeathCheckpoint() = 0;
	virtual void probeSave(SaveInfo *out) = 0;
	virtual bool hasPendingCode() const = 0;
	virtual int lastSaveError() const = 0;
	virtual bool lastSaveDroppedBackground() const = 0;
	virtual int reachedCheckpoints() const = 0;
	virtual bool isCheatUnlocked() const = 0;
	virtual void unlockCheat() = 0;
	virtual void beginLoadingHold() = 0;
	virtual bool softReset() = 0;
	virtual bool runIntroAttract() = 0;
	virtual void startTitleMusic() = 0;
	virtual void tickTitleMusic() = 0;
	virtual void stopTitleMusic() = 0;
	virtual void silenceAudio() = 0;
	virtual void pauseAudio() = 0;
	virtual void resumeAudio() = 0;
	virtual void setDeathRetry(bool retry) = 0;
	virtual void restoreGamePalette() = 0;
	virtual const uint8_t *frontPage() const = 0;

protected:
	~GameSession() = default;
};

#endif
