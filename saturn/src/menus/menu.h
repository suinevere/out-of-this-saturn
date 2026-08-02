#ifndef MENU_H
#define MENU_H

#include "menu_state.h"
#include "backup.h"
#include "front_end.h"
#include "game_session.h"

struct System;

void menuShowLoading(System *sys, int fadeOutFields);

void menuHoldLoading(System *sys);

void menuShowNoSpace(System *sys, const BackupSpace *space);

struct Menu : FrontEnd {
	GameSession *_session;
	System *_sys;
	uint8_t *_page;
	MenuState _st;
	SaveInfo _save;
	uint8_t _savedPal[32];
	int _statusError;
	int _settingsError;
	uint32_t _prevPad;
	uint32_t _prevRaw;
	int _repeatTimer;
	bool _loadedSlot;
	int _idleFrames;

	void refreshSave();

	bool runLoad();

	bool loadedSlot() const { return _loadedSlot; }

	void init(GameSession *session, System *sys);

	bool runTitle();

	bool runDeath(int statusError, bool scriptWaiting);

	bool runPause();

	void showLoading(int fadeOutFields);

	void holdLoading();

	void showNoSpace(const BackupSpace *space);
};

#endif
