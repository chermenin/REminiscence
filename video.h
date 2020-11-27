
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef VIDEO_H__
#define VIDEO_H__

#include "intern.h"

struct Resource;
struct SystemStub;

struct Video {
	typedef void (Video::*drawCharFunc)(uint8_t *, int, int, int, const uint8_t *, uint8_t, uint8_t);

	enum {
		GAMESCREEN_W = 256,
		GAMESCREEN_H = 224,
		SCREENBLOCK_W = 8,
		SCREENBLOCK_H = 8,
		CHAR_W = 8,
		CHAR_H = 8
	};

	static const uint8_t _conradPal1[];
	static const uint8_t _conradPal2[];
	static const uint8_t _textPal[];
	static const uint8_t _palSlot0xF[];
	static const uint8_t _font8Jp[];

	Resource *_res;
	SystemStub *_stub;
	WidescreenMode _widescreenMode;

	int _w, _h;
	int _layerSize;
	int _layerScale; // 1 for Amiga/PC, 2 for Macintosh
	uint8_t *_frontLayer;
	uint8_t *_backLayer;
	uint8_t *_tempLayer;
	uint8_t *_tempLayer2;
	uint8_t _unkPalSlot1, _unkPalSlot2;
	uint8_t _mapPalSlot1, _mapPalSlot2, _mapPalSlot3, _mapPalSlot4;
	uint8_t _charFrontColor;
	uint8_t _charTransparentColor;
	uint8_t _charShadowColor;
	uint8_t *_screenBlocks;
	bool _fullRefresh;
	uint8_t _shakeOffset;
	drawCharFunc _drawChar;

	Video(Resource *res, SystemStub *stub, WidescreenMode widescreenMode);
	~Video();

	void markBlockAsDirty(int16_t x, int16_t y, uint16_t w, uint16_t h, int scale);
	void updateScreen();
	void updateWidescreen();
	void fullRefresh();
	void fadeOut();
	void fadeOutPalette();
	void setPaletteColorBE(int num, int offset);
	void setPaletteSlotBE(int palSlot, int palNum);
	void setPaletteSlotLE(int palSlot, const uint8_t *palData);
	void setTextPalette();
	void setPalette0xF();
	void PC_decodeLev(int level, int room);
	void PC_decodeMap(int level, int room);
	void PC_setLevelPalettes();
	void PC_decodeIcn(const uint8_t *src, int num, uint8_t *dst);
	void PC_decodeSpc(const uint8_t *src, int w, int h, uint8_t *dst);
	void PC_decodeSpm(const uint8_t *dataPtr, uint8_t *dstPtr);
	void AMIGA_decodeLev(int level, int room);
	void AMIGA_decodeSpm(const uint8_t *src, uint8_t *dst);
	void AMIGA_decodeIcn(const uint8_t *src, int num, uint8_t *dst);
	void AMIGA_decodeSpc(const uint8_t *src, int w, int h, uint8_t *dst);
	void AMIGA_decodeCmp(const uint8_t *src, uint8_t *dst);
	void drawSpriteSub1(const uint8_t *src, uint8_t *dst, int pitch, int h, int w, uint8_t colMask);
	void drawSpriteSub2(const uint8_t *src, uint8_t *dst, int pitch, int h, int w, uint8_t colMask);
	void drawSpriteSub3(const uint8_t *src, uint8_t *dst, int pitch, int h, int w, uint8_t colMask);
	void drawSpriteSub4(const uint8_t *src, uint8_t *dst, int pitch, int h, int w, uint8_t colMask);
	void drawSpriteSub5(const uint8_t *src, uint8_t *dst, int pitch, int h, int w, uint8_t colMask);
	void drawSpriteSub6(const uint8_t *src, uint8_t *dst, int pitch, int h, int w, uint8_t colMask);
	void PC_drawChar(uint8_t c, int16_t y, int16_t x, bool forceDefaultFont = false);
	void PC_drawStringChar(uint8_t *dst, int pitch, int x, int y, const uint8_t *src, uint8_t color, uint8_t chr);
	void AMIGA_drawStringChar(uint8_t *dst, int pitch, int x, int y, const uint8_t *src, uint8_t color, uint8_t chr);
	void MAC_drawStringChar(uint8_t *dst, int pitch, int x, int y, const uint8_t *src, uint8_t color, uint8_t chr);
	const char *drawString(const char *str, int16_t x, int16_t y, uint8_t col);
	void drawStringLen(const char *str, int len, int x, int y, uint8_t color);
	static Color AMIGA_convertColor(const uint16_t color, bool bgr = false);
	void MAC_decodeMap(int level, int room);
	static void MAC_setPixel(DecodeBuffer *buf, int x, int y, uint8_t color);
	static void MAC_setPixelMask(DecodeBuffer *buf, int x, int y, uint8_t color);
	static void MAC_setPixelFont(DecodeBuffer *buf, int x, int y, uint8_t color);
	void fillRect(int x, int y, int w, int h, uint8_t color);
	void MAC_drawSprite(int x, int y, const uint8_t *data, int frame, bool xflip, bool eraseBackground);
};

#endif // VIDEO_H__
