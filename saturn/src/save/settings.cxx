#include "settings.h"

extern "C" {
#include <string.h>
}

void settingsDefaults(Settings *s)
{
	keymapDefaults(&s->map);
}

void settingsPack(uint8_t *buf, const Settings *s)
{
	memset(buf, 0, SETTINGS_SIZE);
	buf[0] = 'A';
	buf[1] = 'W';
	buf[2] = 'C';
	buf[3] = 'F';
	buf[4] = (uint8_t)((SETTINGS_VER >> 8) & 0xFF);
	buf[5] = (uint8_t)(SETTINGS_VER & 0xFF);
	for (int i = 0; i < KEYMAP_ROW_COUNT; ++i) {
		buf[6 + i] = (uint8_t)s->map.row[i];
	}
}

bool settingsUnpack(const uint8_t *buf, Settings *s)
{
	if (buf[0] != 'A' || buf[1] != 'W' || buf[2] != 'C' || buf[3] != 'F') {
		return false;
	}
	const uint16_t ver = (uint16_t)((buf[4] << 8) | buf[5]);
	if (ver != SETTINGS_VER) {
		return false;
	}
	KeyMap map;
	for (int i = 0; i < KEYMAP_ROW_COUNT; ++i) {
		map.row[i] = (PadButton)buf[6 + i];
	}
	if (!keymapValid(&map)) {
		return false;
	}

	s->map = map;
	return true;
}

static const char SETTINGS_NAME[] = "AW_CFG";

bool settingsLoad(Settings *s)
{
	uint8_t buf[SETTINGS_SIZE];

	if (backup_read(BACKUP_INTERNAL, SETTINGS_NAME, buf, SETTINGS_SIZE)
	    != BACKUP_OK) {
		return false;
	}
	return settingsUnpack(buf, s);
}

int settingsStore(const Settings *s)
{
	uint8_t buf[SETTINGS_SIZE];

	settingsPack(buf, s);
	return backup_write(BACKUP_INTERNAL, SETTINGS_NAME, "BUTTONS", buf,
	                     SETTINGS_SIZE, 1);
}

