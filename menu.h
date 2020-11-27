
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef MENU_H__
#define MENU_H__

#include "intern.h"

struct Resource;
struct SystemStub;
struct Video;

struct Menu {
	enum {
		MENU_OPTION_ITEM_START,
		MENU_OPTION_ITEM_SKILL,
		MENU_OPTION_ITEM_PASSWORD,
		MENU_OPTION_ITEM_LEVEL,
		MENU_OPTION_ITEM_INFO,
		MENU_OPTION_ITEM_DEMO,
		MENU_OPTION_ITEM_QUIT
	};
	enum {
		SCREEN_TITLE,
		SCREEN_SKILL,
		SCREEN_PASSWORD,
		SCREEN_LEVEL,
		SCREEN_INFO
	};
	enum {
		kMacTitleScreen_MacPlay = 1,
		kMacTitleScreen_Presage = 2,
		kMacTitleScreen_Flashback = 3,
		kMacTitleScreen_LeftEye = 4,
		kMacTitleScreen_RightEye = 5,
		kMacTitleScreen_Controls = 6
	};

	enum {
		EVENTS_DELAY = 80
	};

	struct Item {
		int str;
		int opt;
	};

	static const char *_levelNames[];
	static const char *_passwordsDOS[];
	static const char *_passwordsFrAmiga[];
	static const char *_passwordsEnAmiga[];
	static const char *_passwordsMac[];

	static const uint8_t _flagEn16x12[];
	static const uint8_t _flagFr16x12[];
	static const uint8_t _flagDe16x12[];
	static const uint8_t _flagIt16x12[];
	static const uint8_t _flagJp16x12[];
	static const uint8_t _flagSp16x12[];

	Resource *_res;
	SystemStub *_stub;
	Video *_vid;

	int _currentScreen;
	int _nextScreen;
	int _selectedOption;

	int _skill;
	int _level;

	uint8_t _charVar1;
	uint8_t _charVar2;
	uint8_t _charVar3;
	uint8_t _charVar4;
	uint8_t _charVar5;

	Menu(Resource *res, SystemStub *stub, Video *vid);

	void drawString(const char *str, int16_t y, int16_t x, uint8_t color);
	void drawString2(const char *str, int16_t y, int16_t x);
	void loadPicture(const char *prefix);

	void handleInfoScreen();
	void handleSkillScreen();
	bool handlePasswordScreen();
	bool handleLevelScreen();
	void handleTitleScreen();

	const char *getLevelPassword(int level, int skill) const;
};

#endif // MENU_H__
