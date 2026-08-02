#ifndef MENU_DRAW_H
#define MENU_DRAW_H

#include <stdint.h>

enum {
	MENU_PAGE_W     = 320,
	MENU_PAGE_H     = 200,
	MENU_PAGE_PITCH = 160,
	MENU_PAGE_SIZE  = 32000
};

void menuDrawFill(uint8_t *page, int x, int y, int w, int h, uint8_t color);

void menuDrawChar(uint8_t *page, const uint8_t *font, int cellX, int y, uint8_t base, char c);

void menuDrawText(uint8_t *page, const uint8_t *font, int cellX, int y, uint8_t base, const char *s);

int menuCentreCell(int cellLeft, int cellRight, int len);

void menuFreezeRemap(uint8_t *page, const uint8_t *srcPalette);

#endif
