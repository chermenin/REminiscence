
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "unpack.h"
#include "util.h"

struct UnpackCtx {
	int size;
	uint32_t crc;
	uint32_t bits;
	uint8_t *dst;
	const uint8_t *src;
};

static bool nextBit(UnpackCtx *uc) {
	bool bit = (uc->bits & 1) != 0;
	uc->bits >>= 1;
	if (uc->bits == 0) { // getnextlwd
		const uint32_t bits = READ_BE_UINT32(uc->src); uc->src -= 4;
		uc->crc ^= bits;
		bit = (bits & 1) != 0;
		uc->bits = (1 << 31) | (bits >> 1);
	}
	return bit;
}

template<int count>
static uint32_t getBits(UnpackCtx *uc) { // rdd1bits
	uint32_t bits = 0;
	for (int i = 0; i < count; ++i) {
		bits |= (nextBit(uc) ? 1 : 0) << (count - 1 - i);
	}
	return bits;
}

static void copyLiteral(UnpackCtx *uc, int len) { // getd3chr
	uc->size -= len;
	if (uc->size < 0) {
		len += uc->size;
		uc->size = 0;
	}
	for (int i = 0; i < len; ++i) {
		*(uc->dst - i) = (uint8_t)getBits<8>(uc);
	}
	uc->dst -= len;
}

static void copyReference(UnpackCtx *uc, int len, int offset) { // copyd3bytes
	uc->size -= len;
	if (uc->size < 0) {
		len += uc->size;
		uc->size = 0;
	}
	for (int i = 0; i < len; ++i) {
		*(uc->dst - i) = *(uc->dst - i + offset);
	}
	uc->dst -= len;
}

bool bytekiller_unpack(uint8_t *dst, int dstSize, const uint8_t *src, int srcSize) {
	UnpackCtx uc;
	uc.src = src + srcSize - 4;
	uc.size = READ_BE_UINT32(uc.src); uc.src -= 4;
	if (uc.size > dstSize) {
		warning("Unexpected unpack size %d, buffer size %d", uc.size, dstSize);
		return false;
	}
	uc.dst = dst + uc.size - 1;
	uc.crc = READ_BE_UINT32(uc.src); uc.src -= 4;
	uc.bits = READ_BE_UINT32(uc.src); uc.src -= 4;
	uc.crc ^= uc.bits;
	do {
		if (!nextBit(&uc)) {
			if (!nextBit(&uc)) {
				copyLiteral(&uc, getBits<3>(&uc) + 1);
			} else {
				copyReference(&uc, 2, getBits<8>(&uc));
			}
		} else {
			const int code = getBits<2>(&uc);
			switch (code) {
			case 3:
				copyLiteral(&uc, getBits<8>(&uc) + 9);
				break;
			case 2: {
					const int len = getBits<8>(&uc) + 1;
					copyReference(&uc, len, getBits<12>(&uc));
				}
				break;
			case 1:
				copyReference(&uc, 4, getBits<10>(&uc));
				break;
			case 0:
				copyReference(&uc, 3, getBits<9>(&uc));
				break;
			}
		}
	} while (uc.size > 0);
	assert(uc.size == 0);
	return uc.crc == 0;
}
