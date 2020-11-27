
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <math.h>
#include "cutscene.h"
#include "resource.h"
#include "systemstub.h"
#include "util.h"
#include "video.h"

static void scalePoints(Point *pt, int count, int scale) {
	if (scale != 1) {
		while (count-- > 0) {
			pt->x *= scale;
			pt->y *= scale;
			++pt;
		}
	}
}

Cutscene::Cutscene(Resource *res, SystemStub *stub, Video *vid)
	: _res(res), _stub(stub), _vid(vid) {
	_patchedOffsetsTable = 0;
	memset(_palBuf, 0, sizeof(_palBuf));
}

const uint8_t *Cutscene::getCommandData() const {
	return _res->_cmd;
}

const uint8_t *Cutscene::getPolygonData() const {
	return _res->_pol;
}

void Cutscene::sync() {
	if (_stub->_pi.quit) {
		return;
	}
	if (_stub->_pi.dbgMask & PlayerInput::DF_FASTMODE) {
		return;
	}
	const int32_t delay = _stub->getTimeStamp() - _tstamp;
	const int32_t pause = _frameDelay * TIMER_SLICE - delay;
	if (pause > 0) {
		_stub->sleep(pause);
	}
	_tstamp = _stub->getTimeStamp();
}

void Cutscene::copyPalette(const uint8_t *pal, uint16_t num) {
	uint8_t *dst = _palBuf;
	if (num != 0) {
		dst += 0x20;
	}
	memcpy(dst, pal, 0x20);
	_newPal = true;
}

void Cutscene::updatePalette() {
	if (_newPal) {
		const uint8_t *p = _palBuf;
		for (int i = 0; i < 32; ++i) {
			const uint16_t color = READ_BE_UINT16(p); p += 2;
			Color c = Video::AMIGA_convertColor(color);
			_stub->setPaletteEntry(0xC0 + i, &c);
		}
		_newPal = false;
	}
}

void Cutscene::setPalette() {
	sync();
	updatePalette();
	SWAP(_page0, _page1);
	_stub->copyRect(0, 0, _vid->_w, _vid->_h, _page0, _vid->_w);
	_stub->updateScreen(0);
}

#if 0
#define SIN(a) (int16_t)(sin(a * M_PI / 180) * 256)
#define COS(a) (int16_t)(cos(a * M_PI / 180) * 256)
#else
#define SIN(a) _sinTable[a]
#define COS(a) _cosTable[a]
#endif

/*
  cos(60)  table: 128, math: 127
  cos(120) table:-127, math:-128
  cos(240) table:-128, math:-127
  sin(330) table: 221, math:-127
*/

/*
  a = rotation angle
  b = scale/distort vertically (180)
  c = scale/distort horizontally (90)

  | x | cos_a    sin_a | cos_b | cos_c * sin_b |
  | y | sin_a   -cos_a | sin_c |             1 |
*/

void Cutscene::setRotationTransform(uint16_t a, uint16_t b, uint16_t c) { // identity a:0 b:180 c:90
	const int16_t sin_a = SIN(a);
	const int16_t cos_a = COS(a);
	const int16_t sin_c = SIN(c);
	const int16_t cos_c = COS(c);
	const int16_t sin_b = SIN(b);
	const int16_t cos_b = COS(b);
	_rotMat[0] = ((cos_a * cos_b) >> 8) - ((((cos_c * sin_a) >> 8) * sin_b) >> 8);
	_rotMat[1] = ((sin_a * cos_b) >> 8) + ((((cos_c * cos_a) >> 8) * sin_b) >> 8);
	_rotMat[2] = ( sin_c * sin_a) >> 8;
	_rotMat[3] = (-sin_c * cos_a) >> 8;
}

static bool isNewLineChar(uint8_t chr, Resource *res) {
	const uint8_t nl = (res->_lang == LANG_JP) ? 0xD1 : 0x7C;
	return chr == nl;
}

uint16_t Cutscene::findTextSeparators(const uint8_t *p, int len) {
	uint8_t *q = _textSep;
	uint16_t ret = 0;
	uint16_t pos = 0;
	for (int i = 0; i < len && p[i] != 0xA; ++i) {
		if (isNewLineChar(p[i], _res)) {
			*q++ = pos;
			if (pos > ret) {
				ret = pos;
			}
			pos = 0;
		} else {
			++pos;
		}
	}
	*q++ = pos;
	if (pos > ret) {
		ret = pos;
	}
	*q++ = 0;
	return ret;
}

void Cutscene::drawText(int16_t x, int16_t y, const uint8_t *p, uint16_t color, uint8_t *page, int textJustify) {
	debug(DBG_CUT, "Cutscene::drawText(x=%d, y=%d, c=%d, justify=%d)", x, y, color, textJustify);
	int len = 0;
	if (_res->_type == kResourceTypeMac) {
		len = *p++;
	} else {
		len = strlen((const char *)p);
	}
	Video::drawCharFunc dcf = _vid->_drawChar;
	const uint8_t *fnt = (_res->_lang == LANG_JP) ? Video::_font8Jp : _res->_fnt;
	uint16_t lastSep = 0;
	if (textJustify != kTextJustifyLeft) {
		lastSep = findTextSeparators(p, len);
		if (textJustify != kTextJustifyCenter) {
			lastSep = (_res->_lang == LANG_JP) ? 20 : 30;
		}
	}
	const uint8_t *sep = _textSep;
	y += 50;
	x += (_res->_lang == LANG_JP) ? 0 : 8;
	int16_t yPos = y;
	int16_t xPos = x;
	if (textJustify != kTextJustifyLeft) {
		xPos += ((lastSep - *sep++) / 2) * Video::CHAR_W;
	}
	for (int i = 0; i < len && p[i] != 0xA; ++i) {
		if (isNewLineChar(p[i], _res)) {
			yPos += Video::CHAR_H;
			xPos = x;
			if (textJustify != kTextJustifyLeft) {
				xPos += ((lastSep - *sep++) / 2) * Video::CHAR_W;
			}
		} else if (p[i] == 0x20) {
			xPos += Video::CHAR_W;
		} else if (p[i] == 0x9) {
			// ignore tab
		} else {
			(_vid->*dcf)(page, _vid->_w, xPos, yPos, fnt, color, p[i]);
			xPos += Video::CHAR_W;
		}
	}
}

void Cutscene::swapLayers() {
	if (_clearScreen == 0) {
		memcpy(_page1, _pageC, _vid->_layerSize);
	} else {
		memset(_page1, 0xC0, _vid->_layerSize);
	}
}

void Cutscene::drawCreditsText() {
	if (_creditsSequence) {
		if (_creditsKeepText != 0) {
			if (_creditsSlowText == 0) {
				_creditsKeepText = 0;
			} else {
				return;
			}
		}
		if (_creditsTextCounter <= 0) {
			const uint8_t code = *_textCurPtr;
			if (code == 0xFF) {
				_textBuf[0] = 0xA;
			} else if (code == 0xFE) {
				++_textCurPtr;
				_creditsTextCounter = *_textCurPtr++;
			} else if (code == 1) {
				++_textCurPtr;
				_creditsTextPosX = *_textCurPtr++;
				_creditsTextPosY = *_textCurPtr++;
			} else if (code == 0) {
				_textCurBuf = _textBuf;
				_textBuf[0] = 0xA;
				++_textCurPtr;
				if (_creditsSlowText != 0) {
					_creditsKeepText = 0xFF;
				}
			} else {
				*_textCurBuf++ = code;
				*_textCurBuf = 0xA;
				++_textCurPtr;
			}
		} else {
			_creditsTextCounter -= 10;
		}
		drawText((_creditsTextPosX - 1) * 8, _creditsTextPosY * 8, _textBuf, 0xEF, _page1, kTextJustifyLeft);
	}
}

void Cutscene::drawProtectionShape(uint8_t shapeNum, int16_t zoom) {
	debug(DBG_CUT, "Cutscene::drawProtectionShape() shapeNum = %d", shapeNum);
	_shape_ix = 64;
	_shape_iy = 64;
	_shape_count = 0;

	int16_t x = 0;
	int16_t y = 0;
	zoom += 512;

	const uint8_t *shapeOffsetTable    = _protectionShapeData + READ_BE_UINT16(_protectionShapeData + 0x02);
	const uint8_t *shapeDataTable      = _protectionShapeData + READ_BE_UINT16(_protectionShapeData + 0x0E);
	const uint8_t *verticesOffsetTable = _protectionShapeData + READ_BE_UINT16(_protectionShapeData + 0x0A);
	const uint8_t *verticesDataTable   = _protectionShapeData + READ_BE_UINT16(_protectionShapeData + 0x12);

	++shapeNum;
	const uint8_t *shapeData = shapeDataTable + READ_BE_UINT16(shapeOffsetTable + (shapeNum & 0x7FF) * 2);
	uint16_t primitiveCount = READ_BE_UINT16(shapeData); shapeData += 2;

	while (primitiveCount--) {
		uint16_t verticesOffset = READ_BE_UINT16(shapeData); shapeData += 2;
		const uint8_t *p = verticesDataTable + READ_BE_UINT16(verticesOffsetTable + (verticesOffset & 0x3FFF) * 2);
		int16_t dx = 0;
		int16_t dy = 0;
		if (verticesOffset & 0x8000) {
			dx = READ_BE_UINT16(shapeData); shapeData += 2;
			dy = READ_BE_UINT16(shapeData); shapeData += 2;
		}
		_hasAlphaColor = (verticesOffset & 0x4000) != 0;
		_primitiveColor = 0xC0 + *shapeData++;
		drawShapeScale(p, zoom, dx, dy, x, y, 0, 0);
		++_shape_count;
	}
}

void Cutscene::op_markCurPos() {
	debug(DBG_CUT, "Cutscene::op_markCurPos()");
	_cmdPtrBak = _cmdPtr;
	drawCreditsText();
	_frameDelay = 5;
	setPalette();
	swapLayers();
	_creditsSlowText = 0;
}

void Cutscene::op_refreshScreen() {
	debug(DBG_CUT, "Cutscene::op_refreshScreen()");
	_clearScreen = fetchNextCmdByte();
	if (_clearScreen != 0) {
		swapLayers();
		_creditsSlowText = 0;
	}
}

void Cutscene::op_waitForSync() {
	debug(DBG_CUT, "Cutscene::op_waitForSync()");
	if (_creditsSequence) {
		uint16_t n = fetchNextCmdByte() * 2;
		do {
			_creditsSlowText = 0xFF;
			_frameDelay = 3;
			if (_textBuf == _textCurBuf) {
				_creditsTextCounter = _res->isAmiga() ? 60 : 20;
			}
			memcpy(_page1, _page0, _vid->_layerSize);
			drawCreditsText();
			setPalette();
		} while (--n);
		swapLayers();
		_creditsSlowText = 0;
	} else {
		_frameDelay = fetchNextCmdByte() * 4;
		sync(); // XXX handle input
	}
}

void Cutscene::drawShape(const uint8_t *data, int16_t x, int16_t y) {
	debug(DBG_CUT, "Cutscene::drawShape()");
	_gfx.setLayer(_page1, _vid->_w);
	uint8_t numVertices = *data++;
	if (numVertices & 0x80) {
		Point pt;
		pt.x = READ_BE_UINT16(data) + x; data += 2;
		pt.y = READ_BE_UINT16(data) + y; data += 2;
		uint16_t rx = READ_BE_UINT16(data); data += 2;
		uint16_t ry = READ_BE_UINT16(data); data += 2;
		scalePoints(&pt, 1, _vid->_layerScale);
		_gfx.drawEllipse(_primitiveColor, _hasAlphaColor, &pt, rx, ry);
	} else if (numVertices == 0) {
		Point pt;
		pt.x = READ_BE_UINT16(data) + x; data += 2;
		pt.y = READ_BE_UINT16(data) + y; data += 2;
		scalePoints(&pt, 1, _vid->_layerScale);
		_gfx.drawPoint(_primitiveColor, &pt);
	} else {
		Point *pt = _vertices;
		int16_t ix = READ_BE_UINT16(data); data += 2;
		int16_t iy = READ_BE_UINT16(data); data += 2;
		pt->x = ix + x;
		pt->y = iy + y;
		++pt;
		int16_t n = numVertices - 1;
		++numVertices;
		for (; n >= 0; --n) {
			int16_t dx = (int8_t)*data++;
			int16_t dy = (int8_t)*data++;
			if (dy == 0 && n != 0 && *(data + 1) == 0) {
				ix += dx;
				--numVertices;
			} else {
				ix += dx;
				iy += dy;
				pt->x = ix + x;
				pt->y = iy + y;
				++pt;
			}
		}
		scalePoints(_vertices, numVertices, _vid->_layerScale);
		_gfx.drawPolygon(_primitiveColor, _hasAlphaColor, _vertices, numVertices);
	}
}

void Cutscene::op_drawShape() {
	debug(DBG_CUT, "Cutscene::op_drawShape()");

	int16_t x = 0;
	int16_t y = 0;
	uint16_t shapeOffset = fetchNextCmdWord();
	if (shapeOffset & 0x8000) {
		x = fetchNextCmdWord();
		y = fetchNextCmdWord();
	}

	const uint8_t *shapeOffsetTable    = _polPtr + READ_BE_UINT16(_polPtr + 0x02);
	const uint8_t *shapeDataTable      = _polPtr + READ_BE_UINT16(_polPtr + 0x0E);
	const uint8_t *verticesOffsetTable = _polPtr + READ_BE_UINT16(_polPtr + 0x0A);
	const uint8_t *verticesDataTable   = _polPtr + READ_BE_UINT16(_polPtr + 0x12);

	const uint8_t *shapeData = shapeDataTable + READ_BE_UINT16(shapeOffsetTable + (shapeOffset & 0x7FF) * 2);
	uint16_t primitiveCount = READ_BE_UINT16(shapeData); shapeData += 2;

	while (primitiveCount--) {
		uint16_t verticesOffset = READ_BE_UINT16(shapeData); shapeData += 2;
		const uint8_t *primitiveVertices = verticesDataTable + READ_BE_UINT16(verticesOffsetTable + (verticesOffset & 0x3FFF) * 2);
		int16_t dx = 0;
		int16_t dy = 0;
		if (verticesOffset & 0x8000) {
			dx = READ_BE_UINT16(shapeData); shapeData += 2;
			dy = READ_BE_UINT16(shapeData); shapeData += 2;
		}
		_hasAlphaColor = (verticesOffset & 0x4000) != 0;
		uint8_t color = *shapeData++;
		if (_clearScreen == 0) {
			color += 0x10;
		}
		_primitiveColor = 0xC0 + color;
		drawShape(primitiveVertices, x + dx, y + dy);
	}
	if (_clearScreen != 0) {
		memcpy(_pageC, _page1, _vid->_layerSize);
	}
}

void Cutscene::op_setPalette() {
	debug(DBG_CUT, "Cutscene::op_setPalette()");
	uint8_t num = fetchNextCmdByte();
	uint8_t palNum = fetchNextCmdByte();
	uint16_t off = READ_BE_UINT16(_polPtr + 6);
	const uint8_t *p = _polPtr + off + num * 32;
	copyPalette(p, palNum ^ 1);
	if (_creditsSequence) {
		_palBuf[0x20] = 0x0F;
		_palBuf[0x21] = 0xFF;
	}
}

void Cutscene::op_drawCaptionText() {
	debug(DBG_CUT, "Cutscene::op_drawCaptionText()");
	uint16_t strId = fetchNextCmdWord();
	if (!_creditsSequence) {

		// 'espions' - ignore last call, allows caption to be displayed longer on the screen
		if (_id == 0x39 && strId == 0xFFFF) {
			if ((_res->isDOS() && (_cmdPtr - _cmdPtrBak) == 0x10) || (_res->isAmiga() && (_cmdPtr - getCommandData()) == 0x9F3)) {
				_frameDelay = 100;
				setPalette();
				return;
			}
		}

		const int h = 45 * _vid->_layerScale;
		const int y = Video::GAMESCREEN_H * _vid->_layerScale - h;

		memset(_pageC + y * _vid->_w, 0xC0, h * _vid->_w);
		memset(_page1 + y * _vid->_w, 0xC0, h * _vid->_w);
		memset(_page0 + y * _vid->_w, 0xC0, h * _vid->_w);
		if (strId != 0xFFFF) {
			const uint8_t *str = _res->getCineString(strId);
			if (str) {
				drawText(0, 129, str, 0xEF, _page1, kTextJustifyAlign);
				drawText(0, 129, str, 0xEF, _pageC, kTextJustifyAlign);
			}
		}
	}
}

void Cutscene::op_nop() {
	debug(DBG_CUT, "Cutscene::op_nop()");
}

void Cutscene::op_skip3() {
	debug(DBG_CUT, "Cutscene::op_skip3()");
	_cmdPtr += 3;
}

void Cutscene::op_refreshAll() {
	debug(DBG_CUT, "Cutscene::op_refreshAll()");
	_frameDelay = 5;
	setPalette();
	swapLayers();
	_creditsSlowText = 0xFF;
	op_handleKeys();
}

void Cutscene::drawShapeScale(const uint8_t *data, int16_t zoom, int16_t b, int16_t c, int16_t d, int16_t e, int16_t f, int16_t g) {
	debug(DBG_CUT, "Cutscene::drawShapeScale(%d, %d, %d, %d, %d, %d, %d)", zoom, b, c, d, e, f, g);
	_gfx.setLayer(_page1, _vid->_w);
	uint8_t numVertices = *data++;
	if (numVertices & 0x80) {
		int16_t x, y;
		Point *pt = _vertices;
		Point pr[2];
		_shape_cur_x = b + READ_BE_UINT16(data); data += 2;
		_shape_cur_y = c + READ_BE_UINT16(data); data += 2;
		x = READ_BE_UINT16(data); data += 2;
		y = READ_BE_UINT16(data); data += 2;
		_shape_cur_x16 = 0;
		_shape_cur_y16 = 0;
		pr[0].x =  0;
		pr[0].y = -y;
		pr[1].x = -x;
		pr[1].y =  y;
		if (_shape_count == 0) {
			f -= ((((_shape_ix - _shape_ox) * zoom) * 128) + 0x8000) >> 16;
			g -= ((((_shape_iy - _shape_oy) * zoom) * 128) + 0x8000) >> 16;
			pt->x = f;
			pt->y = g;
			++pt;
			_shape_cur_x16 = f << 16;
			_shape_cur_y16 = g << 16;
		} else {
			_shape_cur_x16 = _shape_prev_x16 + ((_shape_cur_x - _shape_prev_x) * zoom) * 128;
			pt->x = (_shape_cur_x16 + 0x8000) >> 16;
			_shape_cur_y16 = _shape_prev_y16 + ((_shape_cur_y - _shape_prev_y) * zoom) * 128;
			pt->y = (_shape_cur_y16 + 0x8000) >> 16;
			++pt;
		}
		for (int i = 0; i < 2; ++i) {
			_shape_cur_x += pr[i].x;
			_shape_cur_x16 += pr[i].x * zoom * 128;
			pt->x = (_shape_cur_x16 + 0x8000) >> 16;
			_shape_cur_y += pr[i].y;
			_shape_cur_y16 += pr[i].y * zoom * 128;
			pt->y = (_shape_cur_y16 + 0x8000) >> 16;
			++pt;
		}
		_shape_prev_x = _shape_cur_x;
		_shape_prev_y = _shape_cur_y;
		_shape_prev_x16 = _shape_cur_x16;
		_shape_prev_y16 = _shape_cur_y16;
		Point po;
		po.x = _vertices[0].x + d + _shape_ix;
		po.y = _vertices[0].y + e + _shape_iy;
		int16_t rx = _vertices[0].x - _vertices[2].x;
		int16_t ry = _vertices[0].y - _vertices[1].y;
		scalePoints(&po, 1, _vid->_layerScale);
		_gfx.drawEllipse(_primitiveColor, _hasAlphaColor, &po, rx, ry);
	} else if (numVertices == 0) {
		Point pt;
 		pt.x = _shape_cur_x = b + READ_BE_UINT16(data); data += 2;
	 	pt.y = _shape_cur_y = c + READ_BE_UINT16(data); data += 2;
 		if (_shape_count == 0) {
			f -= ((((_shape_ix - pt.x) * zoom) * 128) + 0x8000) >> 16;
			g -= ((((_shape_iy - pt.y) * zoom) * 128) + 0x8000) >> 16;
			pt.x = f + _shape_ix + d;
			pt.y = g + _shape_iy + e;
			_shape_cur_x16 = f << 16;
			_shape_cur_y16 = g << 16;
		} else {
			_shape_cur_x16 = _shape_prev_x16 + ((pt.x - _shape_prev_x) * zoom) * 128;
			_shape_cur_y16 = _shape_prev_y16 + ((pt.y - _shape_prev_y) * zoom) * 128;
			pt.x = ((_shape_cur_x16 + 0x8000) >> 16) + _shape_ix + d;
			pt.y = ((_shape_cur_y16 + 0x8000) >> 16) + _shape_iy + e;
		}
		_shape_prev_x = _shape_cur_x;
		_shape_prev_y = _shape_cur_y;
		_shape_prev_x16 = _shape_cur_x16;
		_shape_prev_y16 = _shape_cur_y16;
		scalePoints(&pt, 1, _vid->_layerScale);
		_gfx.drawPoint(_primitiveColor, &pt);
	} else {
		Point *pt = _vertices;
		int16_t ix, iy;
		_shape_cur_x = ix = READ_BE_UINT16(data) + b; data += 2;
		_shape_cur_y = iy = READ_BE_UINT16(data) + c; data += 2;
		if (_shape_count == 0) {
			f -= ((((_shape_ix - _shape_ox) * zoom) * 128) + 0x8000) >> 16;
			g -= ((((_shape_iy - _shape_oy) * zoom) * 128) + 0x8000) >> 16;
			pt->x = f + _shape_ix + d;
			pt->y = g + _shape_iy + e;
			++pt;
			_shape_cur_x16 = f << 16;
			_shape_cur_y16 = g << 16;
		} else {
			_shape_cur_x16 = _shape_prev_x16 + ((_shape_cur_x - _shape_prev_x) * zoom) * 128;
			_shape_cur_y16 = _shape_prev_y16 + ((_shape_cur_y - _shape_prev_y) * zoom) * 128;
			pt->x = ix = ((_shape_cur_x16 + 0x8000) >> 16) + _shape_ix + d;
			pt->y = iy = ((_shape_cur_y16 + 0x8000) >> 16) + _shape_iy + e;
			++pt;
		}
		int16_t n = numVertices - 1;
		++numVertices;
		int16_t sx = 0;
		for (; n >= 0; --n) {
			ix = (int8_t)(*data++) + sx;
			iy = (int8_t)(*data++);
			if (iy == 0 && n != 0 && *(data + 1) == 0) {
				sx = ix;
				--numVertices;
			} else {
				sx = 0;
				_shape_cur_x += ix;
				_shape_cur_y += iy;
				_shape_cur_x16 += ix * zoom * 128;
				_shape_cur_y16 += iy * zoom * 128;
				pt->x = ((_shape_cur_x16 + 0x8000) >> 16) + _shape_ix + d;
				pt->y = ((_shape_cur_y16 + 0x8000) >> 16) + _shape_iy + e;
				++pt;
			}
		}
		_shape_prev_x = _shape_cur_x;
		_shape_prev_y = _shape_cur_y;
		_shape_prev_x16 = _shape_cur_x16;
		_shape_prev_y16 = _shape_cur_y16;
		scalePoints(_vertices, numVertices, _vid->_layerScale);
		_gfx.drawPolygon(_primitiveColor, _hasAlphaColor, _vertices, numVertices);
	}
}

void Cutscene::op_drawShapeScale() {
	debug(DBG_CUT, "Cutscene::op_drawShapeScale()");

	_shape_count = 0;

	int16_t x = 0;
	int16_t y = 0;
	uint16_t shapeOffset = fetchNextCmdWord();
	if (shapeOffset & 0x8000) {
		x = fetchNextCmdWord();
		y = fetchNextCmdWord();
	}

	uint16_t zoom = fetchNextCmdWord() + 512;
	_shape_ix = fetchNextCmdByte();
	_shape_iy = fetchNextCmdByte();

	const uint8_t *shapeOffsetTable    = _polPtr + READ_BE_UINT16(_polPtr + 0x02);
	const uint8_t *shapeDataTable      = _polPtr + READ_BE_UINT16(_polPtr + 0x0E);
	const uint8_t *verticesOffsetTable = _polPtr + READ_BE_UINT16(_polPtr + 0x0A);
	const uint8_t *verticesDataTable   = _polPtr + READ_BE_UINT16(_polPtr + 0x12);

	const uint8_t *shapeData = shapeDataTable + READ_BE_UINT16(shapeOffsetTable + (shapeOffset & 0x7FF) * 2);
	uint16_t primitiveCount = READ_BE_UINT16(shapeData); shapeData += 2;

	if (primitiveCount != 0) {
		uint16_t verticesOffset = READ_BE_UINT16(shapeData);
		int16_t dx = 0;
		int16_t dy = 0;
		if (verticesOffset & 0x8000) {
			dx = READ_BE_UINT16(shapeData + 2);
			dy = READ_BE_UINT16(shapeData + 4);
		}
		const uint8_t *p = verticesDataTable + READ_BE_UINT16(verticesOffsetTable + (verticesOffset & 0x3FFF) * 2) + 1;
		_shape_ox = READ_BE_UINT16(p) + dx; p += 2;
		_shape_oy = READ_BE_UINT16(p) + dy; p += 2;
		while (primitiveCount--) {
			verticesOffset = READ_BE_UINT16(shapeData); shapeData += 2;
			p = verticesDataTable + READ_BE_UINT16(verticesOffsetTable + (verticesOffset & 0x3FFF) * 2);
			dx = 0;
			dy = 0;
			if (verticesOffset & 0x8000) {
				dx = READ_BE_UINT16(shapeData); shapeData += 2;
				dy = READ_BE_UINT16(shapeData); shapeData += 2;
			}
			_hasAlphaColor = (verticesOffset & 0x4000) != 0;
			uint8_t color = *shapeData++;
			if (_clearScreen == 0) {
				color += 0x10; // 2nd pal buf
			}
			_primitiveColor = 0xC0 + color;
			drawShapeScale(p, zoom, dx, dy, x, y, 0, 0);
			++_shape_count;
		}
	}
}

void Cutscene::drawShapeScaleRotate(const uint8_t *data, int16_t zoom, int16_t b, int16_t c, int16_t d, int16_t e, int16_t f, int16_t g) {
	debug(DBG_CUT, "Cutscene::drawShapeScaleRotate(%d, %d, %d, %d, %d, %d, %d)", zoom, b, c, d, e, f, g);
	_gfx.setLayer(_page1, _vid->_w);
	uint8_t numVertices = *data++;
	if (numVertices & 0x80) {
		int16_t x, y, ix, iy;
		Point pr[2];
		Point *pt = _vertices;
		_shape_cur_x = ix = b + READ_BE_UINT16(data); data += 2;
		_shape_cur_y = iy = c + READ_BE_UINT16(data); data += 2;
		x = READ_BE_UINT16(data); data += 2;
		y = READ_BE_UINT16(data); data += 2;
		_shape_cur_x16 = _shape_ix - ix;
		_shape_cur_y16 = _shape_iy - iy;
		_shape_ox = _shape_cur_x = _shape_ix + ((_shape_cur_x16 * _rotMat[0] + _shape_cur_y16 * _rotMat[1]) >> 8);
		_shape_oy = _shape_cur_y = _shape_iy + ((_shape_cur_x16 * _rotMat[2] + _shape_cur_y16 * _rotMat[3]) >> 8);
		pr[0].x =  0;
		pr[0].y = -y;
		pr[1].x = -x;
		pr[1].y =  y;
		if (_shape_count == 0) {
			f -= ((_shape_ix - _shape_cur_x) * zoom * 128 + 0x8000) >> 16;
			g -= ((_shape_iy - _shape_cur_y) * zoom * 128 + 0x8000) >> 16;
			pt->x = f;
			pt->y = g;
			++pt;
			_shape_cur_x16 = f << 16;
			_shape_cur_y16 = g << 16;
		} else {
			_shape_cur_x16 = _shape_prev_x16 + (_shape_cur_x - _shape_prev_x) * zoom * 128;
			_shape_cur_y16 = _shape_prev_y16 + (_shape_cur_y - _shape_prev_y) * zoom * 128;
			pt->x = (_shape_cur_x16 + 0x8000) >> 16;
			pt->y = (_shape_cur_y16 + 0x8000) >> 16;
			++pt;
		}
		for (int i = 0; i < 2; ++i) {
			_shape_cur_x += pr[i].x;
			_shape_cur_x16 += pr[i].x * zoom * 128;
			pt->x = (_shape_cur_x16 + 0x8000) >> 16;
			_shape_cur_y += pr[i].y;
			_shape_cur_y16 += pr[i].y * zoom * 128;
			pt->y = (_shape_cur_y16 + 0x8000) >> 16;
			++pt;
		}
		_shape_prev_x = _shape_cur_x;
		_shape_prev_y = _shape_cur_y;
		_shape_prev_x16 = _shape_cur_x16;
		_shape_prev_y16 = _shape_cur_y16;
		Point po;
		po.x = _vertices[0].x + d + _shape_ix;
		po.y = _vertices[0].y + e + _shape_iy;
		int16_t rx = _vertices[0].x - _vertices[2].x;
		int16_t ry = _vertices[0].y - _vertices[1].y;
		scalePoints(&po, 1, _vid->_layerScale);
		_gfx.drawEllipse(_primitiveColor, _hasAlphaColor, &po, rx, ry);
	} else if (numVertices == 0) {
		Point pt;
		pt.x = b + READ_BE_UINT16(data); data += 2;
		pt.y = c + READ_BE_UINT16(data); data += 2;
		_shape_cur_x16 = _shape_ix - pt.x;
		_shape_cur_y16 = _shape_iy - pt.y;
		_shape_cur_x = _shape_ix + ((_rotMat[0] * _shape_cur_x16 + _rotMat[1] * _shape_cur_y16) >> 8);
		_shape_cur_y = _shape_iy + ((_rotMat[2] * _shape_cur_x16 + _rotMat[3] * _shape_cur_y16) >> 8);
		if (_shape_count != 0) {
			_shape_cur_x16 = _shape_prev_x16 + (_shape_cur_x - _shape_prev_x) * zoom * 128;
			pt.x = ((_shape_cur_x16 + 0x8000) >> 16) + _shape_ix + d;
			_shape_cur_y16 = _shape_prev_y16 + (_shape_cur_y - _shape_prev_y) * zoom * 128;
			pt.y = ((_shape_cur_y16 + 0x8000) >> 16) + _shape_iy + e;
		} else {
			f -= (((_shape_ix - _shape_cur_x) * zoom * 128) + 0x8000) >> 16;
			g -= (((_shape_iy - _shape_cur_y) * zoom * 128) + 0x8000) >> 16;
			_shape_cur_x16 = f << 16;
			_shape_cur_y16 = g << 16;
			pt.x = f + _shape_ix + d;
			pt.y = g + _shape_iy + e;
		}
		_shape_prev_x = _shape_cur_x;
		_shape_prev_y = _shape_cur_y;
		_shape_prev_x16 = _shape_cur_x16;
		_shape_prev_y16 = _shape_cur_y16;
		scalePoints(&pt, 1, _vid->_layerScale);
		_gfx.drawPoint(_primitiveColor, &pt);
	} else {
		int16_t x, y, a, shape_last_x, shape_last_y;
		Point tempVertices[40];
		_shape_cur_x = b + READ_BE_UINT16(data); data += 2;
		x = _shape_cur_x;
		_shape_cur_y = c + READ_BE_UINT16(data); data += 2;
		y = _shape_cur_y;
		_shape_cur_x16 = _shape_ix - x;
		_shape_cur_y16 = _shape_iy - y;

		a = _shape_ix + ((_rotMat[0] * _shape_cur_x16 + _rotMat[1] * _shape_cur_y16) >> 8);
		if (_shape_count == 0) {
			_shape_ox = a;
		}
		_shape_cur_x = shape_last_x = a;
		a = _shape_iy + ((_rotMat[2] * _shape_cur_x16 + _rotMat[3] * _shape_cur_y16) >> 8);
		if (_shape_count == 0) {
			_shape_oy = a;
		}
		_shape_cur_y = shape_last_y = a;

		int16_t ix = x;
		int16_t iy = y;
		Point *pt2 = tempVertices;
		int16_t sx = 0;
		for (int16_t n = numVertices - 1; n >= 0; --n) {
			x = (int8_t)(*data++) + sx;
			y = (int8_t)(*data++);
			if (y == 0 && n != 0 && *(data + 1) == 0) {
				sx = x;
				--numVertices;
			} else {
				ix += x;
				iy += y;
				sx = 0;
				_shape_cur_x16 = _shape_ix - ix;
				_shape_cur_y16 = _shape_iy - iy;
				a = _shape_ix + ((_rotMat[0] * _shape_cur_x16 + _rotMat[1] * _shape_cur_y16) >> 8);
				pt2->x = a - shape_last_x;
				shape_last_x = a;
				a = _shape_iy + ((_rotMat[2] * _shape_cur_x16 + _rotMat[3] * _shape_cur_y16) >> 8);
				pt2->y = a - shape_last_y;
				shape_last_y = a;
				++pt2;
			}
		}
		Point *pt = _vertices;
		if (_shape_count == 0) {
			ix = _shape_ox;
			iy = _shape_oy;
			f -= (((_shape_ix - ix) * zoom * 128) + 0x8000) >> 16;
			g -= (((_shape_iy - iy) * zoom * 128) + 0x8000) >> 16;
			pt->x = f + _shape_ix + d;
			pt->y = g + _shape_iy + e;
			++pt;
			_shape_cur_x16 = f << 16;
			_shape_cur_y16 = g << 16;
		} else {
			_shape_cur_x16 = _shape_prev_x16 + ((_shape_cur_x - _shape_prev_x) * zoom * 128);
			pt->x = _shape_ix + d + ((_shape_cur_x16 + 0x8000) >> 16);
			_shape_cur_y16 = _shape_prev_y16 + ((_shape_cur_y - _shape_prev_y) * zoom * 128);
			pt->y = _shape_iy + e + ((_shape_cur_y16 + 0x8000) >> 16);
			++pt;
		}
		for (int i = 0; i < numVertices; ++i) {
			_shape_cur_x += tempVertices[i].x;
			_shape_cur_x16 += tempVertices[i].x * zoom * 128;
			pt->x = d + _shape_ix + ((_shape_cur_x16 + 0x8000) >> 16);
			_shape_cur_y += tempVertices[i].y;
			_shape_cur_y16 += tempVertices[i].y * zoom * 128;
			pt->y = e + _shape_iy + ((_shape_cur_y16 + 0x8000) >> 16);
			++pt;
		}
		_shape_prev_x = _shape_cur_x;
		_shape_prev_y = _shape_cur_y;
		_shape_prev_x16 = _shape_cur_x16;
		_shape_prev_y16 = _shape_cur_y16;
		scalePoints(_vertices, numVertices + 1, _vid->_layerScale);
		_gfx.drawPolygon(_primitiveColor, _hasAlphaColor, _vertices, numVertices + 1);
	}
}

void Cutscene::op_drawShapeScaleRotate() {
	debug(DBG_CUT, "Cutscene::op_drawShapeScaleRotate()");

	_shape_count = 0;

	int16_t x = 0;
	int16_t y = 0;
	uint16_t shapeOffset = fetchNextCmdWord();
	if (shapeOffset & 0x8000) {
		x = fetchNextCmdWord();
		y = fetchNextCmdWord();
	}

	uint16_t zoom = 512;
	if (shapeOffset & 0x4000) {
		zoom += fetchNextCmdWord();
	}
	_shape_ix = fetchNextCmdByte();
	_shape_iy = fetchNextCmdByte();

	uint16_t r1, r2, r3;
	r1 = fetchNextCmdWord();
	r2 = 180;
	if (shapeOffset & 0x2000) {
		r2 = fetchNextCmdWord();
	}
	r3 = 90;
	if (shapeOffset & 0x1000) {
		r3 = fetchNextCmdWord();
	}
	setRotationTransform(r1, r2, r3);

	const uint8_t *shapeOffsetTable    = _polPtr + READ_BE_UINT16(_polPtr + 0x02);
	const uint8_t *shapeDataTable      = _polPtr + READ_BE_UINT16(_polPtr + 0x0E);
	const uint8_t *verticesOffsetTable = _polPtr + READ_BE_UINT16(_polPtr + 0x0A);
	const uint8_t *verticesDataTable   = _polPtr + READ_BE_UINT16(_polPtr + 0x12);

	const uint8_t *shapeData = shapeDataTable + READ_BE_UINT16(shapeOffsetTable + (shapeOffset & 0x7FF) * 2);
	uint16_t primitiveCount = READ_BE_UINT16(shapeData); shapeData += 2;

	while (primitiveCount--) {
		uint16_t verticesOffset = READ_BE_UINT16(shapeData); shapeData += 2;
		const uint8_t *p = verticesDataTable + READ_BE_UINT16(verticesOffsetTable + (verticesOffset & 0x3FFF) * 2);
		int16_t dx = 0;
		int16_t dy = 0;
		if (verticesOffset & 0x8000) {
			dx = READ_BE_UINT16(shapeData); shapeData += 2;
			dy = READ_BE_UINT16(shapeData); shapeData += 2;
		}
		_hasAlphaColor = (verticesOffset & 0x4000) != 0;
		uint8_t color = *shapeData++;
		if (_clearScreen == 0) {
			color += 0x10; // 2nd pal buf
		}
		_primitiveColor = 0xC0 + color;
		drawShapeScaleRotate(p, zoom, dx, dy, x, y, 0, 0);
		++_shape_count;
	}
}

void Cutscene::op_drawCreditsText() {
	debug(DBG_CUT, "Cutscene::op_drawCreditsText()");
	_creditsSlowText = 0xFF;
	if (_textCurBuf == _textBuf) {
		++_creditsTextCounter;
	}
	memcpy(_page1, _page0, _vid->_layerSize);
	_frameDelay = 10;
	setPalette();
}

void Cutscene::op_drawStringAtPos() {
	debug(DBG_CUT, "Cutscene::op_drawStringAtPos()");
	uint16_t strId = fetchNextCmdWord();
	if (strId != 0xFFFF) {
		int16_t x = (int8_t)fetchNextCmdByte() * 8;
		int16_t y = (int8_t)fetchNextCmdByte() * 8;
		if (!_creditsSequence) {
			const uint8_t *str = _res->getCineString(strId & 0xFFF);
			if (str) {
				uint8_t color = 0xD0 + (strId >> 0xC);
				drawText(x, y, str, color, _page1, kTextJustifyCenter);
			}
			// 'voyage' - cutscene script redraws the string to refresh the screen
			if (_id == 0x34 && (strId & 0xFFF) == 0x45) {
				if ((_cmdPtr - _cmdPtrBak) == 0xA) {
					_stub->copyRect(0, 0, _vid->_w, _vid->_h, _page1, _vid->_w);
					_stub->updateScreen(0);
				} else {
					_stub->sleep(15);
				}
			}
		}
	}
}

void Cutscene::op_handleKeys() {
	debug(DBG_CUT, "Cutscene::op_handleKeys()");
	while (1) {
		uint8_t key_mask = fetchNextCmdByte();
		if (key_mask == 0xFF) {
			return;
		}
		bool b = true;
		switch (key_mask) {
		case 1:
			b = (_stub->_pi.dirMask & PlayerInput::DIR_UP) != 0;
			break;
		case 2:
			b = (_stub->_pi.dirMask & PlayerInput::DIR_DOWN) != 0;
			break;
		case 4:
			b = (_stub->_pi.dirMask & PlayerInput::DIR_LEFT) != 0;
			break;
		case 8:
			b = (_stub->_pi.dirMask & PlayerInput::DIR_RIGHT) != 0;
			break;
		case 0x80:
			b = _stub->_pi.space || _stub->_pi.enter || _stub->_pi.shift;
			break;
		}
		if (b) {
			break;
		}
		_cmdPtr += 2;
	}
	_stub->_pi.dirMask = 0;
	_stub->_pi.enter = false;
	_stub->_pi.space = false;
	_stub->_pi.shift = false;
	int16_t n = fetchNextCmdWord();
	if (n < 0) {
		n = -n - 1;
		if (_varKey == 0) {
			_stop = true;
			return;
		}
		if (_varKey != n) {
			_cmdPtr = _cmdPtrBak;
			return;
		}
		_varKey = 0;
		--n;
		_cmdPtr = getCommandData();
		n = READ_BE_UINT16(_cmdPtr + n * 2 + 2);
	}
	if (_res->isMac()) {
		_cmdPtr = getCommandData();
		_baseOffset = READ_BE_UINT16(_cmdPtr + 2 + n * 2);
		n = 0;
	}
	_cmdPtr = _cmdPtrBak = getCommandData() + n + _baseOffset;
}

uint8_t Cutscene::fetchNextCmdByte() {
	return *_cmdPtr++;
}

uint16_t Cutscene::fetchNextCmdWord() {
	uint16_t i = READ_BE_UINT16(_cmdPtr);
	_cmdPtr += 2;
	return i;
}

void Cutscene::mainLoop(uint16_t num) {
	_frameDelay = 5;
	_tstamp = _stub->getTimeStamp();

	Color c;
	c.r = c.g = c.b = 0;
	for (int i = 0; i < 0x20; ++i) {
		_stub->setPaletteEntry(0xC0 + i, &c);
	}
	_newPal = false;
	_hasAlphaColor = false;
	const uint8_t *p = getCommandData();
	int offset = 0;
	if (_res->isMac()) {
		// const int count = READ_BE_UINT16(p);
		_baseOffset = READ_BE_UINT16(p + 2 + num * 2);
	} else {
		if (num != 0) {
			offset = READ_BE_UINT16(p + 2 + num * 2);
		}
		_baseOffset = (READ_BE_UINT16(p) + 1) * 2;
	}
	_varKey = 0;
	_cmdPtr = _cmdPtrBak = p + _baseOffset + offset;
	_polPtr = getPolygonData();
	debug(DBG_CUT, "_baseOffset = %d offset = %d", _baseOffset, offset);

	while (!_stub->_pi.quit && !_interrupted && !_stop) {
		uint8_t op = fetchNextCmdByte();
		debug(DBG_CUT, "Cutscene::play() opcode = 0x%X (%d)", op, (op >> 2));
		if (op & 0x80) {
			break;
		}
		op >>= 2;
		if (op >= NUM_OPCODES) {
			error("Invalid cutscene opcode = 0x%02X", op);
		}
		(this->*_opcodeTable[op])();
		_stub->processEvents();
		if (_stub->_pi.backspace) {
			_stub->_pi.backspace = false;
			_interrupted = true;
		}
	}
}

bool Cutscene::load(uint16_t cutName) {
	assert(cutName != 0xFFFF);
	const char *name = _namesTableDOS[cutName & 0xFF];
	switch (_res->_type) {
	case kResourceTypeAmiga:
		if (cutName == 7) {
			name = "INTRO";
		} else if (cutName == 10) {
			name = "SERRURE";
		}
		_res->load(name, Resource::OT_CMP);
		if (_id == 0x39 && _res->_lang != LANG_FR) {
			//
			// 'espions' - '... the power which we need' caption is missing in Amiga English.
			// fixed in DOS version, opcodes order is wrong
			//
			// opcode 0 pos 0x323
			// opcode 6 pos 0x324
			// str 0x3a
			//
			uint8_t *p = _res->_cmd + 0x322;
			if (memcmp(p, "\x00\x18\x00\x3a", 4) == 0) {
				p[0] = 0x06 << 2; // op_drawCaptionText
				p[1] = 0x00;
				p[2] = 0x3a;
				p[3] = 0x00; // op_markCurPos
			}
		}
		break;
	case kResourceTypeDOS:
		_res->load(name, Resource::OT_CMD);
		_res->load(name, Resource::OT_POL);
		break;
	case kResourceTypeMac:
		_res->MAC_loadCutscene(name);
		break;
	}
	_res->load_CINE();
	return _res->_cmd && _res->_pol;
}

void Cutscene::unload() {
	switch (_res->_type) {
	case kResourceTypeAmiga:
		_res->unload(Resource::OT_CMP);
		break;
	case kResourceTypeDOS:
		_res->unload(Resource::OT_CMD);
		_res->unload(Resource::OT_POL);
		break;
	case kResourceTypeMac:
		_res->MAC_unloadCutscene();
		break;
	}
}

void Cutscene::prepare() {
	_page0 = _vid->_frontLayer;
	_page1 = _vid->_tempLayer;
	_pageC = _vid->_tempLayer2;
	_stub->_pi.dirMask = 0;
	_stub->_pi.enter = false;
	_stub->_pi.space = false;
	_stub->_pi.shift = false;
	_interrupted = false;
	_stop = false;
	const int w = 240;
	const int h = 128;
	const int x = (Video::GAMESCREEN_W - w) / 2;
	const int y = 50;
	const int sw = w * _vid->_layerScale;
	const int sh = h * _vid->_layerScale;
	const int sx = x * _vid->_layerScale;
	const int sy = y * _vid->_layerScale;
	_gfx.setClippingRect(sx, sy, sw, sh);
}

void Cutscene::playCredits() {
	if (_res->isMac()) {
		warning("Cutscene::playCredits() unimplemented");
		return;
	}
	_textCurPtr = _res->isAmiga() ? _creditsDataAmiga : _creditsDataDOS;
	_textBuf[0] = 0xA;
	_textCurBuf = _textBuf;
	_creditsSequence = true;
	_creditsSlowText = 0;
	_creditsKeepText = 0;
	_creditsTextCounter = 0;
	_interrupted = false;
	const uint16_t *cut_seq = _creditsCutSeq;
	while (!_stub->_pi.quit && !_interrupted) {
		uint16_t cut_id = *cut_seq++;
		if (cut_id == 0xFFFF) {
			break;
		}
		prepare();
		const uint16_t *offsets = _res->isAmiga() ? _offsetsTableAmiga : _offsetsTableDOS;
		uint16_t cutName = offsets[cut_id * 2 + 0];
		uint16_t cutOff  = offsets[cut_id * 2 + 1];
		if (load(cutName)) {
			mainLoop(cutOff);
			unload();
		}
	}
	_creditsSequence = false;
}

void Cutscene::playText(const char *str) {
	Color c;
	// background
	c.r = c.g = c.b = 0;
	_stub->setPaletteEntry(0xC0, &c);
	// text
	c.r = c.g = c.b = 255;
	_stub->setPaletteEntry(0xC1, &c);

	int lines = 0;
	for (int i = 0; str[i]; ++i) {
		if (str[i] == '|') {
			++lines;
		}
	}
	const int y = (128 - lines * 8) / 2;
	memset(_page1, 0xC0, _vid->_layerSize);
	drawText(0, y, (const uint8_t *)str, 0xC1, _page1, kTextJustifyAlign);
	_stub->copyRect(0, 0, _vid->_w, _vid->_h, _page1, _vid->_w);
	_stub->updateScreen(0);

	while (!_stub->_pi.quit) {
		_stub->processEvents();
		if (_stub->_pi.backspace) {
			_stub->_pi.backspace = false;
			break;
		}
		_stub->sleep(TIMER_SLICE);
	}
}

void Cutscene::play() {
	if (_id != 0xFFFF) {
		_textCurBuf = NULL;
		debug(DBG_CUT, "Cutscene::play() _id=0x%X", _id);
		_creditsSequence = false;
		prepare();
		const uint16_t *offsets = _res->isAmiga() ? _offsetsTableAmiga : _offsetsTableDOS;
		uint16_t cutName = offsets[_id * 2 + 0];
		uint16_t cutOff  = offsets[_id * 2 + 1];
		if (cutName == 0xFFFF) {
			switch (_id) {
			case 3: // keys
				if (g_options.play_carte_cutscene) {
					cutName = 2; // CARTE
				}
				break;
			case 8: // save checkpoints
				break;
			case 19:
				if (g_options.play_serrure_cutscene) {
					cutName = 31; // SERRURE
				}
				break;
			case 22: // Level 2 fuse repaired
			case 23: // switches
			case 24: // Level 2 fuse is blown
				if (g_options.play_asc_cutscene) {
					cutName = 12; // ASC
				}
				break;
			case 30:
			case 31:
				if (g_options.play_metro_cutscene) {
					cutName = 14; // METRO
				}
				break;
			case 46: // Level 2 terminal card mission
				break;
			default:
				warning("Unknown cutscene %d", _id);
				break;
			}
		}
		if (_patchedOffsetsTable) {
			for (int i = 0; _patchedOffsetsTable[i] != 255; i += 3) {
				if (_patchedOffsetsTable[i] == _id) {
					cutName = _patchedOffsetsTable[i + 1];
					cutOff = _patchedOffsetsTable[i + 2];
					break;
				}
			}
		}
		if (g_options.use_text_cutscenes) {
			const Text *textsTable = (_res->_lang == LANG_FR) ? _frTextsTable : _enTextsTable;
			for (int i = 0; textsTable[i].str; ++i) {
				if (_id == textsTable[i].num) {
					playText(textsTable[i].str);
					break;
				}
			}
		} else if (cutName != 0xFFFF) {
			if (load(cutName)) {
				mainLoop(cutOff);
				unload();
			}
		} else if (_id == 8 && g_options.play_caillou_cutscene) {
			playSet(_caillouSetData, 0x5E4);
		}
		_vid->fullRefresh();
		if (_id != 0x3D) {
			_id = 0xFFFF;
		}
	}
}

static void readSetPalette(const uint8_t *p, uint16_t offset, uint16_t *palette) {
	offset += 12;
	for (int i = 0; i < 16; ++i) {
		const uint16_t color = READ_BE_UINT16(p + offset); offset += 2;
		palette[i] = color;
	}
}

void Cutscene::drawSetShape(const uint8_t *p, uint16_t offset, int x, int y, uint8_t *paletteLut) {
	const int count = READ_BE_UINT16(p + offset); offset += 2;
	for (int i = 0; i < count - 1; ++i) {
		offset += 5; // shape_marker
		const int verticesCount = p[offset++];
		const int ix = (int16_t)READ_BE_UINT16(p + offset); offset += 2;
		const int iy = (int16_t)READ_BE_UINT16(p + offset); offset += 2;
		uint8_t color = paletteLut[p[offset]]; offset += 2;

		if (verticesCount == 255) {
			int rx = (int16_t)READ_BE_UINT16(p + offset); offset += 2;
			int ry = (int16_t)READ_BE_UINT16(p + offset); offset += 2;
			Point pt;
			pt.x = x + ix;
			pt.y = y + iy;
			scalePoints(&pt, 1, _vid->_layerScale);
			_gfx.drawEllipse(color, false, &pt, rx, ry);
		} else {
			for (int i = 0; i < verticesCount; ++i) {
				_vertices[i].x = x + (int16_t)READ_BE_UINT16(p + offset); offset += 2;
				_vertices[i].y = y + (int16_t)READ_BE_UINT16(p + offset); offset += 2;
			}
			scalePoints(_vertices, verticesCount, _vid->_layerScale);
			_gfx.drawPolygon(color, false, _vertices, verticesCount);
		}
	}
}

static uint16_t readSetShapeOffset(const uint8_t *p, int offset) {
	const int count = READ_BE_UINT16(p + offset); offset += 2;
	for (int i = 0; i < count - 1; ++i) {
		offset += 5; // shape_marker
		const int verticesCount = p[offset++];
		offset += 6;
		if (verticesCount == 255) {
			offset += 4; // ellipse
		} else {
			offset += verticesCount * 4; // polygon
		}
	}
	return offset;
}

static const int kMaxShapesCount = 16;
static const int kMaxPaletteSize = 32;

void Cutscene::playSet(const uint8_t *p, int offset) {
	SetShape backgroundShapes[kMaxShapesCount];
	const int bgCount = READ_BE_UINT16(p + offset); offset += 2;
	assert(bgCount <= kMaxShapesCount);
	for (int i = 0; i < bgCount; ++i) {
		uint16_t nextOffset = readSetShapeOffset(p, offset);
		backgroundShapes[i].offset = offset;
		backgroundShapes[i].size = nextOffset - offset;
		offset = nextOffset + 45;
	}
	SetShape foregroundShapes[kMaxShapesCount];
	const int fgCount = READ_BE_UINT16(p + offset); offset += 2;
	assert(fgCount <= kMaxShapesCount);
	for (int i = 0; i < fgCount; ++i) {
		uint16_t nextOffset = readSetShapeOffset(p, offset);
		foregroundShapes[i].offset = offset;
		foregroundShapes[i].size = nextOffset - offset;
		offset = nextOffset + 45;
	}

	prepare();
	_gfx.setLayer(_page1, _vid->_w);

	offset = 10;
	const int frames = READ_BE_UINT16(p + offset); offset += 2;
	for (int i = 0; i < frames && !_stub->_pi.quit && !_interrupted; ++i) {
		const uint32_t timestamp = _stub->getTimeStamp();

		memset(_page1, 0xC0, _vid->_layerSize);

		const int shapeBg = READ_BE_UINT16(p + offset); offset += 2;
		const int count = READ_BE_UINT16(p + offset); offset += 2;

		uint16_t paletteBuffer[kMaxPaletteSize];
		memset(paletteBuffer, 0, sizeof(paletteBuffer));
		readSetPalette(p, backgroundShapes[shapeBg].offset + backgroundShapes[shapeBg].size, paletteBuffer);
		int paletteLutSize = 16;

		uint8_t paletteLut[kMaxPaletteSize];
		for (int j = 0; j < 16; ++j) {
			paletteLut[j] = 0xC0 + j;
		}

		drawSetShape(p, backgroundShapes[shapeBg].offset, 0, 0, paletteLut);
		for (int j = 0; j < count; ++j) {
			const int shapeFg = READ_BE_UINT16(p + offset); offset += 2;
			const int shapeX = (int16_t)READ_BE_UINT16(p + offset); offset += 2;
			const int shapeY = (int16_t)READ_BE_UINT16(p + offset); offset += 2;

			uint16_t tempPalette[16];
			readSetPalette(p, foregroundShapes[shapeFg].offset + foregroundShapes[shapeFg].size, tempPalette);
			for (int k = 0; k < 16; ++k) {
				bool found = false;
				for (int l = 0; l < paletteLutSize; ++l) {
					if (tempPalette[k] == paletteBuffer[l]) {
						found = true;
						paletteLut[k] = 0xC0 + l;
						break;
					}
				}
				if (!found) {
					assert(paletteLutSize < kMaxPaletteSize);
					paletteLut[k] = 0xC0 + paletteLutSize;
					paletteBuffer[paletteLutSize++] = tempPalette[k];
				}
			}

			drawSetShape(p, foregroundShapes[shapeFg].offset, shapeX, shapeY, paletteLut);
		}

		for (int j = 0; j < paletteLutSize; ++j) {
			Color c = Video::AMIGA_convertColor(paletteBuffer[j]);
			_stub->setPaletteEntry(0xC0 + j, &c);
		}

		_stub->copyRect(0, 0, _vid->_w, _vid->_h, _page1, _vid->_w);
		_stub->updateScreen(0);
		const int diff = 6 * TIMER_SLICE - (_stub->getTimeStamp() - timestamp);
		_stub->sleep((diff < 16) ? 16 : diff);
		_stub->processEvents();
		if (_stub->_pi.backspace) {
			_stub->_pi.backspace = false;
			_interrupted = true;
		}
	}
}
