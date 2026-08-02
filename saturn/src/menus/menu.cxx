#include "menu.h"
#include "game_session.h"
#include "sys.h"
#include "video.h"
#include "menu_draw.h"
#include "menu_blit.h"
#include "menu_art.h"
#include "menu_input.h"
#include "savedata.h"
#include "checkpoints.h"
#include "settings.h"
#include "keymap.h"
#include "backup.h"
#include "boot.h"
#include "display.h"
#include "fade.h"
#include "cd_music.h"
#include "opening.h"

extern "C" {
#include <string.h>
}

static uint8_t s_menuPage[MENU_PAGE_SIZE];

enum {
	MENU_BASE_DIM    = 8,
	MENU_BASE_SEL    = 12,
	MENU_COL_PANEL   = 0,
	MENU_COL_BORDER  = 7
};

enum {
	MENU_REPEAT_DELAY = 20,
	MENU_REPEAT_RATE  = 4
};

enum {
	MENU_TITLE_IDLE_FRAMES = 900
};

enum {
	MENU_TITLE_CUE = 24
};

enum {
	MENU_TITLE_START_Y  = 130,
	MENU_TITLE_ROW_STEP = 24,

	MENU_TITLE_STATUS_Y = 114
};

enum {
	MENU_PAD_UP      = 1 << 0,
	MENU_PAD_DOWN    = 1 << 1,
	MENU_PAD_LEFT    = 1 << 2,
	MENU_PAD_RIGHT   = 1 << 3,
	MENU_PAD_CONFIRM = 1 << 4,
	MENU_PAD_CANCEL  = 1 << 5,
	MENU_PAD_PAUSE   = 1 << 6,
	MENU_PAD_L       = 1 << 7,
	MENU_PAD_R       = 1 << 8,
	MENU_PAD_NAV     = MENU_PAD_UP | MENU_PAD_DOWN | MENU_PAD_LEFT | MENU_PAD_RIGHT,
	MENU_PAD_EDGE    = MENU_PAD_CONFIRM | MENU_PAD_CANCEL | MENU_PAD_PAUSE |
	                   MENU_PAD_L | MENU_PAD_R
};

static void menuAppendChar(char *dst, int cap, int *pos, char c) {
	if (*pos < cap - 1) {
		dst[*pos] = c;
		(*pos)++;
		dst[*pos] = 0;
	}
}

static void menuAppendStr(char *dst, int cap, int *pos, const char *s) {
	while (*s != 0) {
		menuAppendChar(dst, cap, pos, *s);
		++s;
	}
}

static void menuAppendPad2(char *dst, int cap, int *pos, int v) {
	if (v < 0) {
		v = 0;
	}
	if (v > 99) {
		v = 99;
	}
	menuAppendChar(dst, cap, pos, (char)('0' + (v / 10)));
	menuAppendChar(dst, cap, pos, (char)('0' + (v % 10)));
}

static const char *menuStatusText(int err, uint32_t device) {
	switch (err) {
	case BACKUP_OK:
		return 0;
	case ENGINE_SAVE_ERR_TOO_LARGE:
		return "SAVE STATE TOO LARGE";
	case BACKUP_ERR_NONE:
		return "NO BACKUP DEVICE";
	case BACKUP_ERR_UNFORMAT:
		return (device == BACKUP_CART) ? "CARTRIDGE UNFORMATTED"
		                                : "BACKUP RAM UNFORMATTED";
	case BACKUP_ERR_PROTECTED:
		return (device == BACKUP_CART) ? "CARTRIDGE WRITE PROTECTED"
		                                : "WRITE PROTECTED";
	case BACKUP_ERR_NO_SPACE:
		return "NOT ENOUGH SPACE";
	case BACKUP_ERR_NOT_FOUND:
		return "SAVE NOT FOUND";
	case BACKUP_ERR_EXISTS:
		return "SAVE FILE IN USE";
	case ENGINE_SAVE_WARN_NO_BACKGROUND:
		return "SAVED WITHOUT BACKGROUND";
	case ENGINE_SAVE_ERR_NO_CHECKPOINT:
		return "NO CHECKPOINT YET";
	case BACKUP_ERR_BROKEN:
		return "SAVE DATA DAMAGED";
	default:
		return "SAVE FAILED";
	}
}

static void menuSaveLine(char *out, int cap, const SaveInfo *info) {
	int pos = 0;
	out[0] = 0;

	if (!savedataHasAny(info)) {
		return;
	}

	const bool code = savedataNewestIsCode(info);
	const uint16_t partId = code ? info->codePartId : info->statePartId;
	const uint32_t date = code ? info->codeDate : info->stateDate;

	menuAppendStr(out, cap, &pos, savedataChapterName(partId));
	menuAppendStr(out, cap, &pos, "  ");

	int month = 0;
	int day = 0;
	int hour = 0;
	int minute = 0;
	backup_date_split(date, &month, &day, &hour, &minute);

	menuAppendPad2(out, cap, &pos, month);
	menuAppendChar(out, cap, &pos, '/');
	menuAppendPad2(out, cap, &pos, day);
	menuAppendChar(out, cap, &pos, ' ');
	menuAppendPad2(out, cap, &pos, hour);
	menuAppendChar(out, cap, &pos, ':');
	menuAppendPad2(out, cap, &pos, minute);
}

void Menu::init(GameSession *session, System *sys) {
	_session = session;
	_sys = sys;
	_page = s_menuPage;
	_statusError = BACKUP_OK;
	_settingsError = BACKUP_OK;
	_prevPad = 0;
	_prevRaw = 0;
	_repeatTimer = 0;
	_loadedSlot = false;
	_idleFrames = 0;

	memset(&_st, 0, sizeof(_st));
	memset(_savedPal, 0, sizeof(_savedPal));
	savedataClear(&_save);
	_save.state = SAVE_FILE_NONE;

	_st.capturing = -1;
}

void Menu::refreshSave() {
	_session->probeSave(&_save);
	_st.hasContinue = savedataHasAny(&_save);
	_st.reached = _session->reachedCheckpoints();

	if (_session->isCheatUnlocked()) {
		_st.cheatUnlocked = true;
	}
}

static uint32_t menuPadMask(System *sys) {
	sys->processEvents();
	return menuInputBits(sys);
}

static void menuPrimeEdges(System *sys, uint32_t *prevPad, int *repeatTimer,
                           uint32_t *prevRaw) {
	*prevPad = menuPadMask(sys);
	*prevRaw = sys->input.padMask;
	*repeatTimer = 0;
}

static void menuPollEdges(System *sys, uint32_t *prevPad, int *repeatTimer,
                          uint32_t *prevRaw, MenuInput *out) {
	const uint32_t now = menuPadMask(sys);

	const uint32_t nav = now & MENU_PAD_NAV;
	const uint32_t prevNav = *prevPad & MENU_PAD_NAV;
	uint32_t fired = nav & ~prevNav;

	if (fired != 0) {
		*repeatTimer = MENU_REPEAT_DELAY;
	} else if (nav != 0 && nav == prevNav) {
		(*repeatTimer)--;
		if (*repeatTimer <= 0) {
			fired = nav;
			*repeatTimer = MENU_REPEAT_RATE;
		}
	} else if (nav == 0) {
		*repeatTimer = 0;
	}

	const uint32_t justPressed = now & ~(*prevPad);

	fired |= justPressed & MENU_PAD_EDGE;
	*prevPad = now;

	out->up      = (fired & MENU_PAD_UP) != 0;
	out->down    = (fired & MENU_PAD_DOWN) != 0;
	out->left    = (fired & (MENU_PAD_LEFT | MENU_PAD_L)) != 0;
	out->right   = (fired & (MENU_PAD_RIGHT | MENU_PAD_R)) != 0;
	out->confirm = (fired & MENU_PAD_CONFIRM) != 0;
	out->cancel  = (fired & MENU_PAD_CANCEL) != 0;
	out->pause   = (fired & MENU_PAD_PAUSE) != 0;

	out->dirEdge = 0;
	if (justPressed & MENU_PAD_UP) {
		out->dirEdge |= MENU_DIR_UP;
	}
	if (justPressed & MENU_PAD_DOWN) {
		out->dirEdge |= MENU_DIR_DOWN;
	}
	if (justPressed & (MENU_PAD_LEFT | MENU_PAD_L)) {
		out->dirEdge |= MENU_DIR_LEFT;
	}
	if (justPressed & (MENU_PAD_RIGHT | MENU_PAD_R)) {
		out->dirEdge |= MENU_DIR_RIGHT;
	}

	const uint32_t raw = sys->input.padMask;
	const uint32_t rawFired = raw & ~(*prevRaw);
	*prevRaw = raw;

	out->captured = keymapButtonFromBit(rawFired & (uint32_t)(-(int32_t)rawFired));
}

static void menuDrawTitleScreen(uint8_t *page, const MenuState *st,
                                int statusError) {
	static const MenuArt *const ART[MENU_TITLE_ROWS_FULL] = {
		&MENU_ART_START_GAME, &MENU_ART_CONTINUE, &MENU_ART_OPTIONS
	};
	const int rows = menuStateTitleRowCount(st);

	memcpy(page, MENU_ART_TITLE_BACKDROP, MENU_PAGE_SIZE);

	for (int i = 0; i < rows; ++i) {
		const MenuArt *art = ART[menuStateTitleRowAt(st, i)];
		menuBlit2bpp(page, art, (MENU_PAGE_W - art->w) / 2,
		             MENU_TITLE_START_Y + i * MENU_TITLE_ROW_STEP,
		             (i == st->cursor) ? MENU_BASE_SEL : MENU_BASE_DIM);
	}

	const char *status = menuStatusText(statusError, BACKUP_INTERNAL);
	if (status != 0) {
		menuDrawText(page, Video::_font, 5, MENU_TITLE_STATUS_Y, MENU_BASE_DIM,
		             status);
	}
}

enum {
	MENU_BOX_MARGIN  = 3,
	MENU_PAGE_CELLS  = MENU_PAGE_W / 8,

	MENU_CTRL_CELL_L = 6,
	MENU_CTRL_CELL_R = 34,
	MENU_CTRL_TEXT_L = MENU_CTRL_CELL_L + MENU_BOX_MARGIN,
	MENU_CTRL_TEXT_R = MENU_CTRL_CELL_R - MENU_BOX_MARGIN
};

static void menuBoxCells(int widest, int *cellL, int *cellR) {
	int cells = widest + 2 * MENU_BOX_MARGIN;

	if ((cells & 1) != 0) {
		++cells;
	}
	if (cells > MENU_PAGE_CELLS) {
		cells = MENU_PAGE_CELLS;
	}

	*cellL = (MENU_PAGE_CELLS - cells) / 2;
	*cellR = *cellL + cells;
}

static int menuWidest(const char *const *lines, int count) {
	int widest = 0;

	for (int i = 0; i < count; ++i) {
		if (lines[i] == 0) {
			continue;
		}
		const int len = (int)strlen(lines[i]);
		if (len > widest) {
			widest = len;
		}
	}
	return widest;
}

enum {
	MENU_PAUSE_PANEL_Y      = 48,
	MENU_PAUSE_ROW0_Y       = 60,
	MENU_PAUSE_ROW_DY       = 16,
	MENU_PAUSE_HEIGHT       = 96,
	MENU_PAUSE_HEIGHT_STATUS = 120,

	MENU_DEATH_PANEL_Y      = 48,
	MENU_DEATH_ROW0_Y       = 64,
	MENU_DEATH_ROW_DY       = 16,
	MENU_DEATH_HEIGHT       = 104,
	MENU_DEATH_HEIGHT_STATUS = 128
};

static void menuDrawCentred(uint8_t *page, int cellL, int cellR, int y,
                            uint8_t base, const char *s) {
	menuDrawText(page, Video::_font,
	             menuCentreCell(cellL + MENU_BOX_MARGIN,
	                            cellR - MENU_BOX_MARGIN, (int)strlen(s)),
	             y, base, s);
}

static void menuDrawRowList(uint8_t *page, const MenuState *st,
                            const char *const *labels, int panelY, int height,
                            int row0Y, int rowDy, const char *status) {
	const int rows = menuStateRowCount(st);

	const char *lines[MENU_ROWS_FULL + 1];
	for (int i = 0; i < rows; ++i) {
		lines[i] = labels[menuStateRowAt(st, i)];
	}
	lines[rows] = status;

	int cellL = 0;
	int cellR = 0;
	menuBoxCells(menuWidest(lines, rows + 1), &cellL, &cellR);

	const int x = cellL * 8;
	const int w = (cellR - cellL) * 8;

	menuDrawFill(page, x, panelY, w, height, MENU_COL_BORDER);
	menuDrawFill(page, x + 2, panelY + 2, w - 4, height - 4, MENU_COL_PANEL);

	for (int i = 0; i < rows; ++i) {
		menuDrawCentred(page, cellL, cellR, row0Y + i * rowDy,
		                (i == st->cursor) ? MENU_BASE_SEL : MENU_BASE_DIM,
		                labels[menuStateRowAt(st, i)]);
	}

	if (status != 0) {
		menuDrawCentred(page, cellL, cellR, row0Y + rows * rowDy + 4,
		                MENU_BASE_DIM, status);
	}
}

static void menuDrawPauseScreen(uint8_t *page, const MenuState *st,
                                int statusError) {
	static const char *const LABELS[MENU_ROWS_FULL] = {
		"RESUME", "SAVE GAME", "LOAD GAME", "CONTROLS", "RETURN TO MENU"
	};
	const char *labels[MENU_ROWS_FULL];
	const char *status = menuStatusText(statusError, BACKUP_INTERNAL);
	int i;
	const int hidden = MENU_ROWS_FULL - menuStateRowCount(st);
	const int height = ((status != 0) ? MENU_PAUSE_HEIGHT_STATUS
	                                  : MENU_PAUSE_HEIGHT)
	                   - hidden * MENU_PAUSE_ROW_DY;

	for (i = 0; i < MENU_ROWS_FULL; ++i) {
		labels[i] = (i == MENU_ROW_LOAD && st->cheatUnlocked) ? "LEVEL SELECT"
		                                                     : LABELS[i];
	}

	menuDrawRowList(page, st, labels, MENU_PAUSE_PANEL_Y, height,
	                MENU_PAUSE_ROW0_Y, MENU_PAUSE_ROW_DY, status);
}

static void menuDrawDeathScreen(uint8_t *page, const MenuState *st,
                                int statusError) {
	static const char *const LABELS[MENU_ROWS_FULL] = {
		"RESUME", "SAVE & RESUME", "LOAD GAME", "SAVE & QUIT", "QUIT"
	};
	const char *labels[MENU_ROWS_FULL];
	const char *status = menuStatusText(statusError, BACKUP_INTERNAL);
	int i;
	const int hidden = MENU_ROWS_FULL - menuStateRowCount(st);
	const int height = ((status != 0) ? MENU_DEATH_HEIGHT_STATUS
	                                  : MENU_DEATH_HEIGHT)
	                   - hidden * MENU_DEATH_ROW_DY;

	for (i = 0; i < MENU_ROWS_FULL; ++i) {
		labels[i] = (i == MENU_ROW_LOAD && st->cheatUnlocked) ? "LEVEL SELECT"
		                                                     : LABELS[i];
	}

	menuDrawRowList(page, st, labels, MENU_DEATH_PANEL_Y, height,
	                MENU_DEATH_ROW0_Y, MENU_DEATH_ROW_DY, status);
}

static void menuDrawConfirmScreen(uint8_t *page, const MenuState *st,
                                  const SaveInfo *save) {
	const uint8_t *font = Video::_font;

	char row[32];
	const char *lines[3];

	if (st->pending == MENU_ACT_RETURN_TO_TITLE) {
		lines[0] = "RETURN TO MENU ?";
		lines[1] = "PROGRESS WILL BE LOST";
		lines[2] = 0;
	} else if (st->pending == MENU_ACT_SAVE_GAME) {
		menuSaveLine(row, (int)sizeof(row), save);
		lines[0] = "OVERWRITE SAVE ?";
		lines[1] = row;
		lines[2] = 0;
	} else {
		menuSaveLine(row, (int)sizeof(row), save);
		lines[0] = "LOAD GAME ?";
		lines[1] = row;
		lines[2] = "PROGRESS WILL BE LOST";
	}

	int cellL = 0;
	int cellR = 0;
	menuBoxCells(menuWidest(lines, 3), &cellL, &cellR);

	const int x = cellL * 8;
	const int w = (cellR - cellL) * 8;

	menuDrawFill(page, x, 64, w, 80, MENU_COL_BORDER);
	menuDrawFill(page, x + 2, 66, w - 4, 76, MENU_COL_PANEL);

	for (int i = 0; i < 3; ++i) {
		if (lines[i] != 0) {
			menuDrawCentred(page, cellL, cellR, 76 + i * 16, MENU_BASE_DIM,
			                lines[i]);
		}
	}

	const int pairX = menuCentreCell(cellL + MENU_BOX_MARGIN,
	                                 cellR - MENU_BOX_MARGIN, 11);
	menuDrawText(page, font, pairX, 124,
	             st->confirmYes ? MENU_BASE_SEL : MENU_BASE_DIM, "YES");
	menuDrawText(page, font, pairX + 8, 124,
	             st->confirmYes ? MENU_BASE_DIM : MENU_BASE_SEL, "NO");
}

static void menuDrawNoticeScreen(uint8_t *page) {
	const char *lines[1];
	lines[0] = "GAME SAVED";

	int cellL = 0;
	int cellR = 0;
	menuBoxCells(menuWidest(lines, 1), &cellL, &cellR);

	const int x = cellL * 8;
	const int w = (cellR - cellL) * 8;

	menuDrawFill(page, x, 64, w, 32, MENU_COL_BORDER);
	menuDrawFill(page, x + 2, 66, w - 4, 28, MENU_COL_PANEL);

	menuDrawCentred(page, cellL, cellR, 76, MENU_BASE_DIM, lines[0]);
}

enum {
	MENU_CTRL_PANEL_Y    = 40,
	MENU_CTRL_HEADING_Y  = 52,
	MENU_CTRL_ROW0_Y     = 76,
	MENU_CTRL_ROW_DY     = 16,
	MENU_CTRL_HINT_Y     = 156,
	MENU_CTRL_HEIGHT     = 120,
	MENU_CTRL_HEIGHT_HINT = 136
};

static void menuDrawControlsScreen(uint8_t *page, const MenuState *st,
                                   int settingsError) {
	const uint8_t *font = Video::_font;

	const char *hint = menuStatusText(settingsError, BACKUP_INTERNAL);
	if (hint == 0 && st->capturing >= 0) {
		hint = "START CANCELS";
	}

	const int height = (hint != 0) ? MENU_CTRL_HEIGHT_HINT : MENU_CTRL_HEIGHT;

	menuDrawFill(page, MENU_CTRL_CELL_L * 8, MENU_CTRL_PANEL_Y,
	             (MENU_CTRL_CELL_R - MENU_CTRL_CELL_L) * 8, height,
	             MENU_COL_BORDER);
	menuDrawFill(page, MENU_CTRL_CELL_L * 8 + 2, MENU_CTRL_PANEL_Y + 2,
	             (MENU_CTRL_CELL_R - MENU_CTRL_CELL_L) * 8 - 4, height - 4,
	             MENU_COL_PANEL);
	menuDrawText(page, font, MENU_CTRL_TEXT_L, MENU_CTRL_HEADING_Y,
	             MENU_BASE_DIM, "CONTROLS");

	for (int i = 0; i < MENU_CONTROLS_ROWS; ++i) {
		const int y = MENU_CTRL_ROW0_Y + i * MENU_CTRL_ROW_DY;
		const int ramp = (st->cursor == i) ? MENU_BASE_SEL : MENU_BASE_DIM;
		const char *label;

		if (i == MENU_CONTROLS_ROW_RESET) {
			label = "RESET DEFAULTS";
		} else if (i == MENU_CONTROLS_ROW_BACK) {
			label = "BACK";
		} else {
			label = keymapRowName((KeymapRow)i);
		}

		menuDrawText(page, font, MENU_CTRL_TEXT_L, y, ramp, label);

		if (i >= KEYMAP_ROW_COUNT) {
			continue;
		}

		const char *value = (st->capturing == i)
		                        ? "PRESS?" : keymapButtonName(st->map.row[i]);
		menuDrawText(page, font, MENU_CTRL_TEXT_R - (int)strlen(value), y,
		             (st->capturing == i) ? MENU_BASE_SEL : ramp, value);
	}

	if (hint != 0) {
		menuDrawText(page, font, MENU_CTRL_TEXT_L, MENU_CTRL_HINT_Y,
		             MENU_BASE_DIM, hint);
	}
}

enum {
	MENU_OPT_PANEL_Y = 64,
	MENU_OPT_ROW0_Y  = 80,
	MENU_OPT_ROW_DY  = 16,
	MENU_OPT_HEIGHT  = 72
};

static void menuDrawOptionsScreen(uint8_t *page, const MenuState *st) {
	static const char *const LABELS[MENU_OPTIONS_ROWS_FULL] = {
		"LEVEL SELECT", "CONTROLS", "BACK"
	};

	const int rows = menuStateOptionsRowCount(st);
	const char *lines[MENU_OPTIONS_ROWS_FULL];

	for (int i = 0; i < rows; ++i) {
		lines[i] = LABELS[menuStateOptionsRowAt(st, i)];
	}

	int cellL = 0;
	int cellR = 0;
	menuBoxCells(menuWidest(lines, rows), &cellL, &cellR);

	menuDrawFill(page, cellL * 8, MENU_OPT_PANEL_Y, (cellR - cellL) * 8,
	             MENU_OPT_HEIGHT, MENU_COL_BORDER);
	menuDrawFill(page, cellL * 8 + 2, MENU_OPT_PANEL_Y + 2,
	             (cellR - cellL) * 8 - 4, MENU_OPT_HEIGHT - 4, MENU_COL_PANEL);

	for (int i = 0; i < rows; ++i) {
		menuDrawCentred(page, cellL, cellR, MENU_OPT_ROW0_Y + i * MENU_OPT_ROW_DY,
		                (i == st->cursor) ? MENU_BASE_SEL : MENU_BASE_DIM,
		                lines[i]);
	}
}

enum {
	MENU_LVL_NAME_CELLS = 11,
	MENU_LVL_CODE_CELLS = 4,
	MENU_LVL_GAP_CELLS  = 2,
	MENU_LVL_CONTENT    = MENU_LVL_NAME_CELLS + MENU_LVL_GAP_CELLS +
	                      3 * MENU_LVL_CODE_CELLS + 2 * MENU_LVL_GAP_CELLS,
	MENU_LVL_CELL_L     = (MENU_PAGE_CELLS -
	                       (MENU_LVL_CONTENT + 2 * MENU_BOX_MARGIN)) / 2,
	MENU_LVL_CELL_R     = MENU_LVL_CELL_L + MENU_LVL_CONTENT +
	                      2 * MENU_BOX_MARGIN,
	MENU_LVL_TEXT_L     = MENU_LVL_CELL_L + MENU_BOX_MARGIN,
	MENU_LVL_PANEL_Y    = 24,
	MENU_LVL_HEADING_Y  = 34,
	MENU_LVL_ROW0_Y     = 54,
	MENU_LVL_ROW_DY     = 12
};

static void menuDrawLevelSelect(uint8_t *page, const MenuState *st) {
	const int rows = menuStateLevelRowCount(st);
	const int height = MENU_LVL_ROW0_Y - MENU_LVL_PANEL_Y +
	                   rows * MENU_LVL_ROW_DY + 8;

	menuDrawFill(page, MENU_LVL_CELL_L * 8, MENU_LVL_PANEL_Y,
	             (MENU_LVL_CELL_R - MENU_LVL_CELL_L) * 8, height,
	             MENU_COL_BORDER);
	menuDrawFill(page, MENU_LVL_CELL_L * 8 + 2, MENU_LVL_PANEL_Y + 2,
	             (MENU_LVL_CELL_R - MENU_LVL_CELL_L) * 8 - 4, height - 4,
	             MENU_COL_PANEL);
	menuDrawText(page, Video::_font, MENU_LVL_TEXT_L, MENU_LVL_HEADING_Y,
	             MENU_BASE_DIM, "LEVEL SELECT");

	for (int row = 0; row < rows; ++row) {
		const int start = menuStateLevelRowStart(st, row);
		const int len = menuStateLevelRowLen(st, row);
		const int chapter = checkpointChapterOf(start);
		const int y = MENU_LVL_ROW0_Y + row * MENU_LVL_ROW_DY;

		if (start == checkpointChapterFirst(chapter)) {
			menuDrawText(page, Video::_font, MENU_LVL_TEXT_L, y, MENU_BASE_DIM,
			             savedataChapterName(checkpointChapterPart(chapter)));
		}

		for (int k = 0; k < len; ++k) {
			const int cell = MENU_LVL_TEXT_L + MENU_LVL_NAME_CELLS +
			                 MENU_LVL_GAP_CELLS +
			                 k * (MENU_LVL_CODE_CELLS + MENU_LVL_GAP_CELLS);

			menuDrawText(page, Video::_font, cell, y,
			             (start + k == st->cursor) ? MENU_BASE_SEL
			                                       : MENU_BASE_DIM,
			             checkpointWord(start + k));
		}
	}
}

static void menuRenderFrame(uint8_t *page, System *sys, const MenuState *st,
                            int statusError, int settingsError,
                            const SaveInfo *save, bool overlay,
                            bool refreshBackdrop, const uint8_t *backdrop,
                            const uint8_t *freezePal) {
	if (overlay && refreshBackdrop) {
		memcpy(page, backdrop, MENU_PAGE_SIZE);
		menuFreezeRemap(page, freezePal);
	}

	switch (st->screen) {
	case MENU_TITLE:
		menuDrawTitleScreen(page, st, statusError);
		break;
	case MENU_PAUSE:
		menuDrawPauseScreen(page, st, statusError);
		break;
	case MENU_CONTROLS:
		memset(page, 0, MENU_PAGE_SIZE);
		menuDrawControlsScreen(page, st, settingsError);
		break;
	case MENU_OPTIONS:
		memset(page, 0, MENU_PAGE_SIZE);
		menuDrawOptionsScreen(page, st);
		break;
	case MENU_LEVEL_SELECT:
		memset(page, 0, MENU_PAGE_SIZE);
		menuDrawLevelSelect(page, st);
		break;
	case MENU_CONFIRM:
		if (!overlay) {
			memset(page, 0, MENU_PAGE_SIZE);
		}
		menuDrawConfirmScreen(page, st, save);
		break;
	case MENU_DEATH:
		if (!overlay) {
			memset(page, 0, MENU_PAGE_SIZE);
		}
		menuDrawDeathScreen(page, st, statusError);
		break;
	case MENU_NOTICE:
		if (!overlay) {
			memset(page, 0, MENU_PAGE_SIZE);
		}
		menuDrawNoticeScreen(page);
		break;
	default:
		break;
	}

	sys->updateDisplay(page);

	cd_music_frame_shown();
	cd_music_tick();
}

static int menuRunAttract(System *sys, GameSession *session, uint8_t *page) {
	(void)sys;
	(void)page;

	session->runIntroAttract();

	return OPENING_FADE_SKIP_FIELDS;
}

#define MENU_LOADING_CELL_X 29
#define MENU_LOADING_Y      184

void menuShowLoading(System *sys, int fadeOutFields) {
	fade_ramp(FADE_DARK, fadeOutFields);

	memset(s_menuPage, 0, MENU_PAGE_SIZE);
	menuDrawText(s_menuPage, Video::_font, MENU_LOADING_CELL_X, MENU_LOADING_Y,
	             MENU_BASE_DIM, "LOADING...");

	menuHoldLoading(sys);

	fade_ramp(FADE_LIT, OPENING_FADE_SKIP_FIELDS);
}

void menuHoldLoading(System *sys) {
	sys->setPalette(MENU_ART_PALETTE);
	sys->updateDisplay(s_menuPage);
}

static void menuAppendNum(char *dst, int cap, int *pos, uint32_t v) {
	char digits[12];
	int n = 0;

	if (v == 0) {
		menuAppendChar(dst, cap, pos, '0');
		return;
	}
	while (v != 0 && n < (int)sizeof(digits)) {
		digits[n] = (char)('0' + (v % 10));
		v /= 10;
		++n;
	}
	while (n > 0) {
		--n;
		menuAppendChar(dst, cap, pos, digits[n]);
	}
}

void menuShowNoSpace(System *sys, const BackupSpace *space) {
	const uint8_t *font = Video::_font;
	char line[40];

	memset(s_menuPage, 0, MENU_PAGE_SIZE);
	menuDrawFill(s_menuPage, 24, 48, 272, 112, MENU_COL_BORDER);
	menuDrawFill(s_menuPage, 26, 50, 268, 108, MENU_COL_PANEL);

	menuDrawText(s_menuPage, font, 12, 60, MENU_BASE_SEL, "NOT ENOUGH SPACE");

	if (space->blockSize != 0) {
		uint32_t needed = savedataBlocksNeeded(space->blockSize,
		                                       SAVE_MAX_BYTES);

		if (needed <= space->freeBlocks) {
			needed = space->freeBlocks + 1;
		}

		int pos = 0;
		line[0] = 0;
		menuAppendNum(line, (int)sizeof(line), &pos, needed);
		menuAppendStr(line, (int)sizeof(line), &pos, " BLOCKS NEEDED");
		menuDrawText(s_menuPage, font, 5, 84, MENU_BASE_DIM, line);

		pos = 0;
		line[0] = 0;
		menuAppendStr(line, (int)sizeof(line), &pos, "BUT ONLY ");
		menuAppendNum(line, (int)sizeof(line), &pos, space->freeBlocks);
		menuAppendStr(line, (int)sizeof(line), &pos, " AVAILABLE");
		menuDrawText(s_menuPage, font, 5, 100, MENU_BASE_DIM, line);
	} else {
		menuDrawText(s_menuPage, font, 5, 84, MENU_BASE_DIM,
		             "NO BACKUP DEVICE FOUND");
	}

	menuDrawText(s_menuPage, font, 5, 124, MENU_BASE_DIM,
	             "A OR C  PLAY WITHOUT SAVING");
	menuDrawText(s_menuPage, font, 5, 140, MENU_BASE_DIM,
	             "B       SYSTEM BIOS");

	sys->setPalette(MENU_ART_PALETTE);
	sys->updateDisplay(s_menuPage);
	fade_ramp(FADE_LIT, OPENING_FADE_SKIP_FIELDS);

	uint32_t prevPad = 0;
	uint32_t prevRaw = 0;
	int repeatTimer = 0;
	menuPrimeEdges(sys, &prevPad, &repeatTimer, &prevRaw);

	while (!sys->input.quit) {
		MenuInput in;
		menuPollEdges(sys, &prevPad, &repeatTimer, &prevRaw, &in);

		if (in.cancel) {
			boot_bios();
		}
		if (in.confirm) {
			break;
		}

		sys->updateDisplay(s_menuPage);
	}

	fade_ramp(FADE_DARK, OPENING_FADE_SKIP_FIELDS);
}

bool Menu::runLoad() {
	menuShowLoading(_sys, OPENING_FADE_SKIP_FIELDS);

	const bool ok = _session->loadSave();

	if (!_session->hasPendingCode()) {
		fade_ramp(FADE_DARK, OPENING_FADE_SKIP_FIELDS);
	}

	_loadedSlot = ok;
	return ok;
}

bool Menu::runTitle() {
	_loadedSlot = false;

	cd_music_stop();

	refreshSave();
	menuStateEnterTitle(&_st);
	_statusError = BACKUP_OK;
	_settingsError = BACKUP_OK;

	const int bootFade = OPENING_FADE_SKIP_FIELDS;

	cd_music_set_cue(MENU_TITLE_CUE, 1, 0);
	_session->startTitleMusic();

	_sys->setPalette(MENU_ART_TITLE_PALETTE);
	menuRenderFrame(_page, _sys, &_st, _statusError, _settingsError, &_save,
	                false, false, 0, 0);
	fade_ramp(FADE_LIT, bootFade);
	menuPrimeEdges(_sys, &_prevPad, &_repeatTimer, &_prevRaw);

	_idleFrames = 0;

	while (!_sys->input.quit) {
		MenuInput in;
		menuPollEdges(_sys, &_prevPad, &_repeatTimer, &_prevRaw, &in);

		if (_sys->input.resetRequest) {
			_sys->input.resetRequest = false;
			_session->softReset();
		}

		const MenuScreen prevScreen = _st.screen;
		const MenuAction act = menuStateStep(&_st, &in);

		if (_st.cheatUnlocked) {
			_session->unlockCheat();
		}

		if (prevScreen != MENU_CONTROLS && _st.screen == MENU_CONTROLS) {
			_settingsError = BACKUP_OK;
		}

		if (act == MENU_ACT_SAVE_KEYMAP) {
			keymapSetActive(&_st.map);

			Settings s;
			s.map = _st.map;
			_settingsError = settingsStore(&s);
		} else if (act == MENU_ACT_START_GAME) {
			cd_music_stop();
			_session->stopTitleMusic();

			menuShowLoading(_sys, OPENING_FADE_FIELDS);
			_session->startNewGame();

			_session->beginLoadingHold();
			return true;
		} else if (act == MENU_ACT_LOAD_GAME) {
			cd_music_stop();
			_session->stopTitleMusic();

			if (runLoad()) {
				return true;
			}

			_statusError = _session->lastSaveError();
			refreshSave();
			menuStateEnterTitle(&_st);
			cd_music_set_cue(MENU_TITLE_CUE, 1, 0);
			_session->startTitleMusic();
			_sys->setPalette(MENU_ART_TITLE_PALETTE);
			menuRenderFrame(_page, _sys, &_st, _statusError, _settingsError,
			                &_save, false, false, 0, 0);
			fade_ramp(FADE_LIT, OPENING_FADE_SKIP_FIELDS);
		} else if (act == MENU_ACT_LEVEL_JUMP) {
			cd_music_stop();
			_session->stopTitleMusic();

			menuShowLoading(_sys, OPENING_FADE_FIELDS);
			_session->startAtCheckpoint(_st.levelChoice);
			_session->beginLoadingHold();
			return true;
		}

		if (_prevPad != 0 || _st.screen != MENU_TITLE) {
			_idleFrames = 0;
		} else {
			_idleFrames++;
		}

		int fadeIn = 0;

		if (_idleFrames >= MENU_TITLE_IDLE_FRAMES) {
			_idleFrames = 0;
			cd_music_stop();
			_session->stopTitleMusic();
			fadeIn = menuRunAttract(_sys, _session, _page);
			cd_music_set_cue(MENU_TITLE_CUE, 1, 0);
			_session->startTitleMusic();
			menuPrimeEdges(_sys, &_prevPad, &_repeatTimer, &_prevRaw);
			_sys->setPalette(MENU_ART_TITLE_PALETTE);
		}

		_session->tickTitleMusic();
		menuRenderFrame(_page, _sys, &_st, _statusError, _settingsError, &_save,
		                false, false, 0, 0);

		if (fadeIn != 0) {
			fade_ramp(FADE_LIT, fadeIn);
		}
	}

	_session->stopTitleMusic();
	return false;
}

bool Menu::runDeath(int statusError, bool scriptWaiting) {
	menuStateEnterDeath(&_st);
	_loadedSlot = false;
	_statusError = statusError;

	_session->silenceAudio();
	cd_music_pause();

	_sys->setPalette(MENU_ART_PALETTE);

	refreshSave();

	menuRenderFrame(_page, _sys, &_st, _statusError, BACKUP_OK, &_save, false,
	                false, 0, 0);
	menuPrimeEdges(_sys, &_prevPad, &_repeatTimer, &_prevRaw);

	fade_ramp(FADE_LIT, OPENING_FADE_SKIP_FIELDS);

	bool resume = false;
	bool paletteOwnedByLoad = false;

	while (!_sys->input.quit) {
		MenuInput in;
		menuPollEdges(_sys, &_prevPad, &_repeatTimer, &_prevRaw, &in);

		if (_sys->input.resetRequest) {
			_sys->input.resetRequest = false;
			_session->softReset();
			resume = false;
			break;
		}

		const MenuScreen prevScreen = _st.screen;
		const MenuAction act = menuStateStep(&_st, &in);

		if (prevScreen != MENU_DEATH && _st.screen == MENU_DEATH) {
			_statusError = BACKUP_OK;
		}

		if (act == MENU_ACT_RETRY) {
			_session->setDeathRetry(scriptWaiting);
			resume = true;
			break;
		} else if (act == MENU_ACT_RETURN_TO_TITLE) {
			resume = false;
			break;
		} else if (act == MENU_ACT_SAVE_RETRY || act == MENU_ACT_SAVE_AND_QUIT) {
			if (!_session->saveDeathCheckpoint()) {
				_statusError = _session->lastSaveError();
				refreshSave();
			} else {
				_session->setDeathRetry(scriptWaiting);
				resume = (act == MENU_ACT_SAVE_RETRY);
				break;
			}
		} else if (act == MENU_ACT_LEVEL_JUMP) {
			cd_music_stop();
			menuShowLoading(_sys, OPENING_FADE_FIELDS);
			_session->startAtCheckpoint(_st.levelChoice);
			_session->beginLoadingHold();
			resume = true;
			paletteOwnedByLoad = true;
			break;
		} else if (act == MENU_ACT_LOAD_GAME) {
			if (runLoad()) {
				resume = true;
				paletteOwnedByLoad = true;
				break;
			}

			_statusError = _session->lastSaveError();
			refreshSave();
			_sys->setPalette(MENU_ART_PALETTE);
			menuRenderFrame(_page, _sys, &_st, _statusError, BACKUP_OK, &_save,
			                false, false, 0, 0);
			fade_ramp(FADE_LIT, OPENING_FADE_SKIP_FIELDS);
		}

		menuRenderFrame(_page, _sys, &_st, _statusError, BACKUP_OK, &_save,
		                false, false, 0, 0);
	}

	if (!_session->hasPendingCode()) {
		fade_ramp(FADE_DARK, OPENING_FADE_SKIP_FIELDS);
	}

	if (!paletteOwnedByLoad) {
		_session->restoreGamePalette();
	}

	cd_music_resume();

	return resume;
}

bool Menu::runPause() {

	const uint8_t *backdrop = _session->frontPage();

	menuStateEnterPause(&_st);
	_loadedSlot = false;
	_statusError = BACKUP_OK;
	_settingsError = BACKUP_OK;

	_session->pauseAudio();
	cd_music_pause();

	display_get_palette(_savedPal);
	_sys->setPalette(MENU_ART_PALETTE);

	refreshSave();

	MenuScreen lastScreen = MENU_NONE;
	menuRenderFrame(_page, _sys, &_st, _statusError, _settingsError, &_save,
	                true, true, backdrop, _savedPal);
	lastScreen = _st.screen;
	menuPrimeEdges(_sys, &_prevPad, &_repeatTimer, &_prevRaw);

	bool resume = true;
	bool paletteOwnedByLoad = false;

	while (!_sys->input.quit) {
		MenuInput in;
		menuPollEdges(_sys, &_prevPad, &_repeatTimer, &_prevRaw, &in);

		if (_sys->input.resetRequest) {
			_sys->input.resetRequest = false;
			_session->softReset();
			resume = false;
			break;
		}

		const MenuScreen prevScreen = _st.screen;
		const MenuAction act = menuStateStep(&_st, &in);

		if (prevScreen != MENU_CONTROLS && _st.screen == MENU_CONTROLS) {
			_settingsError = BACKUP_OK;
		}

		if (act == MENU_ACT_SAVE_KEYMAP) {
			keymapSetActive(&_st.map);

			Settings s;
			s.map = _st.map;
			_settingsError = settingsStore(&s);
		} else if (act == MENU_ACT_RESUME) {
			resume = true;
			break;
		} else if (act == MENU_ACT_RETURN_TO_TITLE) {
			resume = false;
			break;
		} else if (act == MENU_ACT_LEVEL_JUMP) {
			cd_music_stop();
			menuShowLoading(_sys, OPENING_FADE_FIELDS);
			_session->startAtCheckpoint(_st.levelChoice);
			_session->beginLoadingHold();
			resume = true;
			paletteOwnedByLoad = true;
			break;
		} else if (act == MENU_ACT_SAVE_GAME) {
			const bool saved = _session->saveState();
			_statusError = _session->lastSaveError();
			if (saved && _session->lastSaveDroppedBackground()) {
				_statusError = ENGINE_SAVE_WARN_NO_BACKGROUND;
			}
			refreshSave();

			if (_statusError == BACKUP_OK) {
				menuStateShowSaved(&_st);
			}
		} else if (act == MENU_ACT_LOAD_GAME) {
			if (runLoad()) {
				resume = true;
				paletteOwnedByLoad = true;
				break;
			}

			_statusError = _session->lastSaveError();
			refreshSave();
			_sys->setPalette(MENU_ART_PALETTE);
			menuRenderFrame(_page, _sys, &_st, _statusError, _settingsError,
			                &_save, true, true, backdrop, _savedPal);
			lastScreen = _st.screen;
			fade_ramp(FADE_LIT, OPENING_FADE_SKIP_FIELDS);
		}

		const bool refreshBackdrop = (_st.screen != lastScreen);
		menuRenderFrame(_page, _sys, &_st, _statusError, _settingsError, &_save,
		                true, refreshBackdrop, backdrop, _savedPal);
		lastScreen = _st.screen;
	}

	if (!paletteOwnedByLoad) {
		_session->restoreGamePalette();
		_session->resumeAudio();
	}
	cd_music_resume();
	if (resume && !_session->hasPendingCode()) {
		_sys->updateDisplay(_session->frontPage());
	}
	return resume;
}

void Menu::showLoading(int fadeOutFields) {
	menuShowLoading(_sys, fadeOutFields);
}

void Menu::holdLoading() {
	menuHoldLoading(_sys);
}

void Menu::showNoSpace(const BackupSpace *space) {
	menuShowNoSpace(_sys, space);
}
