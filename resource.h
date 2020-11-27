
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef RESOURCE_H__
#define RESOURCE_H__

#include "intern.h"
#include "resource_aba.h"
#include "resource_mac.h"

struct DecodeBuffer;
struct File;
struct FileSystem;

struct LocaleData {
	enum Id {
		LI_01_CONTINUE_OR_ABORT = 0,
		LI_02_TIME,
		LI_03_CONTINUE,
		LI_04_ABORT,
		LI_05_COMPLETED,
		LI_06_LEVEL,
		LI_07_START,
		LI_08_SKILL,
		LI_09_PASSWORD,
		LI_10_INFO,
		LI_11_QUIT,
		LI_12_SKILL_LEVEL,
		LI_13_EASY,
		LI_14_NORMAL,
		LI_15_EXPERT,
		LI_16_ENTER_PASSWORD1,
		LI_17_ENTER_PASSWORD2,
		LI_18_RESUME_GAME,
		LI_19_ABORT_GAME,
		LI_20_LOAD_GAME,
		LI_21_SAVE_GAME,
		LI_22_SAVE_SLOT,
		LI_23_DEMO,

		LI_NUM
	};

	static const char *_textsTableFR[];
	static const char *_textsTableEN[];
	static const char *_textsTableDE[];
	static const char *_textsTableSP[];
	static const char *_textsTableIT[];

	static const uint8_t _stringsTableFR[];
	static const uint8_t _stringsTableEN[];
	static const uint8_t _stringsTableDE[];
	static const uint8_t _stringsTableSP[];
	static const uint8_t _stringsTableIT[];
	static const uint8_t _stringsTableJP[];

	static const uint8_t _level1TbnJP[];
	static const uint8_t _level2TbnJP[];
	static const uint8_t _level3TbnJP[];
	static const uint8_t _level41TbnJP[];
	static const uint8_t _level42TbnJP[];
	static const uint8_t _level51TbnJP[];
	static const uint8_t _level52TbnJP[];

	static const uint8_t _cineBinJP[];
	static const uint8_t _cineTxtJP[];
};

struct Resource {
	typedef void (Resource::*LoadStub)(File *);

	enum ObjectType {
		OT_MBK,
		OT_PGE,
		OT_PAL,
		OT_CT,
		OT_MAP,
		OT_SPC,
		OT_RP,
		OT_RPC,
		OT_DEMO,
		OT_ANI,
		OT_OBJ,
		OT_TBN,
		OT_SPR,
		OT_TAB,
		OT_ICN,
		OT_FNT,
		OT_TXTBIN,
		OT_CMD,
		OT_POL,
		OT_SPRM,
		OT_OFF,
		OT_CMP,
		OT_OBC,
		OT_SPL,
		OT_LEV,
		OT_SGD,
		OT_BNQ,
		OT_SPM
	};

	enum {
		NUM_SFXS = 66,
		NUM_BANK_BUFFERS = 50,
		NUM_CUTSCENE_TEXTS = 117,
		NUM_SPRITES = 1287
	};

	enum {
		kPaulaFreq = 3546897,
		kClutSize = 1024,
		kScratchBufferSize = 320 * 224 + 1024
	};

	static const uint16_t _voicesOffsetsTable[];
	static const uint32_t _spmOffsetsTable[];
	static const char *_splNames[];
	static const uint8_t _gameSavedSoundData[];
	static const uint16_t _gameSavedSoundLen;

	FileSystem *_fs;
	ResourceType _type;
	Language _lang;
	bool _isDemo;
	ResourceAba *_aba;
	ResourceMac *_mac;
	uint16_t (*_readUint16)(const void *);
	uint32_t (*_readUint32)(const void *);
	bool _hasSeqData;
	char _entryName[32];
	uint8_t *_fnt;
	uint8_t *_mbk;
	uint8_t *_icn;
	int _icnLen;
	uint8_t *_tab;
	uint8_t *_spc; // BE
	uint16_t _numSpc;
	uint8_t _rp[0x4A];
	uint8_t *_pal; // BE
	uint8_t *_ani;
	uint8_t *_tbn;
	int8_t _ctData[0x1D00];
	uint8_t *_spr1;
	uint8_t *_sprData[NUM_SPRITES]; // 0-0x22F + 0x28E-0x2E9 ... conrad, 0x22F-0x28D : junkie
	uint8_t _sprm[0x10000];
	uint16_t _pgeNum;
	InitPGE _pgeInit[256];
	uint8_t *_map;
	uint8_t *_lev;
	int _levNum;
	uint8_t *_sgd;
	uint8_t *_bnq;
	uint16_t _numObjectNodes;
	ObjectNode *_objectNodesMap[255];
	uint8_t *_scratchBuffer;
	SoundFx *_sfxList;
	uint8_t _numSfx;
	uint8_t *_cmd;
	uint8_t *_pol;
	uint8_t *_cineStrings[NUM_CUTSCENE_TEXTS];
	uint8_t *_cine_off;
	uint8_t *_cine_txt;
	const char **_textsTable;
	const uint8_t *_stringsTable;
	uint8_t *_bankData;
	uint8_t *_bankDataHead;
	uint8_t *_bankDataTail;
	BankSlot _bankBuffers[NUM_BANK_BUFFERS];
	int _bankBuffersCount;
	uint8_t *_dem;
	int _demLen;
	uint32_t _resourceMacDataSize;
	int _clutSize;
	Color _clut[kClutSize];
	uint8_t *_perso;
	uint8_t *_monster;
	uint8_t *_str;
	uint8_t *_credits;

	Resource(FileSystem *fs, ResourceType type, Language lang);
	~Resource();

	void init();
	void fini();

	void setLanguage(Language lang);

	bool isDOS()   const { return _type == kResourceTypeDOS; }
	bool isAmiga() const { return _type == kResourceTypeAmiga; }
	bool isMac()   const { return _type == kResourceTypeMac; }

	bool fileExists(const char *filename);

	void clearLevelRes();
	void load_DEM(const char *filename);
	void load_FIB(const char *fileName);
	void load_SPL_demo();
	void load_MAP_menu(const char *fileName, uint8_t *dstPtr);
	void load_PAL_menu(const char *fileName, uint8_t *dstPtr);
	void load_CMP_menu(const char *fileName);
	void load_SPR_OFF(const char *fileName, uint8_t *sprData);
	void load_CINE();
	void free_CINE();
	void load_TEXT();
	void free_TEXT();
	void unload(int objType);
	void load(const char *objName, int objType, const char *ext = 0);
	void load_CT(File *pf);
	void load_FNT(File *pf);
	void load_MBK(File *pf);
	void load_ICN(File *pf);
	void load_SPR(File *pf);
	void load_SPRM(File *pf);
	void load_RP(File *pf);
	void load_SPC(File *pf);
	void load_PAL(File *pf);
	void load_MAP(File *pf);
	void load_OBJ(File *pf);
	void free_OBJ();
	void load_OBC(File *pf);
	void decodeOBJ(const uint8_t *, int);
	void load_PGE(File *pf);
	void decodePGE(const uint8_t *, int);
	void load_ANI(File *pf);
	void load_TBN(File *pf);
	void load_CMD(File *pf);
	void load_POL(File *pf);
	void load_CMP(File *pf);
	void load_VCE(int num, int segment, uint8_t **buf, uint32_t *bufSize);
	void load_SPL(File *pf);
	void load_LEV(File *pf);
	void load_SGD(File *pf);
	void load_BNQ(File *pf);
	void load_SPM(File *f);
	const uint8_t *getAniData(int num) const {
		if (_type == kResourceTypeMac) {
			const int count = READ_BE_UINT16(_ani);
			assert(num < count);
			const int offset = READ_BE_UINT16(_ani + 2 + num * 2);
			return _ani + offset;
		}
		const int offset = _readUint16(_ani + 2 + num * 2);
		return _ani + 2 + offset;
	}
	const uint8_t *getTextString(int level, int num) const {
		if (_type == kResourceTypeMac) {
			const int count = READ_BE_UINT16(_tbn);
			assert(num < count);
			const int offset = READ_BE_UINT16(_tbn + 2 + num * 2);
			return _tbn + offset;
		}
		if (_lang == LANG_JP) {
			const uint8_t *p = 0;
			switch (level) {
			case 0:
				p = LocaleData::_level1TbnJP;
				break;
			case 1:
				p = LocaleData::_level2TbnJP;
				break;
			case 2:
				p = LocaleData::_level3TbnJP;
				break;
			case 3:
				p = LocaleData::_level41TbnJP;
				break;
			case 4:
				p = LocaleData::_level42TbnJP;
				break;
			case 5:
				p = LocaleData::_level51TbnJP;
				break;
			case 6:
				p = LocaleData::_level52TbnJP;
				break;
			default:
				return 0;
			}
			return p + READ_LE_UINT16(p + num * 2);
		}
		return _tbn + _readUint16(_tbn + num * 2);
	}
	const uint8_t *getGameString(int num) const {
		if (_type == kResourceTypeMac) {
			const int count = READ_BE_UINT16(_str);
			assert(num < count);
			const int offset = READ_BE_UINT16(_str + 2 + num * 2);
			return _str + offset;
		}
		return _stringsTable + READ_LE_UINT16(_stringsTable + num * 2);
	}
	const uint8_t *getCineString(int num) const {
		if (_type == kResourceTypeMac) {
			const int count = READ_BE_UINT16(_cine_txt);
			assert(num < count);
			const int offset = READ_BE_UINT16(_cine_txt + 2 + num * 2);
			return _cine_txt + offset;
		}
		if (_lang == LANG_JP) {
			const int offset = READ_BE_UINT16(LocaleData::_cineBinJP + num * 2);
			return LocaleData::_cineTxtJP + offset;
		}
		if (_cine_off) {
			const int offset = READ_BE_UINT16(_cine_off + num * 2);
			return _cine_txt + offset;
		}
		return (num >= 0 && num < NUM_CUTSCENE_TEXTS) ? _cineStrings[num] : 0;
	}
	const char *getMenuString(int num) const {
		return (num >= 0 && num < LocaleData::LI_NUM) ? _textsTable[num] : "";
	}
	void clearBankData();
	int getBankDataSize(uint16_t num);
	uint8_t *findBankData(uint16_t num);
	uint8_t *loadBankData(uint16_t num);

	uint8_t *decodeResourceMacData(const char *name, bool decompressLzss);
	void MAC_decodeImageData(const uint8_t *ptr, int i, DecodeBuffer *dst);
	void MAC_decodeDataCLUT(const uint8_t *ptr);
	void MAC_loadClutData();
	void MAC_loadFontData();
	void MAC_loadIconData();
	void MAC_loadPersoData();
	void MAC_loadMonsterData(const char *name, Color *clut);
	void MAC_loadTitleImage(int i, DecodeBuffer *buf);
	void MAC_unloadLevelData();
	void MAC_loadLevelData(int level);
	void MAC_loadLevelRoom(int level, int i, DecodeBuffer *dst);
	void MAC_clearClut16(Color *clut, uint8_t dest);
	void MAC_copyClut16(Color *clut, uint8_t dest, uint8_t src);
	void MAC_setupRoomClut(int level, int room, Color *clut);
	const uint8_t *MAC_getImageData(const uint8_t *ptr, int i);
	bool MAC_hasLevelMap(int level, int room) const;
	void MAC_unloadCutscene();
	void MAC_loadCutscene(const char *cutscene);
	void MAC_loadCutsceneText();
	void MAC_loadCreditsText();
	void MAC_loadSounds();

	int MAC_getPersoFrame(int anim) const {
		static const int data[] = {
			0x000, 0x22E,
			0x28E, 0x2E9,
			0x4E9, 0x506,
			-1
		};
		int offset = 0;
		for (int i = 0; data[i] != -1; i += 2) {
			if (anim >= data[i] && anim <= data[i + 1]) {
				return offset + anim - data[i];
			}
			const int count = data[i + 1] + 1 - data[i];
			offset += count;
		}
		assert(0);
		return 0;
	}
	int MAC_getMonsterFrame(int anim) const {
		static const int data[] = {
			0x22F, 0x28D, // junky - 94
			0x2EA, 0x385, // mercenai - 156
			0x387, 0x42F, // replican - 169
			0x430, 0x4E8, // glue - 185
			-1
		};
		for (int i = 0; data[i] != -1; i += 2) {
			if (anim >= data[i] && anim <= data[i + 1]) {
				return anim - data[i];
			}
		}
		assert(0);
		return 0;
	}
};

#endif // RESOURCE_H__
