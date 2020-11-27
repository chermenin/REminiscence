
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <time.h>
#include "decode_mac.h"
#include "file.h"
#include "fs.h"
#include "game.h"
#include "seq_player.h"
#include "systemstub.h"
#include "unpack.h"
#include "util.h"

Game::Game(SystemStub *stub, FileSystem *fs, const char *savePath, int level, ResourceType ver, Language lang, WidescreenMode widescreenMode, bool autoSave)
	: _cut(&_res, stub, &_vid), _menu(&_res, stub, &_vid),
	_mix(fs, stub), _res(fs, ver, lang), _seq(stub, &_mix), _vid(&_res, stub, widescreenMode),
	_stub(stub), _fs(fs), _savePath(savePath) {
	_stateSlot = 1;
	_inp_demPos = 0;
	_skillLevel = _menu._skill = kSkillNormal;
	_currentLevel = _menu._level = level;
	_demoBin = -1;
	_widescreenMode = widescreenMode;
	_autoSave = autoSave;
	_rewindPtr = -1;
	_rewindLen = 0;
}

void Game::run() {
	_randSeed = time(0);

	_res.init();
	_res.load_TEXT();

	switch (_res._type) {
	case kResourceTypeAmiga:
		_res.load("FONT8", Resource::OT_FNT, "SPR");
		if (_res._isDemo) {
			_cut._patchedOffsetsTable = Cutscene::_amigaDemoOffsetsTable;
		}
		break;
	case kResourceTypeDOS:
		_res.load("FB_TXT", Resource::OT_FNT);
		if (g_options.use_seq_cutscenes) {
			_res._hasSeqData = _fs->exists("INTRO.SEQ");
		}
		if (_fs->exists("logosssi.cmd")) {
			_cut._patchedOffsetsTable = Cutscene::_ssiOffsetsTable;
		}
		break;
	case kResourceTypeMac:
		_res.MAC_loadClutData();
		_res.MAC_loadFontData();
		break;
	}

	if (!g_options.bypass_protection && !g_options.use_words_protection && !_res.isMac()) {
		while (!handleProtectionScreenShape()) {
			if (_stub->_pi.quit) {
				return;
			}
		}
	}

	_mix.init();
	_mix._mod._isAmiga = _res.isAmiga();

	if (_res.isMac()) {
		displayTitleScreenMac(Menu::kMacTitleScreen_MacPlay);
		if (!_stub->_pi.quit) {
			displayTitleScreenMac(Menu::kMacTitleScreen_Presage);
		}
	}
	playCutscene(0x40);
	playCutscene(0x0D);

	switch (_res._type) {
	case kResourceTypeAmiga:
		_res.load("ICONE", Resource::OT_ICN, "SPR");
		_res.load("ICON", Resource::OT_ICN, "SPR");
		_res.load("PERSO", Resource::OT_SPM);
		break;
	case kResourceTypeDOS:
		_res.load("GLOBAL", Resource::OT_ICN);
		_res.load("GLOBAL", Resource::OT_SPC);
		_res.load("PERSO", Resource::OT_SPR);
		_res.load_SPR_OFF("PERSO", _res._spr1);
		_res.load_FIB("GLOBAL");
		break;
	case kResourceTypeMac:
		_res.MAC_loadIconData();
		_res.MAC_loadPersoData();
		_res.MAC_loadSounds();
		break;
	}

	if (!g_options.bypass_protection && g_options.use_words_protection && _res.isDOS()) {
		while (!handleProtectionScreenWords()) {
			if (_stub->_pi.quit) {
				return;
			}
		}
	}

	bool presentMenu = ((_res._type != kResourceTypeDOS) || _res.fileExists("MENU1.MAP"));
	while (!_stub->_pi.quit) {
		if (presentMenu) {
			_mix.playMusic(1);
			switch (_res._type) {
			case kResourceTypeDOS:
				_menu.handleTitleScreen();
				if (_menu._selectedOption == Menu::MENU_OPTION_ITEM_QUIT || _stub->_pi.quit) {
					_stub->_pi.quit = true;
					break;
				}
				if (_menu._selectedOption == Menu::MENU_OPTION_ITEM_DEMO) {
					_demoBin = (_demoBin + 1) % ARRAYSIZE(_demoInputs);
					const char *fn = _demoInputs[_demoBin].name;
					debug(DBG_DEMO, "Loading inputs from '%s'", fn);
					_res.load_DEM(fn);
					if (_res._demLen == 0) {
						continue;
					}
					_skillLevel = kSkillNormal;
					_currentLevel = _demoInputs[_demoBin].level;
					_randSeed = 0;
					_mix.stopMusic();
					break;
				}
				_demoBin = -1;
				_skillLevel = _menu._skill;
				_currentLevel = _menu._level;
				_mix.stopMusic();
				break;
			case kResourceTypeAmiga:
				displayTitleScreenAmiga();
				_stub->setScreenSize(Video::GAMESCREEN_W, Video::GAMESCREEN_H);
				break;
			case kResourceTypeMac:
				displayTitleScreenMac(Menu::kMacTitleScreen_Flashback);
				break;
			}
		}
		if (_stub->_pi.quit) {
			break;
		}
		if (_stub->hasWidescreen()) {
			_stub->clearWidescreen();
		}
		if (_currentLevel == 7) {
			_vid.fadeOut();
			_vid.setTextPalette();
			playCutscene(0x3D);
		} else {
			_vid.setTextPalette();
			_vid.setPalette0xF();
			_stub->setOverscanColor(0xE0);
			_vid._unkPalSlot1 = 0;
			_vid._unkPalSlot2 = 0;
			_score = 0;
			clearStateRewind();
			loadLevelData();
			resetGameState();
			_endLoop = false;
			_frameTimestamp = _stub->getTimeStamp();
			_saveTimestamp = _frameTimestamp;
			while (!_stub->_pi.quit && !_endLoop) {
				mainLoop();
				if (_demoBin != -1 && _inp_demPos >= _res._demLen) {
					debug(DBG_DEMO, "End of demo");
					// exit level
					_demoBin = -1;
					_endLoop = true;
				}
			}
			// flush inputs
			_stub->_pi.dirMask = 0;
			_stub->_pi.enter = false;
			_stub->_pi.space = false;
			_stub->_pi.shift = false;
		}
	}

	_res.free_TEXT();
	_mix.free();
	_res.fini();
}

void Game::displayTitleScreenAmiga() {
	static const char *FILENAME = "present.cmp";
	_res.load_CMP_menu(FILENAME);
	static const int kW = 320;
	static const int kH = 224;
	uint8_t *buf = (uint8_t *)calloc(1, kW * kH);
	if (!buf) {
		error("Failed to allocate screen buffer w=%d h=%d", kW, kH);
	}
	static const uint16_t kAmigaColors[] = {
		0x000, 0x123, 0x012, 0x134, 0x433, 0x453, 0x046, 0x245,
		0x751, 0x455, 0x665, 0x268, 0x961, 0x478, 0x677, 0x786,
		0x17B, 0x788, 0xB84, 0xC92, 0x49C, 0xF00, 0x9A8, 0x9AA,
		0xCA7, 0xEA3, 0x8BD, 0xBBB, 0xEC7, 0xBCD, 0xDDB, 0xEED
	};
	for (int i = 0; i < 32; ++i) {
		Color c = Video::AMIGA_convertColor(kAmigaColors[i]);
		_stub->setPaletteEntry(i, &c);
	}
	_vid.setTextPalette();
	_stub->setScreenSize(kW, kH);
	// fill with black
	_stub->copyRect(0, 0, kW, kH, buf, kW);
	_stub->updateScreen(0);
	_vid.AMIGA_decodeCmp(_res._scratchBuffer + 6, buf);
	int h = 0;
	while (1) {
		if (h <= kH / 2) {
			const int y = kH / 2 - h;
			_stub->copyRect(0, y, kW, h * 2, buf, kW);
			_stub->updateScreen(0);
			h += 2;
		} else {
			static const uint8_t selectedColor = 0xE4;
			static const uint8_t defaultColor = 0xE8;
			for (int i = 0; i < 7; ++i) {
				const char *str = Menu::_levelNames[i];
				const uint8_t color = (_currentLevel == i) ? selectedColor : defaultColor;
				const int x = 24;
				const int y = 24 + i * 16;
				for (int j = 0; str[j]; ++j) {
					_vid.AMIGA_drawStringChar(buf, kW, x + j * Video::CHAR_W, y, _res._fnt, color, str[j]);
				}
			}
			if (_stub->_pi.dirMask & PlayerInput::DIR_UP) {
				_stub->_pi.dirMask &= ~PlayerInput::DIR_UP;
				if (_currentLevel > 0) {
					--_currentLevel;
				}
			}
			if (_stub->_pi.dirMask & PlayerInput::DIR_DOWN) {
				_stub->_pi.dirMask &= ~PlayerInput::DIR_DOWN;
				if (_currentLevel < 6) {
					++_currentLevel;
				}
			}
			_stub->copyRect(0, 0, kW, kH, buf, kW);
			_stub->updateScreen(0);
		}
		_stub->processEvents();
		if (_stub->_pi.quit) {
			break;
		}
		if (_stub->_pi.enter) {
			_stub->_pi.enter = false;
			break;
		}
		_stub->sleep(30);
	}
	free(buf);
}

void Game::displayTitleScreenMac(int num) {
	const int w = 512;
	int h = 384;
	int clutBaseColor = 0;
	switch (num) {
	case Menu::kMacTitleScreen_MacPlay:
		break;
	case Menu::kMacTitleScreen_Presage:
		clutBaseColor = 12;
		break;
	case Menu::kMacTitleScreen_Flashback:
	case Menu::kMacTitleScreen_LeftEye:
	case Menu::kMacTitleScreen_RightEye:
		h = 448;
		break;
	case Menu::kMacTitleScreen_Controls:
		break;
	}
	DecodeBuffer buf;
	memset(&buf, 0, sizeof(buf));
	buf.ptr = _vid._frontLayer;
	buf.pitch = buf.w = _vid._w;
	buf.h = _vid._h;
	buf.x = (_vid._w - w) / 2;
	buf.y = (_vid._h - h) / 2;
	buf.setPixel = Video::MAC_setPixel;
	memset(_vid._frontLayer, 0, _vid._layerSize);
	_res.MAC_loadTitleImage(num, &buf);
	for (int i = 0; i < 12; ++i) {
		Color palette[16];
		_res.MAC_copyClut16(palette, 0, clutBaseColor + i);
		const int basePaletteColor = i * 16;
		for (int j = 0; j < 16; ++j) {
			_stub->setPaletteEntry(basePaletteColor + j, &palette[j]);
		}
	}
	if (num == Menu::kMacTitleScreen_MacPlay) {
		Color palette[16];
		_res.MAC_copyClut16(palette, 0, 56);
		for (int i = 12; i < 16; ++i) {
			const int basePaletteColor = i * 16;
			for (int j = 0; j < 16; ++j) {
				_stub->setPaletteEntry(basePaletteColor + j, &palette[j]);
			}
		}
	} else if (num == Menu::kMacTitleScreen_Presage) {
		Color c;
		c.r = c.g = c.b = 0;
		_stub->setPaletteEntry(0, &c);
	} else if (num == Menu::kMacTitleScreen_Flashback) {
		_vid.setTextPalette();
		_vid._charShadowColor = 0xE0;
	}
	_stub->copyRect(0, 0, _vid._w, _vid._h, _vid._frontLayer, _vid._w);
	_stub->updateScreen(0);
	while (1) {
		if (num == Menu::kMacTitleScreen_Flashback) {
			static const uint8_t selectedColor = 0xE4;
			static const uint8_t defaultColor = 0xE8;
			for (int i = 0; i < 7; ++i) {
				const char *str = Menu::_levelNames[i];
				_vid.drawString(str, 24, 24 + i * 16, (_currentLevel == i) ? selectedColor : defaultColor);
			}
			if (_stub->_pi.dirMask & PlayerInput::DIR_UP) {
				_stub->_pi.dirMask &= ~PlayerInput::DIR_UP;
				if (_currentLevel > 0) {
					--_currentLevel;
				}
			}
			if (_stub->_pi.dirMask & PlayerInput::DIR_DOWN) {
				_stub->_pi.dirMask &= ~PlayerInput::DIR_DOWN;
				if (_currentLevel < 6) {
					++_currentLevel;
				}
			}
			_vid.updateScreen();
		}
		_stub->processEvents();
		if (_stub->_pi.quit) {
			break;
		}
		if (_stub->_pi.enter) {
			_stub->_pi.enter = false;
			break;
		}
		_stub->sleep(30);
	}
}

void Game::resetGameState() {
	_animBuffers._states[0] = _animBuffer0State;
	_animBuffers._curPos[0] = 0xFF;
	_animBuffers._states[1] = _animBuffer1State;
	_animBuffers._curPos[1] = 0xFF;
	_animBuffers._states[2] = _animBuffer2State;
	_animBuffers._curPos[2] = 0xFF;
	_animBuffers._states[3] = _animBuffer3State;
	_animBuffers._curPos[3] = 0xFF;
	_currentRoom = _res._pgeInit[0].init_room;
	_cut._deathCutsceneId = 0xFFFF;
	_pge_opTempVar2 = 0xFFFF;
	_deathCutsceneCounter = 0;
	_saveStateCompleted = false;
	_loadMap = true;
	pge_resetGroups();
	_blinkingConradCounter = 0;
	_pge_processOBJ = false;
	_pge_opTempVar1 = 0;
	_textToDisplay = 0xFFFF;
}

void Game::mainLoop() {
	playCutscene();
	if (_cut._id == 0x3D) {
		showFinalScore();
		_endLoop = true;
		return;
	}
	if (_deathCutsceneCounter) {
		--_deathCutsceneCounter;
		if (_deathCutsceneCounter == 0) {
			playCutscene(_cut._deathCutsceneId);
			if (!handleContinueAbort()) {
				playCutscene(0x41);
				_endLoop = true;
			} else {
				if (_autoSave && _rewindLen != 0 && loadGameState(kAutoSaveSlot)) {
					// autosave
				} else if (_validSaveState && loadGameState(kIngameSaveSlot)) {
					// ingame save
				} else {
					clearStateRewind();
					loadLevelData();
					resetGameState();
				}
			}
			return;
		}
	}
	memcpy(_vid._frontLayer, _vid._backLayer, _vid._layerSize);
	pge_getInput();
	pge_prepare();
	col_prepareRoomState();
	uint8_t oldLevel = _currentLevel;
	for (uint16_t i = 0; i < _res._pgeNum; ++i) {
		LivePGE *pge = _pge_liveTable2[i];
		if (pge) {
			_col_currentPiegeGridPosY = (pge->pos_y / 36) & ~1;
			_col_currentPiegeGridPosX = (pge->pos_x + 8) >> 4;
			pge_process(pge);
		}
	}
	if (oldLevel != _currentLevel) {
		if (_res._isDemo) {
			_currentLevel = oldLevel;
		}
		changeLevel();
		_pge_opTempVar1 = 0;
		return;
	}
	if (_loadMap) {
		if (_currentRoom == 0xFF || !hasLevelMap(_currentLevel, _pgeLive[0].room_location)) {
			_cut._id = 6;
			_deathCutsceneCounter = 1;
		} else {
			_currentRoom = _pgeLive[0].room_location;
			loadLevelMap();
			_loadMap = false;
			_vid.fullRefresh();
		}
	}
	prepareAnims();
	drawAnims();
	drawCurrentInventoryItem();
	drawLevelTexts();
	if (g_options.enable_password_menu) {
		printLevelCode();
	}
	if (_blinkingConradCounter != 0) {
		--_blinkingConradCounter;
	}
	_vid.updateScreen();
	updateTiming();
	drawStoryTexts();
	if (_stub->_pi.backspace) {
		_stub->_pi.backspace = false;
		handleInventory();
	}
	if (_stub->_pi.escape) {
		_stub->_pi.escape = false;
		if (_demoBin != -1 || handleConfigPanel()) {
			_endLoop = true;
			return;
		}
	}
	inp_handleSpecialKeys();
	if (_autoSave && _stub->getTimeStamp() - _saveTimestamp >= kAutoSaveIntervalMs) {
		// do not save if we died or about to
		if (_pgeLive[0].life > 0 && _deathCutsceneCounter == 0) {
			saveGameState(kAutoSaveSlot);
			_saveTimestamp = _stub->getTimeStamp();
		}
	}
}

void Game::updateTiming() {
	static const int frameHz = 30;
	int32_t delay = _stub->getTimeStamp() - _frameTimestamp;
	int32_t pause = (_stub->_pi.dbgMask & PlayerInput::DF_FASTMODE) ? 20 : (1000 / frameHz);
	pause -= delay;
	if (pause > 0) {
		_stub->sleep(pause);
	}
	_frameTimestamp = _stub->getTimeStamp();
}

void Game::playCutscene(int id) {
	if (id != -1) {
		_cut._id = id;
	}
	if (_cut._id != 0xFFFF) {
		if (_stub->hasWidescreen()) {
			_stub->enableWidescreen(false);
		}
		_mix.stopMusic();
		if (_res._hasSeqData) {
			int num = 0;
			switch (_cut._id) {
			case 0x02: {
					static const uint8_t tab[] = { 1, 2, 1, 3, 3, 4, 4 };
					num = tab[_currentLevel];
				}
				break;
			case 0x05: {
					static const uint8_t tab[] = { 1, 2, 3, 5, 5, 4, 4 };
					num = tab[_currentLevel];
				}
				break;
			case 0x0A: {
					static const uint8_t tab[] = { 1, 2, 2, 2, 2, 2, 2 };
					num = tab[_currentLevel];
				}
				break;
			case 0x10: {
					static const uint8_t tab[] = { 1, 1, 1, 2, 2, 3, 3 };
					num = tab[_currentLevel];
				}
				break;
			case 0x3C: {
					static const uint8_t tab[] = { 1, 1, 1, 1, 1, 2, 2 };
					num = tab[_currentLevel];
				}
				break;
			case 0x40:
				return;
			case 0x4A:
				return;
			}
			if (SeqPlayer::_namesTable[_cut._id]) {
			        char name[16];
			        snprintf(name, sizeof(name), "%s.SEQ", SeqPlayer::_namesTable[_cut._id]);
				char *p = strchr(name, '0');
				if (p) {
					*p += num;
				}
			        if (playCutsceneSeq(name)) {
					if (_cut._id == 0x3D) {
						playCutsceneSeq("CREDITS.SEQ");
						_cut._interrupted = false;
					} else {
						_cut._id = 0xFFFF;
					}
					return;
				}
			}
		}
		if (_cut._id != 0x4A) {
			_mix.playMusic(Cutscene::_musicTable[_cut._id]);
		}
		_cut.play();
		if (id == 0xD && !_cut._interrupted) {
			const bool extendedIntroduction = (_res._type == kResourceTypeDOS || _res._type == kResourceTypeMac);
			if (extendedIntroduction) {
				_cut._id = 0x4A;
				_cut.play();
			}
		}
		if (_res._type == kResourceTypeMac && !(id == 0x48 || id == 0x49)) { // continue or score screens
			// restore palette entries modified by the cutscene player (0xC and 0xD)
			Color palette[32];
			_res.MAC_copyClut16(palette, 0, 0x37);
			_res.MAC_copyClut16(palette, 1, 0x38);
			for (int i = 0; i < 32; ++i) {
				_stub->setPaletteEntry(0xC0 + i, &palette[i]);
			}
		}
		if (id == 0x3D) {
			_cut.playCredits();
		}
		_mix.stopMusic();
		if (_stub->hasWidescreen()) {
			_stub->enableWidescreen(true);
		}
	}
}

bool Game::playCutsceneSeq(const char *name) {
	File f;
	if (f.open(name, "rb", _fs)) {
		_seq.setBackBuffer(_res._scratchBuffer);
		_seq.play(&f);
		_vid.fullRefresh();
		return true;
	}
	return false;
}

void Game::inp_handleSpecialKeys() {
	if (_stub->_pi.dbgMask & PlayerInput::DF_SETLIFE) {
		_pgeLive[0].life = 0x7FFF;
	}
	if (_stub->_pi.load) {
		loadGameState(_stateSlot);
		_stub->_pi.load = false;
	}
	if (_stub->_pi.save) {
		saveGameState(_stateSlot);
		_stub->_pi.save = false;
	}
	if (_stub->_pi.stateSlot != 0) {
		int8_t slot = _stateSlot + _stub->_pi.stateSlot;
		if (slot >= 1 && slot < 100) {
			_stateSlot = slot;
			debug(DBG_INFO, "Current game state slot is %d", _stateSlot);
		}
		_stub->_pi.stateSlot = 0;
	}
	if (_stub->_pi.rewind) {
		if (_rewindLen != 0) {
			loadStateRewind();
		} else {
			debug(DBG_INFO, "Rewind buffer is empty");
		}
		_stub->_pi.rewind = false;
	}
}

void Game::drawCurrentInventoryItem() {
	uint16_t src = _pgeLive[0].current_inventory_PGE;
	if (src != 0xFF) {
		_currentIcon = _res._pgeInit[src].icon_num;
		drawIcon(_currentIcon, 232, 8, 0xA);
	}
}

void Game::showFinalScore() {
	if (_stub->hasWidescreen()) {
		_stub->clearWidescreen();
	}
	playCutscene(0x49);
	char buf[50];
	snprintf(buf, sizeof(buf), "SCORE %08u", _score);
	_vid.drawString(buf, (Video::GAMESCREEN_W - strlen(buf) * Video::CHAR_W) / 2, 40, 0xE5);
	const char *str = _menu.getLevelPassword(7, _skillLevel);
	_vid.drawString(str, (Video::GAMESCREEN_W - strlen(str) * Video::CHAR_W) / 2, 16, 0xE7);
	while (!_stub->_pi.quit) {
		_stub->copyRect(0, 0, _vid._w, _vid._h, _vid._frontLayer, _vid._w);
		_stub->updateScreen(0);
		_stub->processEvents();
		if (_stub->_pi.enter) {
			_stub->_pi.enter = false;
			break;
		}
		_stub->sleep(100);
	}
}

bool Game::handleConfigPanel() {
	const int x = 7;
	const int y = 10;
	const int w = 17;
	const int h = 12;

	_vid._charShadowColor = 0xE2;
	_vid._charFrontColor = 0xEE;
	_vid._charTransparentColor = 0xFF;

	// the panel background is drawn using special characters from FB_TXT.FNT
	static const bool kUseDefaultFont = true;

	switch (_res._type) {
	case kResourceTypeAmiga:
		for (int i = 0; i < h; ++i) {
			for (int j = 0; j < w; ++j) {
				_vid.fillRect(Video::CHAR_W * (x + j), Video::CHAR_H * (y + i), Video::CHAR_W, Video::CHAR_H, 0xE2);
			}
		}
		break;
	case kResourceTypeDOS:
		// top-left rounded corner
		_vid.PC_drawChar(0x81, y, x, kUseDefaultFont);
		// top-right rounded corner
		_vid.PC_drawChar(0x82, y, x + w, kUseDefaultFont);
		// bottom-left rounded corner
		_vid.PC_drawChar(0x83, y + h, x, kUseDefaultFont);
		// bottom-right rounded corner
		_vid.PC_drawChar(0x84, y + h, x + w, kUseDefaultFont);
		// horizontal lines
		for (int i = 1; i < w; ++i) {
			_vid.PC_drawChar(0x85, y, x + i, kUseDefaultFont);
			_vid.PC_drawChar(0x88, y + h, x + i, kUseDefaultFont);
		}
		for (int j = 1; j < h; ++j) {
			_vid._charTransparentColor = 0xFF;
			// left vertical line
			_vid.PC_drawChar(0x86, y + j, x, kUseDefaultFont);
			// right vertical line
			_vid.PC_drawChar(0x87, y + j, x + w, kUseDefaultFont);
			_vid._charTransparentColor = 0xE2;
			for (int i = 1; i < w; ++i) {
				_vid.PC_drawChar(0x20, y + j, x + i, kUseDefaultFont);
			}
		}
		break;
	case kResourceTypeMac:
		// top-left rounded corner
		_vid.MAC_drawStringChar(_vid._frontLayer, _vid._w, Video::CHAR_W * x,       Video::CHAR_H * y,       _res._fnt, _vid._charFrontColor, 0x81);
		// top-right rounded corner
		_vid.MAC_drawStringChar(_vid._frontLayer, _vid._w, Video::CHAR_W * (x + w), Video::CHAR_H * y,       _res._fnt, _vid._charFrontColor, 0x82);
		// bottom-left rounded corner
		_vid.MAC_drawStringChar(_vid._frontLayer, _vid._w, Video::CHAR_W * x,       Video::CHAR_H * (y + h), _res._fnt, _vid._charFrontColor, 0x83);
		// bottom-right rounded corner
		_vid.MAC_drawStringChar(_vid._frontLayer, _vid._w, Video::CHAR_W * (x + w), Video::CHAR_H * (y + h), _res._fnt, _vid._charFrontColor, 0x84);
		// horizontal lines
		for (int i = 1; i < w; ++i) {
			_vid.MAC_drawStringChar(_vid._frontLayer, _vid._w, Video::CHAR_W * (x + i), Video::CHAR_H * y,       _res._fnt, _vid._charFrontColor, 0x85);
			_vid.MAC_drawStringChar(_vid._frontLayer, _vid._w, Video::CHAR_W * (x + i), Video::CHAR_H * (y + h), _res._fnt, _vid._charFrontColor, 0x88);
		}
		// vertical lines
		for (int i = 1; i < h; ++i) {
			_vid.MAC_drawStringChar(_vid._frontLayer, _vid._w, Video::CHAR_W * x,       Video::CHAR_H * (y + i), _res._fnt, _vid._charFrontColor, 0x86);
			_vid.MAC_drawStringChar(_vid._frontLayer, _vid._w, Video::CHAR_W * (x + w), Video::CHAR_H * (y + i), _res._fnt, _vid._charFrontColor, 0x87);
			for (int j = 1; j < w; ++j) {
				_vid.fillRect(Video::CHAR_W * (x + j), Video::CHAR_H * (y + i), Video::CHAR_W, Video::CHAR_H, 0xE2);
			}
		}
		break;
	}

	_menu._charVar3 = 0xE4;
	_menu._charVar4 = 0xE5;
	_menu._charVar1 = 0xE2;
	_menu._charVar2 = 0xEE;

	_vid.fullRefresh();
	enum { MENU_ITEM_ABORT = 1, MENU_ITEM_LOAD = 2, MENU_ITEM_SAVE = 3 };
	uint8_t colors[] = { 2, 3, 3, 3 };
	int current = 0;
	while (!_stub->_pi.quit) {
		_menu.drawString(_res.getMenuString(LocaleData::LI_18_RESUME_GAME), y + 2, 9, colors[0]);
		_menu.drawString(_res.getMenuString(LocaleData::LI_19_ABORT_GAME), y + 4, 9, colors[1]);
		_menu.drawString(_res.getMenuString(LocaleData::LI_20_LOAD_GAME), y + 6, 9, colors[2]);
		_menu.drawString(_res.getMenuString(LocaleData::LI_21_SAVE_GAME), y + 8, 9, colors[3]);
		_vid.fillRect(Video::CHAR_W * (x + 1), Video::CHAR_H * (y + 10), Video::CHAR_W * (w - 2), Video::CHAR_H, 0xE2);
		char buf[32];
		snprintf(buf, sizeof(buf), "%s < %02d >", _res.getMenuString(LocaleData::LI_22_SAVE_SLOT), _stateSlot);
		_menu.drawString(buf, y + 10, 9, 1);

		_vid.updateScreen();
		_stub->sleep(80);
		inp_update();

		int prev = current;
		if (_stub->_pi.dirMask & PlayerInput::DIR_UP) {
			_stub->_pi.dirMask &= ~PlayerInput::DIR_UP;
			current = (current + 3) % 4;
		}
		if (_stub->_pi.dirMask & PlayerInput::DIR_DOWN) {
			_stub->_pi.dirMask &= ~PlayerInput::DIR_DOWN;
			current = (current + 1) % 4;
		}
		if (_stub->_pi.dirMask & PlayerInput::DIR_LEFT) {
			_stub->_pi.dirMask &= ~PlayerInput::DIR_LEFT;
			--_stateSlot;
			if (_stateSlot < 1) {
				_stateSlot = 1;
			}
		}
		if (_stub->_pi.dirMask & PlayerInput::DIR_RIGHT) {
			_stub->_pi.dirMask &= ~PlayerInput::DIR_RIGHT;
			++_stateSlot;
			if (_stateSlot > 99) {
				_stateSlot = 99;
			}
		}
		if (prev != current) {
			SWAP(colors[prev], colors[current]);
		}
		if (_stub->_pi.enter) {
			_stub->_pi.enter = false;
			switch (current) {
			case MENU_ITEM_LOAD:
				_stub->_pi.load = true;
				break;
			case MENU_ITEM_SAVE:
				_stub->_pi.save = true;
				break;
			}
			break;
		}
		if (_stub->_pi.escape) {
			_stub->_pi.escape = false;
			break;
		}
	}
	_vid.fullRefresh();
	return (current == MENU_ITEM_ABORT);
}

bool Game::handleContinueAbort() {
	if (_stub->hasWidescreen()) {
		_stub->clearWidescreen();
	}
	playCutscene(0x48);
	int timeout = 100;
	int current_color = 0;
	uint8_t colors[] = { 0xE4, 0xE5 };
	uint8_t color_inc = 0xFF;
	Color col;
	_stub->getPaletteEntry(0xE4, &col);
	memcpy(_vid._tempLayer, _vid._frontLayer, _vid._layerSize);
	while (timeout >= 0 && !_stub->_pi.quit) {
		const char *str;
		str = _res.getMenuString(LocaleData::LI_01_CONTINUE_OR_ABORT);
		_vid.drawString(str, (Video::GAMESCREEN_W - strlen(str) * Video::CHAR_W) / 2, 64, 0xE3);
		str = _res.getMenuString(LocaleData::LI_02_TIME);
		char buf[50];
		snprintf(buf, sizeof(buf), "%s : %d", str, timeout / 10);
		_vid.drawString(buf, 96, 88, 0xE3);
		str = _res.getMenuString(LocaleData::LI_03_CONTINUE);
		_vid.drawString(str, (Video::GAMESCREEN_W - strlen(str) * Video::CHAR_W) / 2, 104, colors[0]);
		str = _res.getMenuString(LocaleData::LI_04_ABORT);
		_vid.drawString(str, (Video::GAMESCREEN_W - strlen(str) * Video::CHAR_W) / 2, 112, colors[1]);
		snprintf(buf, sizeof(buf), "SCORE  %08u", _score);
		_vid.drawString(buf, 64, 154, 0xE3);
		if (_stub->_pi.dirMask & PlayerInput::DIR_UP) {
			_stub->_pi.dirMask &= ~PlayerInput::DIR_UP;
			if (current_color > 0) {
				SWAP(colors[current_color], colors[current_color - 1]);
				--current_color;
			}
		}
		if (_stub->_pi.dirMask & PlayerInput::DIR_DOWN) {
			_stub->_pi.dirMask &= ~PlayerInput::DIR_DOWN;
			if (current_color < 1) {
				SWAP(colors[current_color], colors[current_color + 1]);
				++current_color;
			}
		}
		if (_stub->_pi.enter) {
			_stub->_pi.enter = false;
			return (current_color == 0);
		}
		_stub->copyRect(0, 0, _vid._w, _vid._h, _vid._frontLayer, _vid._w);
		_stub->updateScreen(0);
		static const int COLOR_STEP = 8;
		static const int COLOR_MIN = 16;
		static const int COLOR_MAX = 256 - 16;
		if (col.b >= COLOR_MAX) {
			color_inc = 0;
		} else if (col.b < COLOR_MIN) {
			color_inc = 0xFF;
		}
		if (color_inc == 0xFF) {
			col.b += COLOR_STEP;
			col.g += COLOR_STEP;
		} else {
			col.b -= COLOR_STEP;
			col.g -= COLOR_STEP;
		}
		_stub->setPaletteEntry(0xE4, &col);
		_stub->processEvents();
		_stub->sleep(100);
		--timeout;
		memcpy(_vid._frontLayer, _vid._tempLayer, _vid._layerSize);
	}
	return false;
}

void Game::printLevelCode() {
	if (_printLevelCodeCounter != 0) {
		--_printLevelCodeCounter;
		if (_printLevelCodeCounter != 0) {
			char buf[32];
			snprintf(buf, sizeof(buf), "CODE: %s", _menu.getLevelPassword(_currentLevel, _skillLevel));
			_vid.drawString(buf, (Video::GAMESCREEN_W - strlen(buf) * Video::CHAR_W) / 2, 16, 0xE7);
		}
	}
}

void Game::printSaveStateCompleted() {
	if (_saveStateCompleted) {
		const char *str = _res.getMenuString(LocaleData::LI_05_COMPLETED);
		_vid.drawString(str, (176 - strlen(str) * Video::CHAR_W) / 2, 34, 0xE6);
	}
}

void Game::drawLevelTexts() {
	LivePGE *pge = &_pgeLive[0];
	int8_t obj = col_findCurrentCollidingObject(pge, 3, 0xFF, 0xFF, &pge);
	if (obj == 0) {
		obj = col_findCurrentCollidingObject(pge, 0xFF, 5, 9, &pge);
	}
	if (obj > 0) {
		_printLevelCodeCounter = 0;
		if (_textToDisplay == 0xFFFF) {
			uint8_t icon_num = obj - 1;
			drawIcon(icon_num, 80, 8, 0xA);
			uint8_t txt_num = pge->init_PGE->text_num;
			const uint8_t *str = _res.getTextString(_currentLevel, txt_num);
			drawString(str, 176, 26, 0xE6, true);
			if (icon_num == 2) {
				printSaveStateCompleted();
				return;
			}
		} else {
			_currentInventoryIconNum = obj - 1;
		}
	}
	_saveStateCompleted = false;
}

static int getLineLength(const uint8_t *str) {
	int len = 0;
	while (*str && *str != 0xB && *str != 0xA) {
		++str;
		++len;
	}
	return len;
}

void Game::drawStoryTexts() {
	if (_textToDisplay != 0xFFFF) {
		uint8_t textColor = 0xE8;
		const uint8_t *str = _res.getGameString(_textToDisplay);
		memcpy(_vid._tempLayer, _vid._frontLayer, _vid._layerSize);
		int textSpeechSegment = 0;
		int textSegmentsCount = 0;
		while (!_stub->_pi.quit) {
			drawIcon(_currentInventoryIconNum, 80, 8, 0xA);
			int yPos = 26;
			if (_res._type == kResourceTypeMac) {
				if (textSegmentsCount == 0) {
					textSegmentsCount = *str++;
				}
				int len = *str++;
				if (*str == '@') {
					switch (str[1]) {
					case '1':
						textColor = 0xE9;
						break;
					case '2':
						textColor = 0xEB;
						break;
					default:
						warning("Unhandled MAC text color code 0x%x", str[1]);
						break;
					}
					str += 2;
					len -= 2;
				}
				for (; len > 0; yPos += 8) {
					const uint8_t *next = (const uint8_t *)memchr(str, 0x7C, len);
					if (!next) {
						_vid.drawStringLen((const char *)str, len, (176 - len * Video::CHAR_W) / 2, yPos, textColor);
						// point 'str' to beginning of next text segment
						str += len;
						break;
					}
					const int lineLength = next - str;
					_vid.drawStringLen((const char *)str, lineLength, (176 - lineLength * Video::CHAR_W) / 2, yPos, textColor);
					str = next + 1;
					len -= lineLength + 1;
				}
			} else {
				if (*str == 0xFF) {
					if (_res._lang == LANG_JP) {
						switch (str[1]) {
						case 0:
							textColor = 0xE9;
							break;
						case 1:
							textColor = 0xEB;
							break;
						default:
							warning("Unhandled JP text color code 0x%x", str[1]);
							break;
						}
						str += 2;
					} else {
						textColor = str[1];
						// str[2] is an unused color (possibly the shadow)
						str += 3;
					}
				}
				while (1) {
					const int len = getLineLength(str);
					str = (const uint8_t *)_vid.drawString((const char *)str, (176 - len * Video::CHAR_W) / 2, yPos, textColor);
					if (*str == 0 || *str == 0xB) {
						break;
					}
					++str;
					yPos += 8;
				}
			}
			uint8_t *voiceSegmentData = 0;
			uint32_t voiceSegmentLen = 0;
			_res.load_VCE(_textToDisplay, textSpeechSegment++, &voiceSegmentData, &voiceSegmentLen);
			if (voiceSegmentData) {
				_mix.play(voiceSegmentData, voiceSegmentLen, 32000, Mixer::MAX_VOLUME);
			}
			_vid.updateScreen();
			while (!_stub->_pi.backspace && !_stub->_pi.quit) {
				if (voiceSegmentData && !_mix.isPlaying(voiceSegmentData)) {
					break;
				}
				inp_update();
				_stub->sleep(80);
			}
			if (voiceSegmentData) {
				_mix.stopAll();
				free(voiceSegmentData);
			}
			_stub->_pi.backspace = false;
			if (_res._type == kResourceTypeMac) {
				if (textSpeechSegment == textSegmentsCount) {
					break;
				}
			} else {
				if (*str == 0) {
					break;
				}
				++str;
			}
			memcpy(_vid._frontLayer, _vid._tempLayer, _vid._layerSize);
		}
		_textToDisplay = 0xFFFF;
	}
}

void Game::drawString(const uint8_t *p, int x, int y, uint8_t color, bool hcenter) {
	const char *str = (const char *)p;
	int len = 0;
	if (_res._type == kResourceTypeMac) {
		len = *p;
		++str;
	} else {
		len = strlen(str);
	}
	if (hcenter) {
		x = (x - len * Video::CHAR_W) / 2;
	}
	_vid.drawStringLen(str, len, x, y, color);
}

void Game::prepareAnims() {
	if (!(_currentRoom & 0x80) && _currentRoom < 0x40) {
		int8_t pge_room;
		LivePGE *pge = _pge_liveTable1[_currentRoom];
		while (pge) {
			prepareAnimsHelper(pge, 0, 0);
			pge = pge->next_PGE_in_room;
		}
		pge_room = _res._ctData[CT_UP_ROOM + _currentRoom];
		if (pge_room >= 0 && pge_room < 0x40) {
			pge = _pge_liveTable1[pge_room];
			while (pge) {
				if ((pge->init_PGE->object_type != 10 && pge->pos_y > 176) || (pge->init_PGE->object_type == 10 && pge->pos_y > 216)) {
					prepareAnimsHelper(pge, 0, -216);
				}
				pge = pge->next_PGE_in_room;
			}
		}
		pge_room = _res._ctData[CT_DOWN_ROOM + _currentRoom];
		if (pge_room >= 0 && pge_room < 0x40) {
			pge = _pge_liveTable1[pge_room];
			while (pge) {
				if (pge->pos_y < 48) {
					prepareAnimsHelper(pge, 0, 216);
				}
				pge = pge->next_PGE_in_room;
			}
		}
		pge_room = _res._ctData[CT_LEFT_ROOM + _currentRoom];
		if (pge_room >= 0 && pge_room < 0x40) {
			pge = _pge_liveTable1[pge_room];
			while (pge) {
				if (pge->pos_x > 224) {
					prepareAnimsHelper(pge, -256, 0);
				}
				pge = pge->next_PGE_in_room;
			}
		}
		pge_room = _res._ctData[CT_RIGHT_ROOM + _currentRoom];
		if (pge_room >= 0 && pge_room < 0x40) {
			pge = _pge_liveTable1[pge_room];
			while (pge) {
				if (pge->pos_x <= 32) {
					prepareAnimsHelper(pge, 256, 0);
				}
				pge = pge->next_PGE_in_room;
			}
		}
	}
}

void Game::prepareAnimsHelper(LivePGE *pge, int16_t dx, int16_t dy) {
	debug(DBG_GAME, "Game::prepareAnimsHelper() dx=0x%X dy=0x%X pge_num=%ld pge->flags=0x%X pge->anim_number=0x%X", dx, dy, pge - &_pgeLive[0], pge->flags, pge->anim_number);
	if (!(pge->flags & 8)) {
		if (pge->index != 0 && loadMonsterSprites(pge) == 0) {
			return;
		}
		const uint8_t *dataPtr = 0;
		int8_t dw = 0, dh = 0;
		switch (_res._type) {
		case kResourceTypeAmiga:
		case kResourceTypeDOS:
			assert(pge->anim_number < 1287);
			dataPtr = _res._sprData[pge->anim_number];
			if (dataPtr == 0) {
				return;
			}
			dw = (int8_t)dataPtr[0];
			dh = (int8_t)dataPtr[1];
			break;
		case kResourceTypeMac:
			break;
		}
		uint8_t w = 0, h = 0;
		switch (_res._type) {
		case kResourceTypeAmiga:
			w = ((dataPtr[2] >> 7) + 1) * 16;
			h = dataPtr[2] & 0x7F;
			break;
		case kResourceTypeDOS:
			w = dataPtr[2];
			h = dataPtr[3];
			dataPtr += 4;
			break;
		case kResourceTypeMac:
			break;
		}
		int16_t ypos = dy + pge->pos_y - dh + 2;
		int16_t xpos = dx + pge->pos_x - dw;
		if (pge->flags & 2) {
			xpos = dw + dx + pge->pos_x;
			uint8_t _cl = w;
			if (_cl & 0x40) {
				_cl = h;
			} else {
				_cl &= 0x3F;
			}
			xpos -= _cl;
		}
		if (xpos <= -32 || xpos >= 256 || ypos < -48 || ypos >= 224) {
			return;
		}
		xpos += 8;
		if (pge == &_pgeLive[0]) {
			_animBuffers.addState(1, xpos, ypos, dataPtr, pge, w, h);
		} else if (pge->flags & 0x10) {
			_animBuffers.addState(2, xpos, ypos, dataPtr, pge, w, h);
		} else {
			_animBuffers.addState(0, xpos, ypos, dataPtr, pge, w, h);
		}
	} else {
		const uint8_t *dataPtr = 0;
		switch (_res._type) {
		case kResourceTypeAmiga:
		case kResourceTypeDOS:
			assert(pge->anim_number < _res._numSpc);
			dataPtr = _res._spc + READ_BE_UINT16(_res._spc + pge->anim_number * 2);
			break;
		case kResourceTypeMac:
			break;
		}
		const int16_t xpos = dx + pge->pos_x + 8;
		const int16_t ypos = dy + pge->pos_y + 2;
		if (pge->init_PGE->object_type == 11) {
			_animBuffers.addState(3, xpos, ypos, dataPtr, pge);
		} else if (pge->flags & 0x10) {
			_animBuffers.addState(2, xpos, ypos, dataPtr, pge);
		} else {
			_animBuffers.addState(0, xpos, ypos, dataPtr, pge);
		}
	}
}

void Game::drawAnims() {
	debug(DBG_GAME, "Game::drawAnims()");
	_eraseBackground = false;
	drawAnimBuffer(2, _animBuffer2State);
	drawAnimBuffer(1, _animBuffer1State);
	drawAnimBuffer(0, _animBuffer0State);
	_eraseBackground = true;
	drawAnimBuffer(3, _animBuffer3State);
}

void Game::drawAnimBuffer(uint8_t stateNum, AnimBufferState *state) {
	debug(DBG_GAME, "Game::drawAnimBuffer() state=%d", stateNum);
	assert(stateNum < 4);
	_animBuffers._states[stateNum] = state;
	uint8_t lastPos = _animBuffers._curPos[stateNum];
	if (lastPos != 0xFF) {
		uint8_t numAnims = lastPos + 1;
		state += lastPos;
		_animBuffers._curPos[stateNum] = 0xFF;
		do {
			LivePGE *pge = state->pge;
			if (!(pge->flags & 8)) {
				if (stateNum == 1 && (_blinkingConradCounter & 1)) {
					break;
				}
				switch (_res._type) {
				case kResourceTypeAmiga:
					_vid.AMIGA_decodeSpm(state->dataPtr, _res._scratchBuffer);
					drawCharacter(_res._scratchBuffer, state->x, state->y, state->h, state->w, pge->flags);
					break;
				case kResourceTypeDOS:
					if (!(state->dataPtr[-2] & 0x80)) {
						_vid.PC_decodeSpm(state->dataPtr, _res._scratchBuffer);
						drawCharacter(_res._scratchBuffer, state->x, state->y, state->h, state->w, pge->flags);
					} else {
						drawCharacter(state->dataPtr, state->x, state->y, state->h, state->w, pge->flags);
					}
					break;
				case kResourceTypeMac:
					drawPiege(state);
					break;
				}
			} else {
				drawPiege(state);
			}
			--state;
		} while (--numAnims != 0);
	}
}

void Game::drawPiege(AnimBufferState *state) {
	LivePGE *pge = state->pge;
	switch (_res._type) {
	case kResourceTypeAmiga:
	case kResourceTypeDOS:
		drawObject(state->dataPtr, state->x, state->y, pge->flags);
		break;
	case kResourceTypeMac:
		if (pge->flags & 8) {
			_vid.MAC_drawSprite(state->x, state->y, _res._spc, pge->anim_number, (pge->flags & 2) != 0, _eraseBackground);
		} else if (pge->index == 0) {
			if (pge->anim_number == 0x386) {
				break;
			}
			const int frame = _res.MAC_getPersoFrame(pge->anim_number);
			_vid.MAC_drawSprite(state->x, state->y, _res._perso, frame, (pge->flags & 2) != 0, _eraseBackground);
		} else {
			const int frame = _res.MAC_getMonsterFrame(pge->anim_number);
			_vid.MAC_drawSprite(state->x, state->y, _res._monster, frame, (pge->flags & 2) != 0, _eraseBackground);
		}
		break;
	}
}

void Game::drawObject(const uint8_t *dataPtr, int16_t x, int16_t y, uint8_t flags) {
	debug(DBG_GAME, "Game::drawObject() dataPtr[]=0x%X dx=%d dy=%d",  dataPtr[0], (int8_t)dataPtr[1], (int8_t)dataPtr[2]);
	assert(dataPtr[0] < 0x4A);
	uint8_t slot = _res._rp[dataPtr[0]];
	uint8_t *data = _res.findBankData(slot);
	if (data == 0) {
		data = _res.loadBankData(slot);
	}
	int16_t posy = y - (int8_t)dataPtr[2];
	int16_t posx = x;
	if (flags & 2) {
		posx += (int8_t)dataPtr[1];
	} else {
		posx -= (int8_t)dataPtr[1];
	}
	int count = 0;
	switch (_res._type) {
	case kResourceTypeAmiga:
		count = dataPtr[8];
		dataPtr += 9;
		break;
	case kResourceTypeDOS:
		count = dataPtr[5];
		dataPtr += 6;
		break;
	case kResourceTypeMac:
		assert(0); // different graphics format
		break;
	}
	for (int i = 0; i < count; ++i) {
		drawObjectFrame(data, dataPtr, posx, posy, flags);
		dataPtr += 4;
	}
}

void Game::drawObjectFrame(const uint8_t *bankDataPtr, const uint8_t *dataPtr, int16_t x, int16_t y, uint8_t flags) {
	debug(DBG_GAME, "Game::drawObjectFrame(%p, %d, %d, 0x%X)", dataPtr, x, y, flags);
	const uint8_t *src = bankDataPtr + dataPtr[0] * 32;

	int16_t sprite_y = y + dataPtr[2];
	int16_t sprite_x;
	if (flags & 2) {
		sprite_x = x - dataPtr[1] - (((dataPtr[3] & 0xC) + 4) * 2);
	} else {
		sprite_x = x + dataPtr[1];
	}

	uint8_t sprite_flags = dataPtr[3];
	if (flags & 2) {
		sprite_flags ^= 0x10;
	}

	uint8_t sprite_h = (((sprite_flags >> 0) & 3) + 1) * 8;
	uint8_t sprite_w = (((sprite_flags >> 2) & 3) + 1) * 8;

	switch (_res._type) {
	case kResourceTypeAmiga:
		_vid.AMIGA_decodeSpc(src, sprite_w, sprite_h, _res._scratchBuffer);
		break;
	case kResourceTypeDOS:
		_vid.PC_decodeSpc(src, sprite_w, sprite_h, _res._scratchBuffer);
		break;
	case kResourceTypeMac:
		assert(0); // different graphics format
		break;
	}

	src = _res._scratchBuffer;
	bool sprite_mirror_x = false;
	int16_t sprite_clipped_w;
	if (sprite_x >= 0) {
		sprite_clipped_w = sprite_x + sprite_w;
		if (sprite_clipped_w < 256) {
			sprite_clipped_w = sprite_w;
		} else {
			sprite_clipped_w = 256 - sprite_x;
			if (sprite_flags & 0x10) {
				sprite_mirror_x = true;
				src += sprite_w - 1;
			}
		}
	} else {
		sprite_clipped_w = sprite_x + sprite_w;
		if (!(sprite_flags & 0x10)) {
			src -= sprite_x;
			sprite_x = 0;
		} else {
			sprite_mirror_x = true;
			src += sprite_x + sprite_w - 1;
			sprite_x = 0;
		}
	}
	if (sprite_clipped_w <= 0) {
		return;
	}

	int16_t sprite_clipped_h;
	if (sprite_y >= 0) {
		sprite_clipped_h = 224 - sprite_h;
		if (sprite_y < sprite_clipped_h) {
			sprite_clipped_h = sprite_h;
		} else {
			sprite_clipped_h = 224 - sprite_y;
		}
	} else {
		sprite_clipped_h = sprite_h + sprite_y;
		src -= sprite_w * sprite_y;
		sprite_y = 0;
	}
	if (sprite_clipped_h <= 0) {
		return;
	}

	if (!sprite_mirror_x && (sprite_flags & 0x10)) {
		src += sprite_w - 1;
	}

	uint32_t dst_offset = 256 * sprite_y + sprite_x;
	uint8_t sprite_col_mask = (flags & 0x60) >> 1;

	if (_eraseBackground) {
		if (!(sprite_flags & 0x10)) {
			_vid.drawSpriteSub1(src, _vid._frontLayer + dst_offset, sprite_w, sprite_clipped_h, sprite_clipped_w, sprite_col_mask);
		} else {
			_vid.drawSpriteSub2(src, _vid._frontLayer + dst_offset, sprite_w, sprite_clipped_h, sprite_clipped_w, sprite_col_mask);
		}
	} else {
		if (!(sprite_flags & 0x10)) {
			_vid.drawSpriteSub3(src, _vid._frontLayer + dst_offset, sprite_w, sprite_clipped_h, sprite_clipped_w, sprite_col_mask);
		} else {
			_vid.drawSpriteSub4(src, _vid._frontLayer + dst_offset, sprite_w, sprite_clipped_h, sprite_clipped_w, sprite_col_mask);
		}
	}
	_vid.markBlockAsDirty(sprite_x, sprite_y, sprite_clipped_w, sprite_clipped_h, _vid._layerScale);
}

void Game::drawCharacter(const uint8_t *dataPtr, int16_t pos_x, int16_t pos_y, uint8_t a, uint8_t b, uint8_t flags) {
	debug(DBG_GAME, "Game::drawCharacter(%p, %d, %d, 0x%X, 0x%X, 0x%X)", dataPtr, pos_x, pos_y, a, b, flags);
	bool var16 = false; // sprite_mirror_y
	if (b & 0x40) {
		b &= 0xBF;
		SWAP(a, b);
		var16 = true;
	}
	uint16_t sprite_h = a;
	uint16_t sprite_w = b;

	const uint8_t *src = dataPtr;
	bool var14 = false;

	int16_t sprite_clipped_w;
	if (pos_x >= 0) {
		if (pos_x + sprite_w < 256) {
			sprite_clipped_w = sprite_w;
		} else {
			sprite_clipped_w = 256 - pos_x;
			if (flags & 2) {
				var14 = true;
				if (var16) {
					src += (sprite_w - 1) * sprite_h;
				} else {
					src += sprite_w - 1;
				}
			}
		}
	} else {
		sprite_clipped_w = pos_x + sprite_w;
		if (!(flags & 2)) {
			if (var16) {
				src -= sprite_h * pos_x;
				pos_x = 0;
			} else {
				src -= pos_x;
				pos_x = 0;
			}
		} else {
			var14 = true;
			if (var16) {
				src += sprite_h * (pos_x + sprite_w - 1);
				pos_x = 0;
			} else {
				src += pos_x + sprite_w - 1;
				var14 = true;
				pos_x = 0;
			}
		}
	}
	if (sprite_clipped_w <= 0) {
		return;
	}

	int16_t sprite_clipped_h;
	if (pos_y >= 0) {
		if (pos_y < 224 - sprite_h) {
			sprite_clipped_h = sprite_h;
		} else {
			sprite_clipped_h = 224 - pos_y;
		}
	} else {
		sprite_clipped_h = sprite_h + pos_y;
		if (var16) {
			src -= pos_y;
		} else {
			src -= sprite_w * pos_y;
		}
		pos_y = 0;
	}
	if (sprite_clipped_h <= 0) {
		return;
	}

	if (!var14 && (flags & 2)) {
		if (var16) {
			src += sprite_h * (sprite_w - 1);
		} else {
			src += sprite_w - 1;
		}
	}

	uint32_t dst_offset = 256 * pos_y + pos_x;
	uint8_t sprite_col_mask = ((flags & 0x60) == 0x60) ? 0x50 : 0x40;

	debug(DBG_GAME, "dst_offset=0x%X src_offset=%ld", dst_offset, src - dataPtr);

	if (!(flags & 2)) {
		if (var16) {
			_vid.drawSpriteSub5(src, _vid._frontLayer + dst_offset, sprite_h, sprite_clipped_h, sprite_clipped_w, sprite_col_mask);
		} else {
			_vid.drawSpriteSub3(src, _vid._frontLayer + dst_offset, sprite_w, sprite_clipped_h, sprite_clipped_w, sprite_col_mask);
		}
	} else {
		if (var16) {
			_vid.drawSpriteSub6(src, _vid._frontLayer + dst_offset, sprite_h, sprite_clipped_h, sprite_clipped_w, sprite_col_mask);
		} else {
			_vid.drawSpriteSub4(src, _vid._frontLayer + dst_offset, sprite_w, sprite_clipped_h, sprite_clipped_w, sprite_col_mask);
		}
	}
	_vid.markBlockAsDirty(pos_x, pos_y, sprite_clipped_w, sprite_clipped_h, _vid._layerScale);
}

int Game::loadMonsterSprites(LivePGE *pge) {
	debug(DBG_GAME, "Game::loadMonsterSprites()");
	InitPGE *init_pge = pge->init_PGE;
	if (init_pge->obj_node_number != 0x49 && init_pge->object_type != 10) {
		return 0xFFFF;
	}
	if (init_pge->obj_node_number == _curMonsterFrame) {
		return 0xFFFF;
	}
	if (pge->room_location != _currentRoom) {
		return 0;
	}

	const uint8_t *mList = _monsterListLevels[_currentLevel];
	while (*mList != init_pge->obj_node_number) {
		if (*mList == 0xFF) { // end of list
			return 0;
		}
		mList += 2;
	}
	_curMonsterFrame = mList[0];
	if (_curMonsterNum != mList[1]) {
		_curMonsterNum = mList[1];
		switch (_res._type) {
		case kResourceTypeAmiga: {
				_res.load(_monsterNames[1][_curMonsterNum], Resource::OT_SPM);
				static const uint8_t tab[4] = { 0, 8, 0, 8 };
				const int offset = _vid._mapPalSlot3 * 16 + tab[_curMonsterNum];
				for (int i = 0; i < 8; ++i) {
					_vid.setPaletteColorBE(0x50 + i, offset + i);
				}
			}
			break;
		case kResourceTypeDOS: {
				const char *name = _monsterNames[0][_curMonsterNum];
				_res.load(name, Resource::OT_SPRM);
				_res.load_SPR_OFF(name, _res._sprm);
				_vid.setPaletteSlotLE(5, _monsterPals[_curMonsterNum]);
			}
			break;
		case kResourceTypeMac: {
				Color palette[256];
				_res.MAC_loadMonsterData(_monsterNames[0][_curMonsterNum], palette);
				static const int kMonsterPalette = 5;
				for (int i = 0; i < 16; ++i) {
					const int color = kMonsterPalette * 16 + i;
					_stub->setPaletteEntry(color, &palette[color]);
				}
			}
			break;
		}
	}
	return 0xFFFF;
}

bool Game::hasLevelMap(int level, int room) const {
	if (_res._type == kResourceTypeMac) {
		return _res.MAC_hasLevelMap(level, room);
	}
	if (_res._map) {
		return READ_LE_UINT32(_res._map + room * 6) != 0;
	} else if (_res._lev) {
		return READ_BE_UINT32(_res._lev + room * 4) != 0;
	}
	return false;
}

void Game::loadLevelMap() {
	debug(DBG_GAME, "Game::loadLevelMap() room=%d", _currentRoom);
	bool widescreenUpdated = false;
	_currentIcon = 0xFF;
	switch (_res._type) {
	case kResourceTypeAmiga:
		if (_currentLevel == 1) {
			int num = 0;
			switch (_currentRoom) {
			case 14:
			case 19:
			case 52:
			case 53:
				num = 1;
				break;
			case 11:
			case 24:
			case 27:
			case 56:
				num = 2;
				break;
			}
			if (num != 0 && _res._levNum != num) {
				char name[9];
				snprintf(name, sizeof(name), "level2_%d", num);
				_res.load(name, Resource::OT_LEV);
				_res._levNum = num;
			}
		}
		_vid.AMIGA_decodeLev(_currentLevel, _currentRoom);
		break;
	case kResourceTypeDOS:
		if (_stub->hasWidescreen() && _widescreenMode == kWidescreenAdjacentRooms) {
			const int leftRoom = _res._ctData[CT_LEFT_ROOM + _currentRoom];
			if (leftRoom > 0 && hasLevelMap(_currentLevel, leftRoom)) {
				_vid.PC_decodeMap(_currentLevel, leftRoom);
				_stub->copyWidescreenLeft(Video::GAMESCREEN_W, Video::GAMESCREEN_H, _vid._backLayer);
			} else {
				_stub->copyWidescreenLeft(Video::GAMESCREEN_W, Video::GAMESCREEN_H, 0);
			}
			const int rightRoom = _res._ctData[CT_RIGHT_ROOM + _currentRoom];
			if (rightRoom > 0 && hasLevelMap(_currentLevel, rightRoom)) {
				_vid.PC_decodeMap(_currentLevel, rightRoom);
				_stub->copyWidescreenRight(Video::GAMESCREEN_W, Video::GAMESCREEN_H, _vid._backLayer);
			} else {
				_stub->copyWidescreenRight(Video::GAMESCREEN_W, Video::GAMESCREEN_H, 0);
			}
			widescreenUpdated = true;
		}
		_vid.PC_decodeMap(_currentLevel, _currentRoom);
		break;
	case kResourceTypeMac:
		if (_stub->hasWidescreen() && _widescreenMode == kWidescreenAdjacentRooms) {
			const int leftRoom = _res._ctData[CT_LEFT_ROOM + _currentRoom];
			if (leftRoom > 0 && hasLevelMap(_currentLevel, leftRoom)) {
				_vid.MAC_decodeMap(_currentLevel, leftRoom);
				_stub->copyWidescreenLeft(_vid._w, _vid._h, _vid._backLayer);
			} else {
				_stub->copyWidescreenLeft(_vid._w, _vid._h, 0);
			}
			const int rightRoom = _res._ctData[CT_RIGHT_ROOM + _currentRoom];
			if (rightRoom > 0 && hasLevelMap(_currentLevel, rightRoom)) {
				_vid.MAC_decodeMap(_currentLevel, rightRoom);
				_stub->copyWidescreenRight(_vid._w, _vid._h, _vid._backLayer);
			} else {
				_stub->copyWidescreenRight(_vid._w, _vid._h, 0);
			}
			widescreenUpdated = true;
		}
		_vid.MAC_decodeMap(_currentLevel, _currentRoom);
		break;
	}
	if (!widescreenUpdated) {
		_vid.updateWidescreen();
	}
}

void Game::loadLevelData() {
	_res.clearLevelRes();
	const Level *lvl = &_gameLevels[_currentLevel];
	switch (_res._type) {
	case kResourceTypeAmiga:
		if (_res._isDemo) {
			static const char *fname1 = "demo";
			static const char *fname2 = "demof";
			_res.load(fname1, Resource::OT_MBK);
			_res.load(fname1, Resource::OT_CT);
			_res.load(fname1, Resource::OT_PAL);
			_res.load(fname1, Resource::OT_RPC);
			_res.load(fname1, Resource::OT_SPC);
			_res.load(fname1, Resource::OT_LEV);
			_res.load(fname2, Resource::OT_PGE);
			_res.load(fname1, Resource::OT_OBJ);
			_res.load(fname1, Resource::OT_ANI);
			_res.load(fname2, Resource::OT_TBN);
			_res.load_SPL_demo();
			_res.load("level1", Resource::OT_SGD);
			break;
		}
		{
			const char *name = lvl->nameAmiga;
			if (_currentLevel == 4) {
				name = _gameLevels[3].nameAmiga;
			}
			_res.load(name, Resource::OT_MBK);
			if (_currentLevel == 6) {
				_res.load(_gameLevels[5].nameAmiga, Resource::OT_CT);
			} else {
				_res.load(name, Resource::OT_CT);
			}
			_res.load(name, Resource::OT_PAL);
			_res.load(name, Resource::OT_RPC);
			_res.load(name, Resource::OT_SPC);
			if (_currentLevel == 1) {
				_res.load("level2_1", Resource::OT_LEV);
				_res._levNum = 1;
			} else {
				_res.load(name, Resource::OT_LEV);
			}
		}
		_res.load(lvl->nameAmiga, Resource::OT_PGE);
		_res.load(lvl->nameAmiga, Resource::OT_OBC);
		_res.load(lvl->nameAmiga, Resource::OT_ANI);
		_res.load(lvl->nameAmiga, Resource::OT_TBN);
		{
			char name[8];
			snprintf(name, sizeof(name), "level%d", lvl->sound);
			_res.load(name, Resource::OT_SPL);
		}
		if (_currentLevel == 0) {
			_res.load(lvl->nameAmiga, Resource::OT_SGD);
		}
		break;
	case kResourceTypeDOS:
		_res.load(lvl->name, Resource::OT_MBK);
		_res.load(lvl->name, Resource::OT_CT);
		_res.load(lvl->name, Resource::OT_PAL);
		_res.load(lvl->name, Resource::OT_RP);
		if (_res._isDemo || g_options.use_tile_data) { // use .BNQ/.LEV/(.SGD) instead of .MAP (PC demo)
			if (_currentLevel == 0) {
				_res.load(lvl->name, Resource::OT_SGD);
			}
			_res.load(lvl->name, Resource::OT_LEV);
			_res.load(lvl->name, Resource::OT_BNQ);
		} else {
			_res.load(lvl->name, Resource::OT_MAP);
		}
		_res.load(lvl->name2, Resource::OT_PGE);
		_res.load(lvl->name2, Resource::OT_OBJ);
		_res.load(lvl->name2, Resource::OT_ANI);
		_res.load(lvl->name2, Resource::OT_TBN);
		break;
	case kResourceTypeMac:
		_res.MAC_unloadLevelData();
		_res.MAC_loadLevelData(_currentLevel);
		break;
	}

	_cut._id = lvl->cutscene_id;
	if (_res._isDemo && _currentLevel == 5) { // PC demo does not include TELEPORT.*
		_cut._id = 0xFFFF;
	}

	_curMonsterNum = 0xFFFF;
	_curMonsterFrame = 0;

	_res.clearBankData();
	_printLevelCodeCounter = 150;

	_col_slots2Cur = _col_slots2;
	_col_slots2Next = 0;

	memset(_pge_liveTable2, 0, sizeof(_pge_liveTable2));
	memset(_pge_liveTable1, 0, sizeof(_pge_liveTable1));

	_currentRoom = _res._pgeInit[0].init_room;
	uint16_t n = _res._pgeNum;
	while (n--) {
		pge_loadForCurrentLevel(n);
	}

	if (_demoBin != -1) {
		_cut._id = -1;
		if (_demoInputs[_demoBin].room != 255) {
			_pgeLive[0].room_location = _demoInputs[_demoBin].room;
			_pgeLive[0].pos_x = _demoInputs[_demoBin].x;
			_pgeLive[0].pos_y = _demoInputs[_demoBin].y;
			_inp_demPos = 0;
		} else {
			_inp_demPos = 1;
		}
		_printLevelCodeCounter = 0;
	}

	for (uint16_t i = 0; i < _res._pgeNum; ++i) {
		if (_res._pgeInit[i].skill <= _skillLevel) {
			LivePGE *pge = &_pgeLive[i];
			pge->next_PGE_in_room = _pge_liveTable1[pge->room_location];
			_pge_liveTable1[pge->room_location] = pge;
		}
	}
	pge_resetGroups();
	_validSaveState = false;

	_mix.playMusic(Mixer::MUSIC_TRACK + lvl->track);
}

void Game::drawIcon(uint8_t iconNum, int16_t x, int16_t y, uint8_t colMask) {
	uint8_t buf[16 * 16];
	switch (_res._type) {
	case kResourceTypeAmiga:
		if (iconNum > 30) {
			// inventory icons
			switch (iconNum) {
			case 76: // cursor
				memset(buf, 0, 16 * 16);
				for (int i = 0; i < 3; ++i) {
					buf[i] = buf[15 * 16 + (15 - i)] = 1;
					buf[i * 16] = buf[(15 - i) * 16 + 15] = 1;
				}
				break;
			case 77: // up - icon.spr 4
				memset(buf, 0, 16 * 16);
				_vid.AMIGA_decodeIcn(_res._icn, 35, buf);
				break;
			case 78: // down - icon.spr 5
				memset(buf, 0, 16 * 16);
				_vid.AMIGA_decodeIcn(_res._icn, 36, buf);
				break;
			default:
				memset(buf, 5, 16 * 16);
				break;
			}
		} else {
			_vid.AMIGA_decodeIcn(_res._icn, iconNum, buf);
		}
		break;
	case kResourceTypeDOS:
		_vid.PC_decodeIcn(_res._icn, iconNum, buf);
		break;
	case kResourceTypeMac:
		switch (iconNum) {
		case 76: // cursor
			iconNum = 32;
			break;
		case 77: // up
			iconNum = 33;
			break;
		case 78: // down
			iconNum = 34;
			break;
		}
		_vid.MAC_drawSprite(x, y, _res._icn, iconNum, false, true);
		return;
	}
	_vid.drawSpriteSub1(buf, _vid._frontLayer + x + y * _vid._w, 16, 16, 16, colMask << 4);
	_vid.markBlockAsDirty(x, y, 16, 16, _vid._layerScale);
}

void Game::playSound(uint8_t num, uint8_t softVol) {
	if (num < _res._numSfx) {
		SoundFx *sfx = &_res._sfxList[num];
		if (sfx->data) {
			const int volume = Mixer::MAX_VOLUME >> (2 * softVol);
			_mix.play(sfx->data, sfx->len, sfx->freq, volume);
		}
	} else if (num == 66) {
		// open/close inventory (DOS)
	} else if (num >= 68 && num <= 75) {
		// in-game music
		_mix.playMusic(num);
	} else if (num == 77) {
		// triggered when Conrad reaches a platform
	} else {
		warning("Unknown sound num %d", num);
	}
}

uint16_t Game::getRandomNumber() {
	uint32_t n = _randSeed * 2;
	if (((int32_t)_randSeed) >= 0) {
		n ^= 0x1D872B41;
	}
	_randSeed = n;
	return n & 0xFFFF;
}

void Game::changeLevel() {
	_vid.fadeOut();
	clearStateRewind();
	loadLevelData();
	loadLevelMap();
	_vid.setPalette0xF();
	_vid.setTextPalette();
	_vid.fullRefresh();
}

void Game::handleInventory() {
	LivePGE *selected_pge = 0;
	LivePGE *pge = &_pgeLive[0];
	if (pge->life > 0 && pge->current_inventory_PGE != 0xFF) {
		playSound(66, 0);
		InventoryItem items[24];
		int num_items = 0;
		uint8_t inv_pge = pge->current_inventory_PGE;
		while (inv_pge != 0xFF) {
			items[num_items].icon_num = _res._pgeInit[inv_pge].icon_num;
			items[num_items].init_pge = &_res._pgeInit[inv_pge];
			items[num_items].live_pge = &_pgeLive[inv_pge];
			inv_pge = _pgeLive[inv_pge].next_inventory_PGE;
			++num_items;
		}
		items[num_items].icon_num = 0xFF;
		int current_item = 0;
		int num_lines = (num_items - 1) / 4 + 1;
		int current_line = 0;
		bool display_score = false;
		while (!_stub->_pi.backspace && !_stub->_pi.quit) {
			static const int icon_spr_w = 16;
			static const int icon_spr_h = 16;
			switch (_res._type) {
			case kResourceTypeAmiga:
			case kResourceTypeDOS: {
					// draw inventory background
					int icon_num = 31;
					for (int y = 140; y < 140 + 5 * icon_spr_h; y += icon_spr_h) {
						for (int x = 56; x < 56 + 9 * icon_spr_w; x += icon_spr_w) {
							drawIcon(icon_num, x, y, 0xF);
							++icon_num;
						}
					}
				}
				if (_res._type == kResourceTypeAmiga) {
					// draw outline rectangle
					static const uint8_t outline_color = 0xE7;
					uint8_t *p = _vid._frontLayer + 140 * Video::GAMESCREEN_W + 56;
					memset(p + 1, outline_color, 9 * icon_spr_w - 2);
					p += Video::GAMESCREEN_W;
					for (int y = 1; y < 5 * icon_spr_h - 1; ++y) {
						p[0] = p[9 * icon_spr_w - 1] = outline_color;
						p += Video::GAMESCREEN_W;
					}
					memset(p + 1, outline_color, 9 * icon_spr_w - 2);
				}
				break;
			case kResourceTypeMac:
				drawIcon(31, 56, 140, 0xF);
				break;
			}
			if (!display_score) {
				int icon_x_pos = 72;
				for (int i = 0; i < 4; ++i) {
					int item_it = current_line * 4 + i;
					if (items[item_it].icon_num == 0xFF) {
						break;
					}
					drawIcon(items[item_it].icon_num, icon_x_pos, 157, 0xA);
					if (current_item == item_it) {
						drawIcon(76, icon_x_pos, 157, 0xA);
						selected_pge = items[item_it].live_pge;
						uint8_t txt_num = items[item_it].init_pge->text_num;
						const uint8_t *str = _res.getTextString(_currentLevel, txt_num);
						drawString(str, Video::GAMESCREEN_W, 189, 0xED, true);
						if (items[item_it].init_pge->init_flags & 4) {
							char buf[10];
							snprintf(buf, sizeof(buf), "%d", selected_pge->life);
							_vid.drawString(buf, (Video::GAMESCREEN_W - strlen(buf) * Video::CHAR_W) / 2, 197, 0xED);
						}
					}
					icon_x_pos += 32;
				}
				if (current_line != 0) {
					drawIcon(78, 120, 176, 0xA); // down arrow
				}
				if (current_line != num_lines - 1) {
					drawIcon(77, 120, 143, 0xA); // up arrow
				}
			} else {
				char buf[50];
				snprintf(buf, sizeof(buf), "SCORE %08u", _score);
				_vid.drawString(buf, (114 - strlen(buf) * Video::CHAR_W) / 2 + 72, 158, 0xE5);
				snprintf(buf, sizeof(buf), "%s:%s", _res.getMenuString(LocaleData::LI_06_LEVEL), _res.getMenuString(LocaleData::LI_13_EASY + _skillLevel));
				_vid.drawString(buf, (114 - strlen(buf) * Video::CHAR_W) / 2 + 72, 166, 0xE5);
			}

			_vid.updateScreen();
			_stub->sleep(80);
			inp_update();

			if (_stub->_pi.dirMask & PlayerInput::DIR_UP) {
				_stub->_pi.dirMask &= ~PlayerInput::DIR_UP;
				if (current_line < num_lines - 1) {
					++current_line;
					current_item = current_line * 4;
				}
			}
			if (_stub->_pi.dirMask & PlayerInput::DIR_DOWN) {
				_stub->_pi.dirMask &= ~PlayerInput::DIR_DOWN;
				if (current_line > 0) {
					--current_line;
					current_item = current_line * 4;
				}
			}
			if (_stub->_pi.dirMask & PlayerInput::DIR_LEFT) {
				_stub->_pi.dirMask &= ~PlayerInput::DIR_LEFT;
				if (current_item > 0) {
					int item_num = current_item % 4;
					if (item_num > 0) {
						--current_item;
					}
				}
			}
			if (_stub->_pi.dirMask & PlayerInput::DIR_RIGHT) {
				_stub->_pi.dirMask &= ~PlayerInput::DIR_RIGHT;
				if (current_item < num_items - 1) {
					int item_num = current_item % 4;
					if (item_num < 3) {
						++current_item;
					}
				}
			}
			if (_stub->_pi.enter) {
				_stub->_pi.enter = false;
				display_score = !display_score;
			}
		}
		_vid.fullRefresh();
		_stub->_pi.backspace = false;
		if (selected_pge) {
			pge_setCurrentInventoryObject(selected_pge);
		}
		playSound(66, 0);
	}
}

void Game::inp_update() {
	_stub->processEvents();
	if (_demoBin != -1 && _inp_demPos < _res._demLen) {
		const int keymask = _res._dem[_inp_demPos++];
		_stub->_pi.dirMask = keymask & 0xF;
		_stub->_pi.enter = (keymask & 0x10) != 0;
		_stub->_pi.space = (keymask & 0x20) != 0;
		_stub->_pi.shift = (keymask & 0x40) != 0;
		_stub->_pi.backspace = (keymask & 0x80) != 0;
	}
}

void Game::makeGameStateName(uint8_t slot, char *buf) {
	sprintf(buf, "rs-level%d-%02d.state", _currentLevel + 1, slot);
}

static const uint32_t TAG_FBSV = 0x46425356;

bool Game::saveGameState(uint8_t slot) {
	if (slot == kAutoSaveSlot) {
		return saveStateRewind();
	}
	bool success = false;
	char stateFile[32];
	makeGameStateName(slot, stateFile);
	File f;
	if (!f.open(stateFile, "zwb", _savePath)) {
		warning("Unable to save state file '%s'", stateFile);
	} else {
		// header
		f.writeUint32BE(TAG_FBSV);
		f.writeUint16BE(2);
		char buf[32];
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "level=%d room=%d", _currentLevel + 1, _currentRoom);
		f.write(buf, sizeof(buf));
		// contents
		saveState(&f);
		if (f.ioErr()) {
			warning("I/O error when saving game state");
		} else {
			debug(DBG_INFO, "Saved state to slot %d", slot);
			success = true;
		}
	}
	return success;
}

bool Game::loadGameState(uint8_t slot) {
	if (slot == kAutoSaveSlot) {
		return loadStateRewind();
	}
	bool success = false;
	char stateFile[32];
	makeGameStateName(slot, stateFile);
	File f;
	if (!f.open(stateFile, "zrb", _savePath)) {
		warning("Unable to open state file '%s'", stateFile);
	} else {
		uint32_t id = f.readUint32BE();
		if (id != TAG_FBSV) {
			warning("Bad save state format");
		} else {
			uint16_t ver = f.readUint16BE();
			if (ver != 2) {
				warning("Invalid save state version");
			} else {
				// header
				char buf[32];
				f.read(buf, sizeof(buf));
				// contents
				loadState(&f);
				if (f.ioErr()) {
					warning("I/O error when loading game state");
				} else {
					debug(DBG_INFO, "Loaded state from slot %d", slot);
					success = true;
				}
			}
		}
	}
	return success;
}

void Game::saveState(File *f) {
	f->writeByte(_skillLevel);
	f->writeUint32BE(_score);
	if (_col_slots2Cur == 0) {
		f->writeUint32BE(0xFFFFFFFF);
	} else {
		f->writeUint32BE(_col_slots2Cur - &_col_slots2[0]);
	}
	if (_col_slots2Next == 0) {
		f->writeUint32BE(0xFFFFFFFF);
	} else {
		f->writeUint32BE(_col_slots2Next - &_col_slots2[0]);
	}
	for (int i = 0; i < _res._pgeNum; ++i) {
		LivePGE *pge = &_pgeLive[i];
		f->writeUint16BE(pge->obj_type);
		f->writeUint16BE(pge->pos_x);
		f->writeUint16BE(pge->pos_y);
		f->writeByte(pge->anim_seq);
		f->writeByte(pge->room_location);
		f->writeUint16BE(pge->life);
		f->writeUint16BE(pge->counter_value);
		f->writeByte(pge->collision_slot);
		f->writeByte(pge->next_inventory_PGE);
		f->writeByte(pge->current_inventory_PGE);
		f->writeByte(pge->unkF);
		f->writeUint16BE(pge->anim_number);
		f->writeByte(pge->flags);
		f->writeByte(pge->index);
		f->writeUint16BE(pge->first_obj_number);
		if (pge->next_PGE_in_room == 0) {
			f->writeUint32BE(0xFFFFFFFF);
		} else {
			f->writeUint32BE(pge->next_PGE_in_room - &_pgeLive[0]);
		}
		if (pge->init_PGE == 0) {
			f->writeUint32BE(0xFFFFFFFF);
		} else {
			f->writeUint32BE(pge->init_PGE - &_res._pgeInit[0]);
		}
	}
	f->write(&_res._ctData[0x100], 0x1C00);
	for (CollisionSlot2 *cs2 = &_col_slots2[0]; cs2 < _col_slots2Cur; ++cs2) {
		if (cs2->next_slot == 0) {
			f->writeUint32BE(0xFFFFFFFF);
		} else {
			f->writeUint32BE(cs2->next_slot - &_col_slots2[0]);
		}
		if (cs2->unk2 == 0) {
			f->writeUint32BE(0xFFFFFFFF);
		} else {
			f->writeUint32BE(cs2->unk2 - &_res._ctData[0x100]);
		}
		f->writeByte(cs2->data_size);
		f->write(cs2->data_buf, 0x10);
	}
}

void Game::loadState(File *f) {
	uint16_t i;
	uint32_t off;
	_skillLevel = f->readByte();
	_score = f->readUint32BE();
	memset(_pge_liveTable2, 0, sizeof(_pge_liveTable2));
	memset(_pge_liveTable1, 0, sizeof(_pge_liveTable1));
	off = f->readUint32BE();
	if (off == 0xFFFFFFFF) {
		_col_slots2Cur = 0;
	} else {
		_col_slots2Cur = &_col_slots2[0] + off;
	}
	off = f->readUint32BE();
	if (off == 0xFFFFFFFF) {
		_col_slots2Next = 0;
	} else {
		_col_slots2Next = &_col_slots2[0] + off;
	}
	for (i = 0; i < _res._pgeNum; ++i) {
		LivePGE *pge = &_pgeLive[i];
		pge->obj_type = f->readUint16BE();
		pge->pos_x = f->readUint16BE();
		pge->pos_y = f->readUint16BE();
		pge->anim_seq = f->readByte();
		pge->room_location = f->readByte();
		pge->life = f->readUint16BE();
		pge->counter_value = f->readUint16BE();
		pge->collision_slot = f->readByte();
		pge->next_inventory_PGE = f->readByte();
		pge->current_inventory_PGE = f->readByte();
		pge->unkF = f->readByte();
		pge->anim_number = f->readUint16BE();
		pge->flags = f->readByte();
		pge->index = f->readByte();
		pge->first_obj_number = f->readUint16BE();
		off = f->readUint32BE();
		if (off == 0xFFFFFFFF) {
			pge->next_PGE_in_room = 0;
		} else {
			pge->next_PGE_in_room = &_pgeLive[0] + off;
		}
		off = f->readUint32BE();
		if (off == 0xFFFFFFFF) {
			pge->init_PGE = 0;
		} else {
			pge->init_PGE = &_res._pgeInit[0] + off;
		}
	}
	f->read(&_res._ctData[0x100], 0x1C00);
	for (CollisionSlot2 *cs2 = &_col_slots2[0]; cs2 < _col_slots2Cur; ++cs2) {
		off = f->readUint32BE();
		if (off == 0xFFFFFFFF) {
			cs2->next_slot = 0;
		} else {
			cs2->next_slot = &_col_slots2[0] + off;
		}
		off = f->readUint32BE();
		if (off == 0xFFFFFFFF) {
			cs2->unk2 = 0;
		} else {
			cs2->unk2 = &_res._ctData[0x100] + off;
		}
		cs2->data_size = f->readByte();
		f->read(cs2->data_buf, 0x10);
	}
	for (i = 0; i < _res._pgeNum; ++i) {
		if (_res._pgeInit[i].skill <= _skillLevel) {
			LivePGE *pge = &_pgeLive[i];
			if (pge->flags & 4) {
				_pge_liveTable2[pge->index] = pge;
			}
			pge->next_PGE_in_room = _pge_liveTable1[pge->room_location];
			_pge_liveTable1[pge->room_location] = pge;
		}
	}
	resetGameState();
}

void Game::clearStateRewind() {
	// debug(DBG_INFO, "Clear rewind state (count %d)", _rewindLen);
	for (int i = 0; i < _rewindLen; ++i) {
		int ptr = _rewindPtr - i;
		if (ptr < 0) {
			ptr += kRewindSize;
		}
		_rewindBuffer[ptr].close();
	}
	_rewindPtr = -1;
	_rewindLen = 0;
}

bool Game::saveStateRewind() {
	if (_rewindPtr == kRewindSize - 1) {
		_rewindPtr = 0;
	} else {
		++_rewindPtr;
	}
	static const int kGameStateSize = 16384;
	File &f = _rewindBuffer[_rewindPtr];
	f.openMemoryBuffer(kGameStateSize);
	saveState(&f);
	if (_rewindLen < kRewindSize) {
		++_rewindLen;
	}
	// debug(DBG_INFO, "Save state for rewind (index %d, count %d, size %d)", _rewindPtr, _rewindLen, f.size());
	return !f.ioErr();
}

bool Game::loadStateRewind() {
	const int ptr = _rewindPtr;
	if (_rewindPtr == 0) {
		_rewindPtr = kRewindSize - 1;
	} else {
		--_rewindPtr;
	}
	File &f = _rewindBuffer[ptr];
	f.seek(0);
	loadState(&f);
	if (_rewindLen > 0) {
		--_rewindLen;
	}
	// debug(DBG_INFO, "Rewind state (index %d, count %d, size %d)", ptr, _rewindLen, f.size());
	return !f.ioErr();
}

void AnimBuffers::addState(uint8_t stateNum, int16_t x, int16_t y, const uint8_t *dataPtr, LivePGE *pge, uint8_t w, uint8_t h) {
	debug(DBG_GAME, "AnimBuffers::addState() stateNum=%d x=%d y=%d dataPtr=%p pge=%p", stateNum, x, y, dataPtr, pge);
	assert(stateNum < 4);
	AnimBufferState *state = _states[stateNum];
	state->x = x;
	state->y = y;
	state->w = w;
	state->h = h;
	state->dataPtr = dataPtr;
	state->pge = pge;
	++_curPos[stateNum];
	++_states[stateNum];
}
