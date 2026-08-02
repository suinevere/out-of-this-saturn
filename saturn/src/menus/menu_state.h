#ifndef MENU_STATE_H
#define MENU_STATE_H

#include "savedata.h"
#include "keymap.h"
#include "checkpoints.h"

enum MenuScreen {
	MENU_NONE,
	MENU_TITLE,
	MENU_PAUSE,
	MENU_CONFIRM,
	MENU_CONTROLS,
	MENU_OPTIONS,
	MENU_DEATH,
	MENU_LEVEL_SELECT,
	MENU_NOTICE
};

enum MenuAction {
	MENU_ACT_NONE,
	MENU_ACT_START_GAME,
	MENU_ACT_RESUME,
	MENU_ACT_SAVE_GAME,
	MENU_ACT_LOAD_GAME,
	MENU_ACT_RETURN_TO_TITLE,
	MENU_ACT_RETRY,
	MENU_ACT_SAVE_RETRY,
	MENU_ACT_SAVE_KEYMAP,
	MENU_ACT_SAVE_AND_QUIT,
	MENU_ACT_LEVEL_JUMP
};

enum {
	MENU_DIR_UP    = 1 << 0,
	MENU_DIR_DOWN  = 1 << 1,
	MENU_DIR_LEFT  = 1 << 2,
	MENU_DIR_RIGHT = 1 << 3
};

struct MenuInput {
	bool up, down, left, right, confirm, cancel, pause;
	int dirEdge;
	PadButton captured;
};

enum { MENU_CHEAT_LEN = 11 };

struct MenuState {
	MenuScreen screen;
	int cursor;
	bool confirmYes;
	bool hasContinue;
	int reached;
	MenuAction pending;

	KeyMap map;
	int capturing;
	bool mapDirty;

	MenuScreen returnScreen;
	MenuScreen optionsBack;

	bool cheatUnlocked;
	int levelChoice;

	int cheatHistory[MENU_CHEAT_LEN];
	int cheatCount;
};

enum {
	MENU_ROWS_FULL           = 5,
	MENU_ROW_SAVE            = 1,
	MENU_ROW_LOAD            = 2,
	MENU_ROW_DEATH_SAVE_QUIT = 3
};

int menuStateRowCount(const MenuState *st);

int menuStateRowAt(const MenuState *st, int cursor);

enum {
	MENU_TITLE_ROWS_FULL    = 3,
	MENU_TITLE_ROW_CONTINUE = 1,
	MENU_TITLE_ROW_OPTIONS  = 2
};

int menuStateTitleRowCount(const MenuState *st);

int menuStateTitleRowAt(const MenuState *st, int cursor);

enum {
	MENU_CONTROLS_ROWS      = KEYMAP_ROW_COUNT + 2,
	MENU_CONTROLS_ROW_RESET = KEYMAP_ROW_COUNT,
	MENU_CONTROLS_ROW_BACK  = KEYMAP_ROW_COUNT + 1
};

enum {
	MENU_OPTIONS_ROWS_FULL    = 3,
	MENU_OPTIONS_ROW_LEVELS   = 0,
	MENU_OPTIONS_ROW_CONTROLS = 1,
	MENU_OPTIONS_ROW_BACK     = 2
};

int menuStateOptionsRowCount(const MenuState *st);

int menuStateOptionsRowAt(const MenuState *st, int cursor);

void menuStateEnterTitle(MenuState *st);

void menuStateEnterPause(MenuState *st);

void menuStateEnterDeath(MenuState *st);

void menuStateEnterControls(MenuState *st, const KeyMap *map, MenuScreen back);

void menuStateEnterOptions(MenuState *st, MenuScreen back);

void menuStateEnterLevelSelect(MenuState *st, MenuScreen back, bool all);

int menuStateLevelShown(const MenuState *st);

int menuStateLevelRowCount(const MenuState *st);

int menuStateLevelRowStart(const MenuState *st, int row);

int menuStateLevelRowLen(const MenuState *st, int row);

int menuStateLevelRowOf(const MenuState *st, int idx);

MenuAction menuStateStep(MenuState *st, const MenuInput *in);

void menuStateShowSaved(MenuState *st);

#endif
