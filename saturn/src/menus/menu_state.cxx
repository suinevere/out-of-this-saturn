#include "menu_state.h"

void menuStateEnterTitle(MenuState *st)
{
	st->screen = MENU_TITLE;
	st->cursor = st->hasContinue ? MENU_TITLE_ROW_CONTINUE : 0;
	st->pending = MENU_ACT_NONE;
	st->confirmYes = false;
	st->cheatCount = 0;
}

void menuStateEnterPause(MenuState *st)
{
	st->screen = MENU_PAUSE;
	st->cursor = 0;
	st->pending = MENU_ACT_NONE;
	st->confirmYes = false;
}

void menuStateEnterDeath(MenuState *st)
{
	st->screen = MENU_DEATH;
	st->cursor = 0;
	st->pending = MENU_ACT_NONE;
	st->confirmYes = false;
}

void menuStateEnterControls(MenuState *st, const KeyMap *map, MenuScreen back)
{
	st->screen = MENU_CONTROLS;
	st->cursor = 0;
	st->pending = MENU_ACT_NONE;
	st->confirmYes = false;
	st->capturing = -1;
	st->mapDirty = false;
	st->map = *map;
	st->returnScreen = back;
}

void menuStateEnterOptions(MenuState *st, MenuScreen back)
{
	st->screen = MENU_OPTIONS;
	st->cursor = 0;
	st->pending = MENU_ACT_NONE;
	st->confirmYes = false;
	st->optionsBack = back;
}

void menuStateEnterLevelSelect(MenuState *st, MenuScreen back, bool unlockAll)
{
	st->screen = MENU_LEVEL_SELECT;
	st->cursor = 0;
	st->pending = MENU_ACT_NONE;
	st->confirmYes = false;
	st->levelChoice = 0;
	st->returnScreen = back;

	if (unlockAll) {
		st->cheatUnlocked = true;
	}
}

int menuStateLevelShown(const MenuState *st)
{
	int shown = st->cheatUnlocked ? CHECKPOINT_COUNT : st->reached;

	if (shown < 1) {
		shown = 1;
	}
	if (shown > CHECKPOINT_COUNT) {
		shown = CHECKPOINT_COUNT;
	}
	return shown;
}

int menuStateLevelRowCount(const MenuState *st)
{
	const int shown = menuStateLevelShown(st);
	int rows = 0;

	for (int c = 0; c < checkpointChapterCount(); ++c) {
		const int first = checkpointChapterFirst(c);
		int len = checkpointChapterLen(c);

		if (first >= shown) {
			break;
		}
		if (first + len > shown) {
			len = shown - first;
		}
		rows += (len + 2) / 3;
	}
	return rows;
}

int menuStateLevelRowStart(const MenuState *st, int row)
{
	const int shown = menuStateLevelShown(st);
	int seen = 0;

	for (int c = 0; c < checkpointChapterCount(); ++c) {
		const int first = checkpointChapterFirst(c);
		int len = checkpointChapterLen(c);

		if (first >= shown) {
			break;
		}
		if (first + len > shown) {
			len = shown - first;
		}
		for (int k = 0; k < len; k += 3) {
			if (seen == row) {
				return first + k;
			}
			++seen;
		}
	}
	return shown;
}

int menuStateLevelRowLen(const MenuState *st, int row)
{
	const int shown = menuStateLevelShown(st);
	const int start = menuStateLevelRowStart(st, row);
	const int chapter = checkpointChapterOf(start);
	int end;

	if (start >= shown || chapter < 0) {
		return 0;
	}

	end = checkpointChapterFirst(chapter) + checkpointChapterLen(chapter);
	if (end > shown) {
		end = shown;
	}
	if (end > start + 3) {
		end = start + 3;
	}
	return end - start;
}

int menuStateLevelRowOf(const MenuState *st, int idx)
{
	const int rows = menuStateLevelRowCount(st);

	for (int row = 0; row < rows; ++row) {
		const int start = menuStateLevelRowStart(st, row);

		if (idx >= start && idx < start + menuStateLevelRowLen(st, row)) {
			return row;
		}
	}
	return -1;
}

int menuStateOptionsRowCount(const MenuState *st)
{
	(void)st;
	return MENU_OPTIONS_ROWS_FULL;
}

int menuStateOptionsRowAt(const MenuState *st, int cursor)
{
	(void)st;
	return cursor;
}

static bool menuRowVisible(const MenuState *st, int row)
{
	if (row == MENU_ROW_LOAD) {
		if (st->cheatUnlocked) {
			return true;
		}
		return st->hasContinue;
	}
	if (!st->cheatUnlocked) {
		return true;
	}
	if (row == MENU_ROW_SAVE) {
		return false;
	}
	return !(row == MENU_ROW_DEATH_SAVE_QUIT && st->screen == MENU_DEATH);
}

int menuStateRowCount(const MenuState *st)
{
	int shown = 0;

	for (int row = 0; row < MENU_ROWS_FULL; ++row) {
		if (menuRowVisible(st, row)) {
			++shown;
		}
	}
	return shown;
}

static const int DEATH_ORDER[MENU_ROWS_FULL] = {
	MENU_ROW_LOAD, 0, MENU_ROW_SAVE, MENU_ROW_DEATH_SAVE_QUIT,
	MENU_ROWS_FULL - 1
};

static const int DEATH_ORDER_UNLOCKED[MENU_ROWS_FULL] = {
	0, MENU_ROW_LOAD, MENU_ROW_SAVE, MENU_ROW_DEATH_SAVE_QUIT,
	MENU_ROWS_FULL - 1
};

int menuStateRowAt(const MenuState *st, int cursor)
{
	int seen = 0;

	for (int i = 0; i < MENU_ROWS_FULL; ++i) {
		const int row =
		    (st->screen != MENU_DEATH)
		        ? i
		        : (st->cheatUnlocked ? DEATH_ORDER_UNLOCKED[i]
		                             : DEATH_ORDER[i]);

		if (!menuRowVisible(st, row)) {
			continue;
		}
		if (seen == cursor) {
			return row;
		}
		++seen;
	}
	return MENU_ROWS_FULL - 1;
}

int menuStateTitleRowCount(const MenuState *st)
{
	return st->hasContinue ? MENU_TITLE_ROWS_FULL : MENU_TITLE_ROWS_FULL - 1;
}

int menuStateTitleRowAt(const MenuState *st, int cursor)
{
	if (!st->hasContinue && cursor >= MENU_TITLE_ROW_CONTINUE) {
		return cursor + 1;
	}
	return cursor;
}

static void menuStateAsk(MenuState *st, MenuAction action)
{
	st->returnScreen = st->screen;
	st->screen = MENU_CONFIRM;
	st->pending = action;
	st->confirmYes = (action == MENU_ACT_LOAD_GAME ||
	                  action == MENU_ACT_SAVE_GAME);
}

static MenuAction menuStateLeaveControls(MenuState *st)
{
	st->screen = st->returnScreen;
	st->cursor = 0;
	st->capturing = -1;

	if (st->mapDirty) {
		st->mapDirty = false;
		return MENU_ACT_SAVE_KEYMAP;
	}
	return MENU_ACT_NONE;
}

enum {
	CHEAT_NONE = 0,
	CHEAT_UP,
	CHEAT_DOWN,
	CHEAT_LEFT,
	CHEAT_RIGHT,
	CHEAT_B,
	CHEAT_A,
	CHEAT_START
};

static const int s_cheatSeq[MENU_CHEAT_LEN] = {
	CHEAT_UP, CHEAT_UP, CHEAT_DOWN, CHEAT_DOWN,
	CHEAT_LEFT, CHEAT_RIGHT, CHEAT_LEFT, CHEAT_RIGHT,
	CHEAT_B, CHEAT_A, CHEAT_START
};

static int cheatToken(const MenuInput *in)
{
	if (in->dirEdge & MENU_DIR_UP) {
		return CHEAT_UP;
	}
	if (in->dirEdge & MENU_DIR_DOWN) {
		return CHEAT_DOWN;
	}
	if (in->dirEdge & MENU_DIR_LEFT) {
		return CHEAT_LEFT;
	}
	if (in->dirEdge & MENU_DIR_RIGHT) {
		return CHEAT_RIGHT;
	}
	if (in->captured == PAD_B) {
		return CHEAT_B;
	}
	if (in->captured == PAD_A || in->captured == PAD_C) {
		return CHEAT_A;
	}
	if (in->pause) {
		return CHEAT_START;
	}
	return CHEAT_NONE;
}

static bool cheatAdvance(MenuState *st, const MenuInput *in)
{
	const int token = cheatToken(in);

	if (token == CHEAT_NONE) {
		return false;
	}

	if (st->cheatCount < MENU_CHEAT_LEN) {
		st->cheatHistory[st->cheatCount] = token;
		++st->cheatCount;
	} else {
		for (int i = 1; i < MENU_CHEAT_LEN; ++i) {
			st->cheatHistory[i - 1] = st->cheatHistory[i];
		}
		st->cheatHistory[MENU_CHEAT_LEN - 1] = token;
	}

	if (st->cheatCount < MENU_CHEAT_LEN) {
		return false;
	}

	for (int i = 0; i < MENU_CHEAT_LEN; ++i) {
		if (st->cheatHistory[i] != s_cheatSeq[i]) {
			return false;
		}
	}

	st->cheatCount = 0;
	return true;
}

static bool cheatEndedAnAttempt(const MenuState *st)
{
	if (st->cheatCount < 2) {
		return false;
	}
	return st->cheatHistory[st->cheatCount - 1] == CHEAT_A &&
	       st->cheatHistory[st->cheatCount - 2] == CHEAT_B;
}

static MenuAction stepTitle(MenuState *st, const MenuInput *in)
{
	if (cheatAdvance(st, in)) {
		menuStateEnterLevelSelect(st, MENU_TITLE, true);
		return MENU_ACT_NONE;
	}
	if (in->confirm && cheatEndedAnAttempt(st)) {
		return MENU_ACT_NONE;
	}

	const int rows = menuStateTitleRowCount(st);
	if (st->cursor >= rows) {
		st->cursor = rows - 1;
	}

	if (in->up) {
		st->cursor = (st->cursor + rows - 1) % rows;
		return MENU_ACT_NONE;
	}
	if (in->down) {
		st->cursor = (st->cursor + 1) % rows;
		return MENU_ACT_NONE;
	}
	if (in->confirm) {
		switch (menuStateTitleRowAt(st, st->cursor)) {
		case MENU_TITLE_ROW_CONTINUE:
			return MENU_ACT_LOAD_GAME;
		case MENU_TITLE_ROW_OPTIONS:
			menuStateEnterOptions(st, MENU_TITLE);
			return MENU_ACT_NONE;
		default:
			return MENU_ACT_START_GAME;
		}
	}
	return MENU_ACT_NONE;
}

static MenuAction stepPause(MenuState *st, const MenuInput *in)
{
	const int rows = menuStateRowCount(st);

	if (st->cursor >= rows) {
		st->cursor = rows - 1;
	}

	if (in->cancel || in->pause) {
		return MENU_ACT_RESUME;
	}
	if (in->up) {
		st->cursor = (st->cursor + rows - 1) % rows;
		return MENU_ACT_NONE;
	}
	if (in->down) {
		st->cursor = (st->cursor + 1) % rows;
		return MENU_ACT_NONE;
	}
	if (in->confirm) {
		switch (menuStateRowAt(st, st->cursor)) {
		case 0:
			return MENU_ACT_RESUME;
		case MENU_ROW_SAVE:
			if (!st->hasContinue) {
				return MENU_ACT_SAVE_GAME;
			}
			menuStateAsk(st, MENU_ACT_SAVE_GAME);
			return MENU_ACT_NONE;
		case MENU_ROW_LOAD:
			if (st->cheatUnlocked) {
				menuStateEnterLevelSelect(st, MENU_PAUSE, true);
				return MENU_ACT_NONE;
			}
			menuStateAsk(st, MENU_ACT_LOAD_GAME);
			return MENU_ACT_NONE;
		case 3:
			menuStateEnterControls(st, keymapActive(), MENU_PAUSE);
			return MENU_ACT_NONE;
		default:
			menuStateAsk(st, MENU_ACT_RETURN_TO_TITLE);
			return MENU_ACT_NONE;
		}
	}
	return MENU_ACT_NONE;
}

static MenuAction stepDeath(MenuState *st, const MenuInput *in)
{
	const int rows = menuStateRowCount(st);

	if (st->cursor >= rows) {
		st->cursor = rows - 1;
	}

	if (in->cancel) {
		return MENU_ACT_RETRY;
	}
	if (in->up) {
		st->cursor = (st->cursor + rows - 1) % rows;
		return MENU_ACT_NONE;
	}
	if (in->down) {
		st->cursor = (st->cursor + 1) % rows;
		return MENU_ACT_NONE;
	}
	if (in->confirm) {
		switch (menuStateRowAt(st, st->cursor)) {
		case 0:
			return MENU_ACT_RETRY;
		case MENU_ROW_SAVE:
			return MENU_ACT_SAVE_RETRY;
		case MENU_ROW_LOAD:
			if (st->cheatUnlocked) {
				menuStateEnterLevelSelect(st, MENU_DEATH, true);
				return MENU_ACT_NONE;
			}
			menuStateAsk(st, MENU_ACT_LOAD_GAME);
			return MENU_ACT_NONE;
		case MENU_ROW_DEATH_SAVE_QUIT:
			return MENU_ACT_SAVE_AND_QUIT;
		default:
			return MENU_ACT_RETURN_TO_TITLE;
		}
	}
	return MENU_ACT_NONE;
}

static MenuAction stepConfirm(MenuState *st, const MenuInput *in)
{
	if (in->left || in->right) {
		st->confirmYes = !st->confirmYes;
		return MENU_ACT_NONE;
	}
	if (in->cancel) {
		st->screen = st->returnScreen;
		return MENU_ACT_NONE;
	}
	if (in->confirm) {
		if (!st->confirmYes) {
			st->screen = st->returnScreen;
			return MENU_ACT_NONE;
		}
		const MenuAction action = st->pending;
		st->screen = (action == MENU_ACT_RETURN_TO_TITLE) ? MENU_TITLE
		                                                  : st->returnScreen;
		return action;
	}
	return MENU_ACT_NONE;
}

static MenuAction stepNotice(MenuState *st, const MenuInput *in)
{
	if (in->confirm || in->cancel || in->pause) {
		st->screen = st->returnScreen;
	}
	return MENU_ACT_NONE;
}

static MenuAction stepControls(MenuState *st, const MenuInput *in)
{
	if (st->capturing >= 0) {
		if (in->pause) {
			st->capturing = -1;
			return MENU_ACT_NONE;
		}
		if (in->captured != PAD_NONE) {
			if (keymapAssign(&st->map, (KeymapRow)st->capturing, in->captured)) {
				st->mapDirty = true;
			}
			st->capturing = -1;
		}
		return MENU_ACT_NONE;
	}

	if (in->up) {
		st->cursor = (st->cursor + MENU_CONTROLS_ROWS - 1) % MENU_CONTROLS_ROWS;
		return MENU_ACT_NONE;
	}
	if (in->down) {
		st->cursor = (st->cursor + 1) % MENU_CONTROLS_ROWS;
		return MENU_ACT_NONE;
	}
	if (in->cancel) {
		return menuStateLeaveControls(st);
	}
	if (in->confirm) {
		if (st->cursor < KEYMAP_ROW_COUNT) {
			st->capturing = st->cursor;
			return MENU_ACT_NONE;
		}
		if (st->cursor == MENU_CONTROLS_ROW_RESET) {
			KeyMap before = st->map;
			keymapDefaults(&st->map);
			for (int i = 0; i < KEYMAP_ROW_COUNT; ++i) {
				if (before.row[i] != st->map.row[i]) {
					st->mapDirty = true;
				}
			}
			return MENU_ACT_NONE;
		}
		return menuStateLeaveControls(st);
	}
	return MENU_ACT_NONE;
}

static MenuAction stepLevelSelect(MenuState *st, const MenuInput *in)
{
	const int shown = menuStateLevelShown(st);

	if (shown <= 0) {
		st->screen = st->returnScreen;
		return MENU_ACT_NONE;
	}
	if (st->cursor >= shown) {
		st->cursor = shown - 1;
	}

	if (in->cancel) {
		st->screen = st->returnScreen;
		return MENU_ACT_NONE;
	}
	if (in->left) {
		st->cursor = (st->cursor + shown - 1) % shown;
		return MENU_ACT_NONE;
	}
	if (in->right) {
		st->cursor = (st->cursor + 1) % shown;
		return MENU_ACT_NONE;
	}
	if (in->up || in->down) {
		const int row = menuStateLevelRowOf(st, st->cursor);
		const int col = st->cursor - menuStateLevelRowStart(st, row);
		const int want = in->up ? row - 1 : row + 1;

		if (want >= 0 && want < menuStateLevelRowCount(st)) {
			const int len = menuStateLevelRowLen(st, want);
			const int use = (col < len) ? col : len - 1;
			st->cursor = menuStateLevelRowStart(st, want) + use;
		}
		return MENU_ACT_NONE;
	}
	if (in->confirm) {
		st->levelChoice = st->cursor;
		st->screen = st->returnScreen;
		return MENU_ACT_LEVEL_JUMP;
	}
	return MENU_ACT_NONE;
}

static MenuAction stepOptions(MenuState *st, const MenuInput *in)
{
	const int rows = menuStateOptionsRowCount(st);
	const MenuScreen back = st->optionsBack;

	if (st->cursor >= rows) {
		st->cursor = rows - 1;
	}

	if (in->cancel) {
		st->screen = back;
		return MENU_ACT_NONE;
	}
	if (in->up) {
		st->cursor = (st->cursor + rows - 1) % rows;
		return MENU_ACT_NONE;
	}
	if (in->down) {
		st->cursor = (st->cursor + 1) % rows;
		return MENU_ACT_NONE;
	}
	if (in->confirm) {
		switch (menuStateOptionsRowAt(st, st->cursor)) {
		case MENU_OPTIONS_ROW_LEVELS:
			menuStateEnterLevelSelect(st, MENU_OPTIONS, false);
			return MENU_ACT_NONE;
		case MENU_OPTIONS_ROW_CONTROLS:
			menuStateEnterControls(st, keymapActive(), MENU_OPTIONS);
			return MENU_ACT_NONE;
		default:
			st->screen = back;
			return MENU_ACT_NONE;
		}
	}
	return MENU_ACT_NONE;
}

MenuAction menuStateStep(MenuState *st, const MenuInput *in)
{
	switch (st->screen) {
	case MENU_TITLE:
		return stepTitle(st, in);
	case MENU_PAUSE:
		return stepPause(st, in);
	case MENU_CONFIRM:
		return stepConfirm(st, in);
	case MENU_CONTROLS:
		return stepControls(st, in);
	case MENU_OPTIONS:
		return stepOptions(st, in);
	case MENU_DEATH:
		return stepDeath(st, in);
	case MENU_LEVEL_SELECT:
		return stepLevelSelect(st, in);
	case MENU_NOTICE:
		return stepNotice(st, in);
	default:
		return MENU_ACT_NONE;
	}
}

void menuStateShowSaved(MenuState *st)
{
	st->returnScreen = st->screen;
	st->screen = MENU_NOTICE;
}
