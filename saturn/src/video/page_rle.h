#ifndef PAGE_RLE_H
#define PAGE_RLE_H

#include <stdint.h>

int32_t pageRleEncode(const uint8_t *src, int32_t srcLen, uint8_t *dst,
                      int32_t dstCap);

int32_t pageDeltaEncode(const uint8_t *src, int32_t srcLen, int32_t stride,
                        int32_t rowStep, uint8_t *dst, int32_t dstCap);

bool pageRleDecode(const uint8_t *src, int32_t srcLen, uint8_t *dst,
                   int32_t dstLen);

bool pageDeltaDecode(const uint8_t *src, int32_t srcLen, uint8_t *dst,
                     int32_t dstLen, int32_t stride, int32_t rowStep);

#endif
