#include "page_rle.h"

enum {
	PAGE_RLE_MAX_BLOCK = 128
};

static uint8_t pageKeptByte(const uint8_t *src, int32_t i, int32_t stride,
                            int32_t rowStep) {
	if (stride <= 0 || rowStep <= 1) {
		return src[i];
	}
	return src[(i / stride) * rowStep * stride + (i % stride)];
}

static uint8_t pageDeltaByte(const uint8_t *src, int32_t i, int32_t stride,
                             int32_t rowStep) {
	const uint8_t v = pageKeptByte(src, i, stride, rowStep);
	if (stride > 0 && i >= stride) {
		return (uint8_t)(v ^ pageKeptByte(src, i - stride, stride, rowStep));
	}
	return v;
}

static int32_t pageRleRunLength(const uint8_t *src, int32_t i, int32_t srcLen,
                                int32_t stride, int32_t rowStep) {
	const uint8_t v = pageDeltaByte(src, i, stride, rowStep);
	int32_t n = 1;
	while (i + n < srcLen && n < PAGE_RLE_MAX_BLOCK &&
	       pageDeltaByte(src, i + n, stride, rowStep) == v) {
		++n;
	}
	return n;
}

static int32_t pageEncodeStride(const uint8_t *src, int32_t srcLen,
                                int32_t stride, int32_t rowStep, uint8_t *dst,
                                int32_t dstCap) {
	if (src == 0 || dst == 0 || srcLen < 0 || dstCap < 0 || stride < 0 ||
	    rowStep < 1) {
		return -1;
	}

	int32_t in = 0;
	int32_t out = 0;

	while (in < srcLen) {
		const int32_t run = pageRleRunLength(src, in, srcLen, stride, rowStep);

		if (run >= 2) {
			if (out + 2 > dstCap) {
				return -1;
			}
			dst[out++] = (uint8_t)(0x80 | (run - 1));
			dst[out++] = pageDeltaByte(src, in, stride, rowStep);
			in += run;
			continue;
		}

		int32_t lit = 0;
		while (in + lit < srcLen && lit < PAGE_RLE_MAX_BLOCK &&
		       pageRleRunLength(src, in + lit, srcLen, stride, rowStep) < 2) {
			++lit;
		}

		if (out + 1 + lit > dstCap) {
			return -1;
		}
		dst[out++] = (uint8_t)(lit - 1);
		for (int32_t i = 0; i < lit; ++i) {
			dst[out++] = pageDeltaByte(src, in + i, stride, rowStep);
		}
		in += lit;
	}

	return out;
}

int32_t pageRleEncode(const uint8_t *src, int32_t srcLen, uint8_t *dst,
                      int32_t dstCap) {
	return pageEncodeStride(src, srcLen, 0, 1, dst, dstCap);
}

int32_t pageDeltaEncode(const uint8_t *src, int32_t srcLen, int32_t stride,
                        int32_t rowStep, uint8_t *dst, int32_t dstCap) {
	if (stride <= 0 || rowStep < 1 || (srcLen % stride) != 0) {
		return -1;
	}
	const int32_t rows = srcLen / stride;
	const int32_t kept = (rows + rowStep - 1) / rowStep;
	return pageEncodeStride(src, kept * stride, stride, rowStep, dst, dstCap);
}

bool pageRleDecode(const uint8_t *src, int32_t srcLen, uint8_t *dst,
                   int32_t dstLen) {
	if (src == 0 || dst == 0 || srcLen < 0 || dstLen < 0) {
		return false;
	}

	int32_t in = 0;
	int32_t out = 0;

	while (in < srcLen) {
		const uint8_t control = src[in++];
		const int32_t count = (int32_t)(control & 0x7F) + 1;

		if (control & 0x80) {
			if (in >= srcLen || out + count > dstLen) {
				return false;
			}
			const uint8_t value = src[in++];
			for (int32_t i = 0; i < count; ++i) {
				dst[out++] = value;
			}
		} else {
			if (in + count > srcLen || out + count > dstLen) {
				return false;
			}
			for (int32_t i = 0; i < count; ++i) {
				dst[out++] = src[in++];
			}
		}
	}

	return out == dstLen;
}

bool pageDeltaDecode(const uint8_t *src, int32_t srcLen, uint8_t *dst,
                     int32_t dstLen, int32_t stride, int32_t rowStep) {
	if (stride <= 0 || rowStep < 1 || dstLen < 0 || (dstLen % stride) != 0) {
		return false;
	}

	const int32_t rows = dstLen / stride;
	const int32_t kept = (rows + rowStep - 1) / rowStep;
	const int32_t keptLen = kept * stride;

	if (!pageRleDecode(src, srcLen, dst, keptLen)) {
		return false;
	}

	for (int32_t i = stride; i < keptLen; ++i) {
		dst[i] = (uint8_t)(dst[i] ^ dst[i - stride]);
	}

	for (int32_t r = kept - 1; r >= 0; --r) {
		for (int32_t rep = rowStep - 1; rep >= 0; --rep) {
			const int32_t out = r * rowStep + rep;
			if (out >= rows) {
				continue;
			}
			for (int32_t i = stride - 1; i >= 0; --i) {
				dst[out * stride + i] = dst[r * stride + i];
			}
		}
	}

	return true;
}
