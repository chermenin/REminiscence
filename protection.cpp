
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "game.h"
#include "screenshot.h"
#include "systemstub.h"

static uint8_t reverseBits(uint8_t ch) {
	static const uint8_t lut[] = {
		0x0, 0x8, 0x4, 0xC, 0x2, 0xA, 0x6, 0xE,
		0x1, 0x9, 0x5, 0xD, 0x3, 0xB, 0x7, 0xF
	};
	return (lut[ch & 15] << 4) | lut[ch >> 4];
}

static uint8_t decryptChar(uint8_t ch) {
	return reverseBits(ch ^ 0x55);
}

static uint8_t encryptChar(uint8_t ch) {
	return reverseBits(ch) ^ 0x55;
}

bool Game::handleProtectionScreenShape() {
	bool valid = false;
	_cut.prepare();
	const int palOffset = _res.isAmiga() ? 32 : 0;
	_cut.copyPalette(_protectionPal + palOffset, 0);

	_cut.updatePalette();
	_cut._gfx.setClippingRect(64, 48, 128, 128);

	_menu._charVar1 = 0xE0;
	_menu._charVar2 = 0xEF;
	_menu._charVar4 = 0xE5;
	_menu._charVar5 = 0xE2;

	// 5 codes per shape (a code is 6 characters long)
	if (0) {
		for (int shape = 0; shape < 30; ++shape) {
			fprintf(stdout, "Shape #%2d\n", shape);
			for (int code = 0; code < 5; ++code) {
				const int offset = (shape * 5 + code) * 6;
				if (_res.isAmiga()) {
					fprintf(stdout, "\t ");
					for (int i = 0; i < 6; ++i) {
						const char chr = _protectionNumberDataAmiga[(shape * 5 + code) * 6 + i] ^ 0xD7;
						fprintf(stdout, "%c", chr);
					}
					fprintf(stdout, " : ");
					for (int i = 0; i < 6; ++i) {
						fprintf(stdout, "%c", _protectionCodeDataAmiga[offset + i] ^ 0xD7);
					}
				} else {
					fprintf(stdout, "\t code %d : ", code + 1);
					for (int i = 0; i < 6; ++i) {
						fprintf(stdout, "%c", decryptChar(_protectionCodeData[offset + i]));
					}
				}
				fprintf(stdout, "\n");
			}
		}
	}
	if (0) {
		uint8_t palette[256 * 3];
		_stub->getPalette(palette, 256);
		for (int shape = 0; shape < 30; ++shape) {
			_cut.drawProtectionShape(shape, 0);
			char fname[32];
			snprintf(fname, sizeof(fname), "shape_%02d.bmp", shape);
			saveBMP(fname, _vid._tempLayer, palette, _vid._w, _vid._h);
		}
	}
	const int shapeNum = getRandomNumber() % 30;
	const int codeNum = getRandomNumber() % 5;
	for (int16_t zoom = 2000; zoom >= 0; zoom -= 100) {
		_cut.drawProtectionShape(shapeNum, zoom);
		_stub->copyRect(0, 0, _vid._w, _vid._h, _vid._tempLayer, _vid._w);
		_stub->updateScreen(0);
		_stub->sleep(30);
	}
	_vid.setTextPalette();

	char codeNumber[8];
	if (_res.isAmiga()) {
		static const uint8_t kNumberLen = 6;
		for (int i = 0; i < kNumberLen; ++i) {
			codeNumber[i] = _protectionNumberDataAmiga[(shapeNum * 5 + codeNum) * kNumberLen + i] ^ 0xD7;
		}
		codeNumber[kNumberLen] = 0;
	} else {
		snprintf(codeNumber, sizeof(codeNumber), "CODE %d", codeNum + 1);
	}

	static const int kCodeLen = 6;
	char codeText[kCodeLen + 1];
	int len = 0;
	do {
		codeText[len] = '\0';
		memcpy(_vid._frontLayer, _vid._tempLayer, _vid._layerSize);
		_vid.drawString("PROTECTION", 11 * Video::CHAR_W, 2 * Video::CHAR_H, _menu._charVar2);
		char buf[32];
		snprintf(buf, sizeof(buf), "%s :  %s", codeNumber, codeText);
		_vid.drawString(buf, 8 * Video::CHAR_W, 23 * Video::CHAR_H, _menu._charVar2);
		_vid.updateScreen();
		_stub->sleep(50);
		_stub->processEvents();
		char c = _stub->_pi.lastChar;
		if (c != 0) {
			_stub->_pi.lastChar = 0;
			if (len < kCodeLen) {
				if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
					codeText[len] = c;
					++len;
				}
			}
		}
		if (_stub->_pi.backspace) {
			_stub->_pi.backspace = false;
			if (len > 0) {
				--len;
			}
		}
		if (_stub->_pi.enter) {
			_stub->_pi.enter = false;
			if (len > 0) {
				int charsCount = 0;
				if (_res.isAmiga()) {
					const uint8_t *p = _protectionCodeDataAmiga + (shapeNum * 5 + codeNum) * kCodeLen;
					for (int i = 0; i < len && (codeText[i] ^ 0xD7) == p[i]; ++i) {
						++charsCount;
					}
				} else {
					const uint8_t *p = _protectionCodeData + (shapeNum * 5 + codeNum) * kCodeLen;
					for (int i = 0; i < len && encryptChar(codeText[i]) == p[i]; ++i) {
						++charsCount;
					}
				}
				valid = (charsCount == kCodeLen);
				break;
			}
		}
	} while (!_stub->_pi.quit);
	_vid.fadeOut();
	return valid;
}

bool Game::handleProtectionScreenWords() {
	bool valid = false;
	static const int kWordsCount = 40;
	if (0) {
		for (int i = 0; i < kWordsCount * 18; i += 18) {
			const uint8_t *data = _protectionWordData + i;
			fprintf(stdout, "page %d column %d line %2d word %d : ", data[0], data[1], data[2], data[3]);
			for (int j = 4; j < 18; ++j) {
				const uint8_t ch = decryptChar(data[j]);
				if (!(ch >= 'A' && ch <= 'Z')) {
					break;
				}
				fprintf(stdout, "%c", ch);
			}
			fprintf(stdout, "\n");
		}
	}

	_vid.setTextPalette();
	_vid.setPalette0xF();

	memset(_vid._frontLayer, 0, _vid._layerSize);

	static const char *kText[] = {
		"Enter the word found in the",
		"following location in your",
		"rulebook. (Do not count the",
		"title header that appears on",
		"all pages. Ignore captions",
		"and header).",
		0
	};
	for (int i = 0; kText[i]; ++i) {
		_vid.drawString(kText[i], 24, 16 + i * Video::CHAR_H, 0xE5);
	}
	static const int icon_spr_w = 16;
	static const int icon_spr_h = 16;
	int icon_num = 31;
	for (int y = 140; y < 140 + 5 * icon_spr_h; y += icon_spr_h) {
		for (int x = 56; x < 56 + 9 * icon_spr_w; x += icon_spr_w) {
			drawIcon(icon_num, x, y, 0xF);
			++icon_num;
		}
	}

	const uint8_t code = getRandomNumber() % kWordsCount;
	const uint8_t *protectionData = _protectionWordData + code * 18;

	static const char *kSecurityCodeText = "SECURITY CODE";
	_vid.drawString(kSecurityCodeText, 72 + (114 - strlen(kSecurityCodeText) * 8) / 2, 158, 0xE4);
	char buf[16];
	snprintf(buf, sizeof(buf), "PAGE %d", protectionData[0]);
	_vid.drawString(buf, 69, 189, 0xE5);
	snprintf(buf, sizeof(buf), "COLUMN %d", protectionData[1]);
	_vid.drawString(buf, 69, 197, 0xE5);
	snprintf(buf, sizeof(buf), "LINE %d", protectionData[2]);
	_vid.drawString(buf, (protectionData[2] < 10) ? 141 : 133, 189, 0xE5);
	snprintf(buf, sizeof(buf), "WORD %d", protectionData[3]);
	_vid.drawString(buf, 141, 197, 0xE5);

	memcpy(_vid._tempLayer, _vid._frontLayer, _vid._layerSize);

	static const int kCodeLen = 14;
	char codeText[kCodeLen + 1];
	int len = 0;
	do {
		memcpy(_vid._frontLayer, _vid._tempLayer, _vid._layerSize);
		codeText[len] = '\0';
		_vid.drawString(codeText, 72 + (114 - strlen(codeText) * 8) / 2, 166, 0xE3);
		_vid.updateScreen();
		_stub->sleep(50);
		_stub->processEvents();
		char c = _stub->_pi.lastChar;
		if (c != 0) {
			_stub->_pi.lastChar = 0;
			if (len < kCodeLen) {
				if (c >= 'A' && c <= 'Z') {
					codeText[len] = c;
					++len;
				}
			}
		}
		if (_stub->_pi.backspace) {
			_stub->_pi.backspace = false;
			if (len > 0) {
				--len;
			}
		}
		if (_stub->_pi.enter) {
			_stub->_pi.enter = false;
			if (len > 0) {
				int charsCount = 0;
				for (int i = 0; i < len; ++i) {
					if (encryptChar(codeText[i]) != protectionData[4 + i]) {
						break;
					}
					++charsCount;
				}
				// words are padded with spaces
				valid = decryptChar(protectionData[4 + charsCount]) == 0x20;
			}
		}
	} while (!_stub->_pi.quit);
	_vid.fadeOut();
	return valid;
}

