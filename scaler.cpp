
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "scaler.h"
#include "util.h"

static void scanline2x(uint32_t *dst0, uint32_t *dst1, const uint32_t *src0, const uint32_t *src1, const uint32_t *src2, int w) {
	uint32_t B, D, E, F, H;

	// ABC
	// DEF
	// GHI

	int x = 0;

	// first pixel (D == E)
	B = *(src0 + x);
	E = *(src1 + x);
	D = E;
	F = *(src1 + x + 1);
	H = *(src2 + x);
	if (B != H && D != F) {
		dst0[0] = D == B ? D : E;
		dst0[1] = B == F ? F : E;
		dst1[0] = D == H ? D : E;
		dst1[1] = H == F ? F : E;
	} else {
		dst0[0] = E;
		dst0[1] = E;
		dst1[0] = E;
		dst1[1] = E;
	}
	dst0 += 2;
	dst1 += 2;

	// center pixels
	E = F;
	for (x = 1; x < w - 1; ++x) {
		B = *(src0 + x);
		F = *(src1 + x + 1);
		H = *(src2 + x);
		if (B != H && D != F) {
			dst0[0] = D == B ? D : E;
			dst0[1] = B == F ? F : E;
			dst1[0] = D == H ? D : E;
			dst1[1] = H == F ? F : E;
		} else {
			dst0[0] = E;
			dst0[1] = E;
			dst1[0] = E;
			dst1[1] = E;
		}
		D = E; E = F;
		dst0 += 2;
		dst1 += 2;
	}

	// last pixel (F == E)
	B = *(src0 + x);
	H = *(src2 + x);
	if (B != H && D != F) {
		dst0[0] = D == B ? D : E;
		dst0[1] = B == F ? F : E;
		dst1[0] = D == H ? D : E;
		dst1[1] = H == F ? F : E;
	} else {
		dst0[0] = E;
		dst0[1] = E;
		dst1[0] = E;
		dst1[1] = E;
	}
}

static void scale2x(uint32_t *dst, int dstPitch, const uint32_t *src, int srcPitch, int w, int h) {
	assert(w > 1 && h > 1);
	const int dstPitch2 = dstPitch * 2;

	const uint32_t *src0, *src1, *src2;

	// y == 0
	src0 = src;
	src1 = src;
	src2 = src + srcPitch;
	scanline2x(dst, dst + dstPitch, src0, src1, src2, w);
	dst += dstPitch2;

	// center
	src0 = src;
	src1 = src + srcPitch;
	src2 = src + srcPitch * 2;
	for (int y = 1; y < h - 1; ++y) {
		scanline2x(dst, dst + dstPitch, src0, src1, src2, w);
		dst += dstPitch2;

		src0 += srcPitch;
		src1 += srcPitch;
		src2 += srcPitch;
	}

	// y == h-1
	src2 = src1;
	scanline2x(dst, dst + dstPitch, src0, src1, src2, w);
}

static void scanline3x(uint32_t *dst0, uint32_t *dst1, uint32_t *dst2, const uint32_t *src0, const uint32_t *src1, const uint32_t *src2, int w) {
	uint32_t A, B, C, D, E, F, G, H, I;

	// ABC
	// DEF
	// GHI

	int x = 0;

	// first pixel (A == B, D == E and G == H)
	B = *(src0 + x);
	A = B;
	C = *(src0 + x + 1);
	E = *(src1 + x);
	D = E;
	F = *(src1 + x + 1);
	H = *(src2 + x);
	G = H;
	I = *(src2 + x + 1);
	if (B != H && D != F) {
		dst0[0] = D == B ? D : E;
		dst0[1] = (E == B && E != C) || (B == F && E != A) ? B : E;
		dst0[2] = B == F ? F : E;
		dst1[0] = (D == B && E != G) || (D == B && E != A) ? D : E;
		dst1[1] = E;
		dst1[2] = (B == F && E != I) || (H == F && E != C) ? F : E;
		dst2[0] = D == H ? D : E;
		dst2[1] = (D == H && E != I) || (H == F && E != G) ? H : E;
		dst2[2] = H == F ? F : E;
	} else {
		dst0[0] = E;
		dst0[1] = E;
		dst0[2] = E;
		dst1[0] = E;
		dst1[1] = E;
		dst1[2] = E;
		dst2[0] = E;
		dst2[1] = E;
		dst2[2] = E;
	}
	dst0 += 3;
	dst1 += 3;
	dst2 += 3;

	// center pixels
	B = C;
	E = F;
	H = I;
	for (x = 1; x < w - 1; ++x) {
		C = *(src0 + x + 1);
		F = *(src1 + x + 1);
		I = *(src2 + x + 1);
		if (B != H && D != F) {
			dst0[0] = D == B ? D : E;
			dst0[1] = (E == B && E != C) || (B == F && E != A) ? B : E;
			dst0[2] = B == F ? F : E;
			dst1[0] = (D == B && E != G) || (D == B && E != A) ? D : E;
			dst1[1] = E;
			dst1[2] = (B == F && E != I) || (H == F && E != C) ? F : E;
			dst2[0] = D == H ? D : E;
			dst2[1] = (D == H && E != I) || (H == F && E != G) ? H : E;
			dst2[2] = H == F ? F : E;
		} else {
			dst0[0] = E;
			dst0[1] = E;
			dst0[2] = E;
			dst1[0] = E;
			dst1[1] = E;
			dst1[2] = E;
			dst2[0] = E;
			dst2[1] = E;
			dst2[2] = E;
		}
		A = B; B = C;
		D = E; E = F;
		G = H; H = I;
		dst0 += 3;
		dst1 += 3;
		dst2 += 3;
	}

	// last pixel (B == C, E == F and H == I)
	if (B != H && D != F) {
		dst0[0] = D == B ? D : E;
		dst0[1] = (E == B && E != C) || (B == F && E != A) ? B : E;
		dst0[2] = B == F ? F : E;
		dst1[0] = (D == B && E != G) || (D == B && E != A) ? D : E;
		dst1[1] = E;
		dst1[2] = (B == F && E != I) || (H == F && E != C) ? F : E;
		dst2[0] = D == H ? D : E;
		dst2[1] = (D == H && E != I) || (H == F && E != G) ? H : E;
		dst2[2] = H == F ? F : E;
	} else {
		dst0[0] = E;
		dst0[1] = E;
		dst0[2] = E;
		dst1[0] = E;
		dst1[1] = E;
		dst1[2] = E;
		dst2[0] = E;
		dst2[1] = E;
		dst2[2] = E;
	}
}

static void scale3x(uint32_t *dst, int dstPitch, const uint32_t *src, int srcPitch, int w, int h) {
	assert(w > 1 && h > 1);
	const int dstPitch2 = dstPitch * 2;
	const int dstPitch3 = dstPitch * 3;

	const uint32_t *src0, *src1, *src2;

	// y == 0
	src0 = src;
	src1 = src;
	src2 = src + srcPitch;
	scanline3x(dst, dst + dstPitch, dst + dstPitch2, src0, src1, src2, w);
	dst += dstPitch3;

	// center
	src0 = src;
	src1 = src + srcPitch;
	src2 = src + srcPitch * 2;
	for (int y = 1; y < h - 1; ++y) {
		scanline3x(dst, dst + dstPitch, dst + dstPitch2, src0, src1, src2, w);
		dst += dstPitch3;

		src0 += srcPitch;
		src1 += srcPitch;
		src2 += srcPitch;
	}

	// y == h-1
	src2 = src1;
	scanline3x(dst, dst + dstPitch, dst + dstPitch2, src0, src1, src2, w);
}

static void scale4x(uint32_t *dst, int dstPitch, const uint32_t *src, int srcPitch, int w, int h) {
	static struct {
		uint32_t *ptr;
		int w, h, pitch;
		int size;
	} buf;
	const int size = (w * 2) * (h * 2) * sizeof(uint32_t);
	if (buf.size < size) {
		free(buf.ptr);
		buf.size = size;
		buf.w = w * 2;
		buf.h = h * 2;
		buf.pitch = buf.w;
		buf.ptr = (uint32_t *)malloc(buf.size);
		if (!buf.ptr) {
			error("Unable to allocate scale4x intermediate buffer");
		}
	}
	scale2x(buf.ptr, buf.pitch, src, srcPitch, w, h);
	scale2x(dst, dstPitch, buf.ptr, buf.pitch, buf.w, buf.h);
}

static void scaleNx(int factor, uint32_t *dst, int dstPitch, const uint32_t *src, int srcPitch, int w, int h) {
	switch (factor) {
	case 2:
		return scale2x(dst, dstPitch, src, srcPitch, w, h);
	case 3:
		return scale3x(dst, dstPitch, src, srcPitch, w, h);
	case 4:
		return scale4x(dst, dstPitch, src, srcPitch, w, h);
	}
}

const Scaler _internalScaler = {
	SCALER_TAG,
	"scaleNx",
	2, 4,
	scaleNx,
};
