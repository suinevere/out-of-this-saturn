#include "menu_draw.h"

void menuDrawFill(uint8_t *page, int x, int y, int w, int h, uint8_t color)
{
	int x0 = x;
	int y0 = y;
	int x1 = x + w;
	int y1 = y + h;

	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > MENU_PAGE_W) x1 = MENU_PAGE_W;
	if (y1 > MENU_PAGE_H) y1 = MENU_PAGE_H;

	for (int py = y0; py < y1; ++py) {
		for (int px = x0; px < x1; ++px) {
			uint8_t *b = page + py * MENU_PAGE_PITCH + px / 2;
			if (px & 1) {
				*b = (*b & 0xF0) | color;
			} else {
				*b = (*b & 0x0F) | (color << 4);
			}
		}
	}
}

void menuDrawChar(uint8_t *page, const uint8_t *font, int cellX, int y, uint8_t base, char c)
{
	if (cellX < 0 || cellX > 39 || y < 0 || y > 192) {
		return;
	}

	const uint8_t color = (uint8_t)(base + 2);
	const uint8_t *ft = font + (c - ' ') * 8;
	uint8_t *p = page + cellX * 4 + y * MENU_PAGE_PITCH;

	for (int j = 0; j < 8; ++j) {
		uint8_t ch = ft[j];
		for (int i = 0; i < 4; ++i) {
			uint8_t b = p[i];
			uint8_t cmask = 0xFF;
			uint8_t colb = 0;
			if (ch & 0x80) {
				colb |= color << 4;
				cmask &= 0x0F;
			}
			ch <<= 1;
			if (ch & 0x80) {
				colb |= color;
				cmask &= 0xF0;
			}
			ch <<= 1;
			p[i] = (b & cmask) | colb;
		}
		p += MENU_PAGE_PITCH;
	}
}

void menuDrawText(uint8_t *page, const uint8_t *font, int cellX, int y, uint8_t base, const char *s)
{
	int cx = cellX;
	while (*s != '\0' && cx < 40) {
		menuDrawChar(page, font, cx, y, base, *s);
		++cx;
		++s;
	}
}

int menuCentreCell(int cellLeft, int cellRight, int len)
{
	const int span = cellRight - cellLeft;

	if (len >= span) {
		return cellLeft;
	}
	return cellLeft + (span - len) / 2;
}

void menuFreezeRemap(uint8_t *page, const uint8_t *srcPalette)
{
	uint8_t map[16];
	for (int i = 0; i < 16; ++i) {
		const uint8_t b0 = srcPalette[i * 2];
		const uint8_t b1 = srcPalette[i * 2 + 1];
		const int r = b0 & 0x0F;
		const int g = (b1 & 0xF0) >> 4;
		const int b = b1 & 0x0F;
		const int y = (r * 77 + g * 151 + b * 28) >> 8;
		map[i] = (uint8_t)(y >> 2);
	}

	uint8_t lut8[256];
	for (int i = 0; i < 256; ++i) {
		lut8[i] = (uint8_t)((map[i >> 4] << 4) | map[i & 0x0F]);
	}

	for (int i = 0; i < MENU_PAGE_SIZE; ++i) {
		page[i] = lut8[page[i]];
	}
}
