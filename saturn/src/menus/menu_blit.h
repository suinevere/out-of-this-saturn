#ifndef MENU_BLIT_H
#define MENU_BLIT_H

#include <stdint.h>

struct MenuArt {
	const uint8_t *bits;
	int16_t w;
	int16_t h;
};

void menuBlit4bpp(uint8_t *page, const MenuArt *art, int x, int y);

void menuBlit2bpp(uint8_t *page, const MenuArt *art, int x, int y, uint8_t base);

#endif
