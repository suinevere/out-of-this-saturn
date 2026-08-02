#ifndef KEYMAP_H
#define KEYMAP_H

#include <stdint.h>
#include "pad.h"

enum PadButton {
	PAD_NONE = 0,
	PAD_A, PAD_B, PAD_C, PAD_X, PAD_Y, PAD_Z, PAD_L, PAD_R,
	PAD_COUNT
};

enum KeymapRow {
	KEYMAP_ROW_ACTION,
	KEYMAP_ROW_JUMP,
	KEYMAP_ROW_RUN,
	KEYMAP_ROW_COUNT
};

struct KeyMap {
	PadButton row[KEYMAP_ROW_COUNT];
};

uint32_t keymapButtonBit(PadButton b);

PadButton keymapButtonFromBit(uint32_t bit);

const char *keymapButtonName(PadButton b);

const char *keymapRowName(KeymapRow row);

void keymapDefaults(KeyMap *m);

bool keymapValid(const KeyMap *m);

bool keymapAssign(KeyMap *m, KeymapRow row, PadButton b);

void keymapApply(const KeyMap *m, uint32_t pad, bool *action, bool *jump,
                 bool *run);

const KeyMap *keymapActive(void);

void keymapSetActive(const KeyMap *m);

#endif
