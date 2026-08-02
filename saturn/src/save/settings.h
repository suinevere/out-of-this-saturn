#ifndef SETTINGS_H
#define SETTINGS_H

#include "backup.h"
#include "keymap.h"

enum {
	SETTINGS_SIZE = 16,
	SETTINGS_VER  = 2
};

struct Settings {
	KeyMap map;
};

void settingsDefaults(Settings *s);

void settingsPack(uint8_t *buf, const Settings *s);

bool settingsUnpack(const uint8_t *buf, Settings *s);

bool settingsLoad(Settings *s);

int settingsStore(const Settings *s);

#endif
