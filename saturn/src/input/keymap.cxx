#include "keymap.h"

static const uint32_t s_bits[PAD_COUNT] = {
	0,
	PAD_BIT_A, PAD_BIT_B, PAD_BIT_C,
	PAD_BIT_X, PAD_BIT_Y, PAD_BIT_Z,
	PAD_BIT_L, PAD_BIT_R
};

static const char *const s_names[PAD_COUNT] = {
	"-", "A", "B", "C", "X", "Y", "Z", "L", "R"
};

static const char *const s_rowNames[KEYMAP_ROW_COUNT] = {
	"ACTION", "JUMP", "RUN"
};

static KeyMap s_active;
static bool s_activeInit = false;

uint32_t keymapButtonBit(PadButton b)
{
	if (b <= PAD_NONE || b >= PAD_COUNT) {
		return 0;
	}
	return s_bits[b];
}

PadButton keymapButtonFromBit(uint32_t bit)
{
	for (int i = PAD_NONE + 1; i < PAD_COUNT; ++i) {
		if (s_bits[i] == bit) {
			return (PadButton)i;
		}
	}
	return PAD_NONE;
}

const char *keymapButtonName(PadButton b)
{
	if (b <= PAD_NONE || b >= PAD_COUNT) {
		return s_names[PAD_NONE];
	}
	return s_names[b];
}

const char *keymapRowName(KeymapRow row)
{
	if (row < 0 || row >= KEYMAP_ROW_COUNT) {
		return "";
	}
	return s_rowNames[row];
}

void keymapDefaults(KeyMap *m)
{
	m->row[KEYMAP_ROW_ACTION] = PAD_A;
	m->row[KEYMAP_ROW_JUMP]   = PAD_C;
	m->row[KEYMAP_ROW_RUN]    = PAD_B;
}

bool keymapValid(const KeyMap *m)
{
	for (int i = 0; i < KEYMAP_ROW_COUNT; ++i) {
		if (m->row[i] <= PAD_NONE || m->row[i] >= PAD_COUNT) {
			return false;
		}
		for (int j = i + 1; j < KEYMAP_ROW_COUNT; ++j) {
			if (m->row[i] == m->row[j]) {
				return false;
			}
		}
	}
	return true;
}

bool keymapAssign(KeyMap *m, KeymapRow row, PadButton b)
{
	if (b <= PAD_NONE || b >= PAD_COUNT) {
		return false;
	}
	if (row < 0 || row >= KEYMAP_ROW_COUNT) {
		return false;
	}
	if (m->row[row] == b) {
		return false;
	}

	const PadButton displaced = m->row[row];

	for (int i = 0; i < KEYMAP_ROW_COUNT; ++i) {
		if (i != row && m->row[i] == b) {
			m->row[i] = displaced;
		}
	}

	m->row[row] = b;
	return true;
}

void keymapApply(const KeyMap *m, uint32_t pad, bool *action, bool *jump,
                 bool *run)
{
	*action = (pad & keymapButtonBit(m->row[KEYMAP_ROW_ACTION])) != 0;
	*jump   = (pad & keymapButtonBit(m->row[KEYMAP_ROW_JUMP])) != 0;
	*run    = (pad & keymapButtonBit(m->row[KEYMAP_ROW_RUN])) != 0;
}

const KeyMap *keymapActive(void)
{
	if (!s_activeInit) {
		keymapDefaults(&s_active);
		s_activeInit = true;
	}
	return &s_active;
}

void keymapSetActive(const KeyMap *m)
{
	if (!keymapValid(m)) {
		return;
	}
	s_active = *m;
	s_activeInit = true;
}
