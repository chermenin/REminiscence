
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef GAME_H__
#define GAME_H__

#include "intern.h"
#include "cutscene.h"
#include "menu.h"
#include "mixer.h"
#include "resource.h"
#include "seq_player.h"
#include "video.h"

struct File;
struct FileSystem;
struct SystemStub;

struct Game {
	typedef int (Game::*pge_OpcodeProc)(ObjectOpcodeArgs *args);
	typedef int (Game::*pge_ZOrderCallback)(LivePGE *, LivePGE *, uint8_t, uint8_t);
	typedef int (Game::*col_Callback1)(LivePGE *, LivePGE *, int16_t, int16_t);
	typedef int (Game::*col_Callback2)(LivePGE *, int16_t, int16_t, int16_t);

	enum {
		kIngameSaveSlot = 0,
		kRewindSize = 120, // 10mins (~2MB)
		kAutoSaveSlot = 255,
		kAutoSaveIntervalMs = 5 * 1000
	};

	enum {
		CT_UP_ROOM    = 0x00,
		CT_DOWN_ROOM  = 0x40,
		CT_RIGHT_ROOM = 0x80,
		CT_LEFT_ROOM  = 0xC0
	};

	static const Demo _demoInputs[3];
	static const Level _gameLevels[];
	static const uint16_t _scoreTable[];
	static const uint8_t _monsterListLevel1[];
	static const uint8_t _monsterListLevel2[];
	static const uint8_t _monsterListLevel3[];
	static const uint8_t _monsterListLevel4_1[];
	static const uint8_t _monsterListLevel4_2[];
	static const uint8_t _monsterListLevel5_1[];
	static const uint8_t _monsterListLevel5_2[];
	static const uint8_t *_monsterListLevels[];
	static const uint8_t _monsterPals[4][32];
	static const char *_monsterNames[2][4];
	static const pge_OpcodeProc _pge_opcodeTable[];
	static const uint8_t _pge_modKeysTable[];
	static const uint8_t _protectionCodeData[];
	static const uint8_t _protectionWordData[];
	static const uint8_t _protectionNumberDataAmiga[];
	static const uint8_t _protectionCodeDataAmiga[];
	static const uint8_t _protectionPal[];

	Cutscene _cut;
	Menu _menu;
	Mixer _mix;
	Resource _res;
	SeqPlayer _seq;
	Video _vid;
	SystemStub *_stub;
	FileSystem *_fs;
	const char *_savePath;
	File _rewindBuffer[kRewindSize];
	int _rewindPtr, _rewindLen;

	const uint8_t *_stringsTable;
	const char **_textsTable;
	uint8_t _currentLevel;
	uint8_t _skillLevel;
	int _demoBin;
	uint32_t _score;
	uint8_t _currentRoom;
	uint8_t _currentIcon;
	bool _loadMap;
	uint8_t _printLevelCodeCounter;
	uint32_t _randSeed;
	uint16_t _currentInventoryIconNum;
	uint16_t _curMonsterFrame;
	uint16_t _curMonsterNum;
	uint8_t _blinkingConradCounter;
	uint16_t _textToDisplay;
	bool _eraseBackground;
	AnimBufferState _animBuffer0State[41];
	AnimBufferState _animBuffer1State[6]; // Conrad
	AnimBufferState _animBuffer2State[42];
	AnimBufferState _animBuffer3State[12];
	AnimBuffers _animBuffers;
	uint16_t _deathCutsceneCounter;
	bool _saveStateCompleted;
	bool _endLoop;
	uint32_t _frameTimestamp;
	WidescreenMode _widescreenMode;
	bool _autoSave;
	uint32_t _saveTimestamp;

	Game(SystemStub *, FileSystem *, const char *savePath, int level, ResourceType ver, Language lang, WidescreenMode widescreenMode, bool autoSave);

	void run();
	void displayTitleScreenAmiga();
	void displayTitleScreenMac(int num);
	void resetGameState();
	void mainLoop();
	void updateTiming();
	void playCutscene(int id = -1);
	bool playCutsceneSeq(const char *name);
	bool hasLevelMap(int level, int room) const;
	void loadLevelMap();
	void loadLevelData();
	void drawIcon(uint8_t iconNum, int16_t x, int16_t y, uint8_t colMask);
	void drawCurrentInventoryItem();
	void printLevelCode();
	void showFinalScore();
	bool handleConfigPanel();
	bool handleContinueAbort();
	void printSaveStateCompleted();
	void drawLevelTexts();
	void drawStoryTexts();
	void drawString(const uint8_t *p, int x, int y, uint8_t color, bool hcenter = false);
	void prepareAnims();
	void prepareAnimsHelper(LivePGE *pge, int16_t dx, int16_t dy);
	void drawAnims();
	void drawAnimBuffer(uint8_t stateNum, AnimBufferState *state);
	void drawPiege(AnimBufferState *state);
	void drawObject(const uint8_t *dataPtr, int16_t x, int16_t y, uint8_t flags);
	void drawObjectFrame(const uint8_t *bankDataPtr, const uint8_t *dataPtr, int16_t x, int16_t y, uint8_t flags);
	void drawCharacter(const uint8_t *dataPtr, int16_t x, int16_t y, uint8_t a, uint8_t b, uint8_t flags);
	int loadMonsterSprites(LivePGE *pge);
	void playSound(uint8_t sfxId, uint8_t softVol);
	uint16_t getRandomNumber();
	void changeLevel();
	void handleInventory();

	// protection
	bool handleProtectionScreenShape();
	bool handleProtectionScreenWords();

	// pieges
	bool _pge_playAnimSound;
	GroupPGE _pge_groups[256];
	GroupPGE *_pge_groupsTable[256];
	GroupPGE *_pge_nextFreeGroup;
	LivePGE *_pge_liveTable2[256]; // active pieges list (index = pge number)
	LivePGE *_pge_liveTable1[256]; // pieges list by room (index = room)
	LivePGE _pgeLive[256];
	uint8_t _pge_currentPiegeRoom;
	bool _pge_currentPiegeFacingDir; // (false == left)
	bool _pge_processOBJ;
	uint8_t _pge_inpKeysMask;
	uint16_t _pge_opTempVar1;
	uint16_t _pge_opTempVar2;
	uint16_t _pge_compareVar1;
	uint16_t _pge_compareVar2;

	void pge_resetGroups();
	void pge_removeFromGroup(uint8_t idx);
	int pge_isInGroup(LivePGE *pge_dst, uint16_t group_id, uint16_t counter);
	void pge_loadForCurrentLevel(uint16_t idx);
	void pge_process(LivePGE *pge);
	void pge_setupNextAnimFrame(LivePGE *pge, GroupPGE *le);
	void pge_playAnimSound(LivePGE *pge, uint16_t arg2);
	void pge_setupAnim(LivePGE *pge);
	int pge_execute(LivePGE *live_pge, InitPGE *init_pge, const Object *obj);
	void pge_prepare();
	void pge_setupDefaultAnim(LivePGE *pge);
	uint16_t pge_processOBJ(LivePGE *pge);
	void pge_setupOtherPieges(LivePGE *pge, InitPGE *init_pge);
	void pge_addToCurrentRoomList(LivePGE *pge, uint8_t room);
	void pge_getInput();
	int pge_op_isInpUp(ObjectOpcodeArgs *args);
	int pge_op_isInpBackward(ObjectOpcodeArgs *args);
	int pge_op_isInpDown(ObjectOpcodeArgs *args);
	int pge_op_isInpForward(ObjectOpcodeArgs *args);
	int pge_op_isInpUpMod(ObjectOpcodeArgs *args);
	int pge_op_isInpBackwardMod(ObjectOpcodeArgs *args);
	int pge_op_isInpDownMod(ObjectOpcodeArgs *args);
	int pge_op_isInpForwardMod(ObjectOpcodeArgs *args);
	int pge_op_isInpIdle(ObjectOpcodeArgs *args);
	int pge_op_isInpNoMod(ObjectOpcodeArgs *args);
	int pge_op_getCollision0u(ObjectOpcodeArgs *args);
	int pge_op_getCollision00(ObjectOpcodeArgs *args);
	int pge_op_getCollision0d(ObjectOpcodeArgs *args);
	int pge_op_getCollision1u(ObjectOpcodeArgs *args);
	int pge_op_getCollision10(ObjectOpcodeArgs *args);
	int pge_op_getCollision1d(ObjectOpcodeArgs *args);
	int pge_op_getCollision2u(ObjectOpcodeArgs *args);
	int pge_op_getCollision20(ObjectOpcodeArgs *args);
	int pge_op_getCollision2d(ObjectOpcodeArgs *args);
	int pge_op_doesNotCollide0u(ObjectOpcodeArgs *args);
	int pge_op_doesNotCollide00(ObjectOpcodeArgs *args);
	int pge_op_doesNotCollide0d(ObjectOpcodeArgs *args);
	int pge_op_doesNotCollide1u(ObjectOpcodeArgs *args);
	int pge_op_doesNotCollide10(ObjectOpcodeArgs *args);
	int pge_op_doesNotCollide1d(ObjectOpcodeArgs *args);
	int pge_op_doesNotCollide2u(ObjectOpcodeArgs *args);
	int pge_op_doesNotCollide20(ObjectOpcodeArgs *args);
	int pge_op_doesNotCollide2d(ObjectOpcodeArgs *args);
	int pge_op_collides0o0d(ObjectOpcodeArgs *args);
	int pge_op_collides2o2d(ObjectOpcodeArgs *args);
	int pge_op_collides0o0u(ObjectOpcodeArgs *args);
	int pge_op_collides2o2u(ObjectOpcodeArgs *args);
	int pge_op_collides2u2o(ObjectOpcodeArgs *args);
	int pge_op_isInGroup(ObjectOpcodeArgs *args);
	int pge_op_updateGroup0(ObjectOpcodeArgs *args);
	int pge_op_updateGroup1(ObjectOpcodeArgs *args);
	int pge_op_updateGroup2(ObjectOpcodeArgs *args);
	int pge_op_updateGroup3(ObjectOpcodeArgs *args);
	int pge_op_isPiegeDead(ObjectOpcodeArgs *args);
	int pge_op_collides1u2o(ObjectOpcodeArgs *args);
	int pge_op_collides1u1o(ObjectOpcodeArgs *args);
	int pge_op_collides1o1u(ObjectOpcodeArgs *args);
	int pge_o_unk0x2B(ObjectOpcodeArgs *args);
	int pge_o_unk0x2C(ObjectOpcodeArgs *args);
	int pge_o_unk0x2D(ObjectOpcodeArgs *args);
	int pge_op_nop(ObjectOpcodeArgs *args);
	int pge_op_pickupObject(ObjectOpcodeArgs *args);
	int pge_op_addItemToInventory(ObjectOpcodeArgs *args);
	int pge_op_copyPiege(ObjectOpcodeArgs *args);
	int pge_op_canUseCurrentInventoryItem(ObjectOpcodeArgs *args);
	int pge_op_removeItemFromInventory(ObjectOpcodeArgs *args);
	int pge_o_unk0x34(ObjectOpcodeArgs *args);
	int pge_op_isInpMod(ObjectOpcodeArgs *args);
	int pge_op_setCollisionState1(ObjectOpcodeArgs *args);
	int pge_op_setCollisionState0(ObjectOpcodeArgs *args);
	int pge_op_isInGroup1(ObjectOpcodeArgs *args);
	int pge_op_isInGroup2(ObjectOpcodeArgs *args);
	int pge_op_isInGroup3(ObjectOpcodeArgs *args);
	int pge_op_isInGroup4(ObjectOpcodeArgs *args);
	int pge_o_unk0x3C(ObjectOpcodeArgs *args);
	int pge_o_unk0x3D(ObjectOpcodeArgs *args);
	int pge_op_setPiegeCounter(ObjectOpcodeArgs *args);
	int pge_op_decPiegeCounter(ObjectOpcodeArgs *args);
	int pge_o_unk0x40(ObjectOpcodeArgs *args);
	int pge_op_wakeUpPiege(ObjectOpcodeArgs *args);
	int pge_op_removePiege(ObjectOpcodeArgs *args);
	int pge_op_removePiegeIfNotNear(ObjectOpcodeArgs *args);
	int pge_op_loadPiegeCounter(ObjectOpcodeArgs *args);
	int pge_o_unk0x45(ObjectOpcodeArgs *args);
	int pge_o_unk0x46(ObjectOpcodeArgs *args);
	int pge_o_unk0x47(ObjectOpcodeArgs *args);
	int pge_o_unk0x48(ObjectOpcodeArgs *args);
	int pge_o_unk0x49(ObjectOpcodeArgs *args);
	int pge_o_unk0x4A(ObjectOpcodeArgs *args);
	int pge_op_killPiege(ObjectOpcodeArgs *args);
	int pge_op_isInCurrentRoom(ObjectOpcodeArgs *args);
	int pge_op_isNotInCurrentRoom(ObjectOpcodeArgs *args);
	int pge_op_scrollPosY(ObjectOpcodeArgs *args);
	int pge_op_playDefaultDeathCutscene(ObjectOpcodeArgs *args);
	int pge_o_unk0x50(ObjectOpcodeArgs *args);
	int pge_o_unk0x52(ObjectOpcodeArgs *args);
	int pge_o_unk0x53(ObjectOpcodeArgs *args);
	int pge_op_isPiegeNear(ObjectOpcodeArgs *args);
	int pge_op_setLife(ObjectOpcodeArgs *args);
	int pge_op_incLife(ObjectOpcodeArgs *args);
	int pge_op_setPiegeDefaultAnim(ObjectOpcodeArgs *args);
	int pge_op_setLifeCounter(ObjectOpcodeArgs *args);
	int pge_op_decLifeCounter(ObjectOpcodeArgs *args);
	int pge_op_playCutscene(ObjectOpcodeArgs *args);
	int pge_op_isTempVar2Set(ObjectOpcodeArgs *args);
	int pge_op_playDeathCutscene(ObjectOpcodeArgs *args);
	int pge_o_unk0x5D(ObjectOpcodeArgs *args);
	int pge_o_unk0x5E(ObjectOpcodeArgs *args);
	int pge_o_unk0x5F(ObjectOpcodeArgs *args);
	int pge_op_findAndCopyPiege(ObjectOpcodeArgs *args);
	int pge_op_isInRandomRange(ObjectOpcodeArgs *args);
	int pge_o_unk0x62(ObjectOpcodeArgs *args);
	int pge_o_unk0x63(ObjectOpcodeArgs *args);
	int pge_o_unk0x64(ObjectOpcodeArgs *args);
	int pge_op_addToCredits(ObjectOpcodeArgs *args);
	int pge_op_subFromCredits(ObjectOpcodeArgs *args);
	int pge_o_unk0x67(ObjectOpcodeArgs *args);
	int pge_op_setCollisionState2(ObjectOpcodeArgs *args);
	int pge_op_saveState(ObjectOpcodeArgs *args);
	int pge_o_unk0x6A(ObjectOpcodeArgs *args);
	int pge_op_isInGroupSlice(ObjectOpcodeArgs *args);
	int pge_o_unk0x6C(ObjectOpcodeArgs *args);
	int pge_op_isCollidingObject(ObjectOpcodeArgs *args);
	int pge_o_unk0x6E(ObjectOpcodeArgs *args);
	int pge_o_unk0x6F(ObjectOpcodeArgs *args);
	int pge_o_unk0x70(ObjectOpcodeArgs *args);
	int pge_o_unk0x71(ObjectOpcodeArgs *args);
	int pge_o_unk0x72(ObjectOpcodeArgs *args);
	int pge_o_unk0x73(ObjectOpcodeArgs *args);
	int pge_op_collides4u(ObjectOpcodeArgs *args);
	int pge_op_doesNotCollide4u(ObjectOpcodeArgs *args);
	int pge_op_isBelowConrad(ObjectOpcodeArgs *args);
	int pge_op_isAboveConrad(ObjectOpcodeArgs *args);
	int pge_op_isNotFacingConrad(ObjectOpcodeArgs *args);
	int pge_op_isFacingConrad(ObjectOpcodeArgs *args);
	int pge_op_collides2u1u(ObjectOpcodeArgs *args);
	int pge_op_displayText(ObjectOpcodeArgs *args);
	int pge_o_unk0x7C(ObjectOpcodeArgs *args);
	int pge_op_playSound(ObjectOpcodeArgs *args);
	int pge_o_unk0x7E(ObjectOpcodeArgs *args);
	int pge_o_unk0x7F(ObjectOpcodeArgs *args);
	int pge_op_setPiegePosX(ObjectOpcodeArgs *args);
	int pge_op_setPiegePosModX(ObjectOpcodeArgs *args);
	int pge_op_changeRoom(ObjectOpcodeArgs *args);
	int pge_op_hasInventoryItem(ObjectOpcodeArgs *args);
	int pge_op_changeLevel(ObjectOpcodeArgs *args);
	int pge_op_shakeScreen(ObjectOpcodeArgs *args);
	int pge_o_unk0x86(ObjectOpcodeArgs *args);
	int pge_op_playSoundGroup(ObjectOpcodeArgs *args);
	int pge_op_adjustPos(ObjectOpcodeArgs *args);
	int pge_op_setTempVar1(ObjectOpcodeArgs *args);
	int pge_op_isTempVar1Set(ObjectOpcodeArgs *args);
	int pge_setCurrentInventoryObject(LivePGE *pge);
	void pge_updateInventory(LivePGE *pge1, LivePGE *pge2);
	void pge_reorderInventory(LivePGE *pge);
	LivePGE *pge_getInventoryItemBefore(LivePGE *pge, LivePGE *last_pge);
	void pge_addToInventory(LivePGE *pge1, LivePGE *pge2, LivePGE *pge3);
	int pge_updateCollisionState(LivePGE *pge, int16_t pge_dy, uint8_t var8);
	int pge_ZOrder(LivePGE *pge, int16_t num, pge_ZOrderCallback compare, uint16_t unk);
	void pge_updateGroup(uint8_t idx, uint8_t unk1, int16_t unk2);
	void pge_removeFromInventory(LivePGE *pge1, LivePGE *pge2, LivePGE *pge3);
	int pge_ZOrderByAnimY(LivePGE *pge1, LivePGE *pge2, uint8_t comp, uint8_t comp2);
	int pge_ZOrderByAnimYIfType(LivePGE *pge1, LivePGE *pge2, uint8_t comp, uint8_t comp2);
	int pge_ZOrderIfIndex(LivePGE *pge1, LivePGE *pge2, uint8_t comp, uint8_t comp2);
	int pge_ZOrderByIndex(LivePGE *pge1, LivePGE *pge2, uint8_t comp, uint8_t comp2);
	int pge_ZOrderByObj(LivePGE *pge1, LivePGE *pge2, uint8_t comp, uint8_t comp2);
	int pge_ZOrderIfDifferentDirection(LivePGE *pge1, LivePGE *pge2, uint8_t comp, uint8_t comp2);
	int pge_ZOrderIfSameDirection(LivePGE *pge1, LivePGE *pge2, uint8_t comp, uint8_t comp2);
	int pge_ZOrderIfTypeAndSameDirection(LivePGE *pge1, LivePGE *pge2, uint8_t comp, uint8_t comp2);
	int pge_ZOrderIfTypeAndDifferentDirection(LivePGE *pge1, LivePGE *pge2, uint8_t comp, uint8_t comp2);
	int pge_ZOrderByNumber(LivePGE *pge1, LivePGE *pge2, uint8_t comp, uint8_t comp2);


	// collision
	CollisionSlot _col_slots[256];
	uint8_t _col_curPos;
	CollisionSlot *_col_slotsTable[256];
	CollisionSlot *_col_curSlot;
	CollisionSlot2 _col_slots2[256];
	CollisionSlot2 *_col_slots2Cur;
	CollisionSlot2 *_col_slots2Next;
	uint8_t _col_activeCollisionSlots[0x30 * 3]; // left, current, right
	uint8_t _col_currentLeftRoom;
	uint8_t _col_currentRightRoom;
	int16_t _col_currentPiegeGridPosX;
	int16_t _col_currentPiegeGridPosY;

	void col_prepareRoomState();
	void col_clearState();
	LivePGE *col_findPiege(LivePGE *pge, uint16_t arg2);
	int16_t col_findSlot(int16_t pos);
	void col_preparePiegeState(LivePGE *dst_pge);
	uint16_t col_getGridPos(LivePGE *pge, int16_t dx);
	int16_t col_getGridData(LivePGE *pge, int16_t dy, int16_t dx);
	uint8_t col_findCurrentCollidingObject(LivePGE *pge, uint8_t n1, uint8_t n2, uint8_t n3, LivePGE **pge_out);
	int16_t col_detectHit(LivePGE *pge, int16_t arg2, int16_t arg4, col_Callback1 callback1, col_Callback2 callback2, int16_t argA, int16_t argC);
	int col_detectHitCallback2(LivePGE *pge1, LivePGE *pge2, int16_t unk1, int16_t unk2);
	int col_detectHitCallback3(LivePGE *pge1, LivePGE *pge2, int16_t unk1, int16_t unk2);
	int col_detectHitCallback4(LivePGE *pge1, LivePGE *pge2, int16_t unk1, int16_t unk2);
	int col_detectHitCallback5(LivePGE *pge1, LivePGE *pge2, int16_t unk1, int16_t unk2);
	int col_detectHitCallback1(LivePGE *pge, int16_t dy, int16_t unk1, int16_t unk2);
	int col_detectHitCallback6(LivePGE *pge, int16_t dy, int16_t unk1, int16_t unk2);
	int col_detectHitCallbackHelper(LivePGE *pge, int16_t unk1);
	int col_detectGunHitCallback1(LivePGE *pge, int16_t arg2, int16_t arg4, int16_t arg6);
	int col_detectGunHitCallback2(LivePGE *pge1, LivePGE *pge2, int16_t arg4, int16_t);
	int col_detectGunHitCallback3(LivePGE *pge1, LivePGE *pge2, int16_t arg4, int16_t);
	int col_detectGunHit(LivePGE *pge, int16_t arg2, int16_t arg4, col_Callback1 callback1, col_Callback2 callback2, int16_t argA, int16_t argC);


	// input
	uint8_t _inp_lastKeysHit;
	uint8_t _inp_lastKeysHitLeftRight;
	int _inp_demPos;

	void inp_handleSpecialKeys();
	void inp_update();


	// save/load state
	uint8_t _stateSlot;
	bool _validSaveState;

	void makeGameStateName(uint8_t slot, char *buf);
	bool saveGameState(uint8_t slot);
	bool loadGameState(uint8_t slot);
	void saveState(File *f);
	void loadState(File *f);
	void clearStateRewind();
	bool saveStateRewind();
	bool loadStateRewind();
};

#endif // GAME_H__
