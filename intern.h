
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef INTERN_H__
#define INTERN_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

#undef ARRAYSIZE
#define ARRAYSIZE(a) (int)(sizeof(a)/sizeof(a[0]))

inline uint16_t READ_BE_UINT16(const void *ptr) {
	const uint8_t *b = (const uint8_t *)ptr;
	return (b[0] << 8) | b[1];
}

inline uint32_t READ_BE_UINT32(const void *ptr) {
	const uint8_t *b = (const uint8_t *)ptr;
	return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}

inline uint16_t READ_LE_UINT16(const void *ptr) {
	const uint8_t *b = (const uint8_t *)ptr;
	return (b[1] << 8) | b[0];
}

inline uint32_t READ_LE_UINT32(const void *ptr) {
	const uint8_t *b = (const uint8_t *)ptr;
	return (b[3] << 24) | (b[2] << 16) | (b[1] << 8) | b[0];
}

inline int16_t ADDC_S16(int a, int b) {
	a += b;
	if (a < -32768) {
		a = -32768;
	} else if (a > 32767) {
		a = 32767;
	}
	return a;
}

inline int16_t S8_to_S16(int a) {
	if (a < -128) {
		return -32768;
	} else if (a > 127) {
		return 32767;
	} else {
		const uint8_t u8 = (a ^ 0x80);
		return ((u8 << 8) | u8) - 32768;
	}
}

template<typename T>
inline void SWAP(T &a, T &b) {
	T tmp = a;
	a = b;
	b = tmp;
}

template<typename T>
inline T CLIP(const T& val, const T& a, const T& b) {
	if (val < a) {
		return a;
	} else if (val > b) {
		return b;
	}
	return val;
}

#undef MIN
template<typename T>
inline T MIN(T v1, T v2) {
	return (v1 < v2) ? v1 : v2;
}

#undef MAX
template<typename T>
inline T MAX(T v1, T v2) {
	return (v1 > v2) ? v1 : v2;
}

#undef ABS
template<typename T>
inline T ABS(T t) {
	return (t < 0) ? -t : t;
}

enum Language {
	LANG_FR,
	LANG_EN,
	LANG_DE,
	LANG_SP,
	LANG_IT,
	LANG_JP,
};

enum ResourceType {
	kResourceTypeAmiga,
	kResourceTypeDOS,
	kResourceTypeMac,
};

enum Skill {
	kSkillEasy = 0,
	kSkillNormal,
	kSkillExpert,
};

enum WidescreenMode {
	kWidescreenNone,
	kWidescreenAdjacentRooms,
	kWidescreenMirrorRoom,
	kWidescreenBlur,
};

struct Options {
	bool bypass_protection;
	bool enable_password_menu;
	bool enable_language_selection;
	bool fade_out_palette;
	bool use_tile_data;
	bool use_text_cutscenes;
	bool use_seq_cutscenes;
	bool use_words_protection;
	bool use_white_tshirt;
	bool play_asc_cutscene;
	bool play_caillou_cutscene;
	bool play_metro_cutscene;
	bool play_serrure_cutscene;
	bool play_carte_cutscene;
	bool play_gamesaved_sound;
};

struct Color {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

struct Point {
	int16_t x;
	int16_t y;
};

struct Demo {
	const char *name;
	int level;
	int room;
	int x, y;
};

struct Level {
	const char *name;
	const char *name2;
	const char *nameAmiga;
	uint16_t cutscene_id;
	uint8_t sound;
	uint8_t track;
};

struct InitPGE {
	uint16_t type;
	int16_t pos_x;
	int16_t pos_y;
	uint16_t obj_node_number;
	uint16_t life;
	int16_t counter_values[4]; // messages
	uint8_t object_type;
	uint8_t init_room;
	uint8_t room_location;
	uint8_t init_flags;
	uint8_t colliding_icon_num;
	uint8_t icon_num;
	uint8_t object_id;
	uint8_t skill;
	uint8_t mirror_x;
	uint8_t flags;
	uint8_t unk1C; // collidable, collision_data_len
	uint16_t text_num;
};

struct LivePGE {
	uint16_t obj_type;
	int16_t pos_x;
	int16_t pos_y;
	uint8_t anim_seq;
	uint8_t room_location;
	int16_t life;
	int16_t counter_value; // msg
	uint8_t collision_slot;
	uint8_t next_inventory_PGE;
	uint8_t current_inventory_PGE;
	uint8_t unkF; // unk_inventory_PGE
	uint16_t anim_number;
	uint8_t flags;
	uint8_t index;
	uint16_t first_obj_number;
	LivePGE *next_PGE_in_room;
	InitPGE *init_PGE;
};

struct GroupPGE {
	GroupPGE *next_entry;
	uint16_t index;
	uint16_t group_id;
};

struct Object {
	uint16_t type;
	int8_t dx;
	int8_t dy;
	uint16_t init_obj_type;
	uint8_t opcode2;
	uint8_t opcode1;
	uint8_t flags;
	uint8_t opcode3;
	uint16_t init_obj_number;
	int16_t opcode_arg1;
	int16_t opcode_arg2;
	int16_t opcode_arg3;
};

struct ObjectNode {
	uint16_t last_obj_number;
	Object *objects;
	uint16_t num_objects;
};

struct ObjectOpcodeArgs {
	LivePGE *pge; // arg0
	int16_t a; // arg2
	int16_t b; // arg4
};

struct AnimBufferState {
	int16_t x, y;
	uint8_t w, h;
	const uint8_t *dataPtr;
	LivePGE *pge;
};

struct AnimBuffers {
	AnimBufferState *_states[4];
	uint8_t _curPos[4];

	void addState(uint8_t stateNum, int16_t x, int16_t y, const uint8_t *dataPtr, LivePGE *pge, uint8_t w = 0, uint8_t h = 0);
};

struct CollisionSlot {
	int16_t ct_pos;
	CollisionSlot *prev_slot;
	LivePGE *live_pge;
	uint16_t index;
};

struct BankSlot {
	uint16_t entryNum;
	uint8_t *ptr;
};

struct CollisionSlot2 {
	CollisionSlot2 *next_slot;
	int8_t *unk2;
	uint8_t data_size;
	uint8_t data_buf[0x10]; // XXX check size
};

struct InventoryItem {
	uint8_t icon_num;
	InitPGE *init_pge;
	LivePGE *live_pge;
};

struct SoundFx {
	uint32_t offset;
	uint16_t len;
	uint16_t freq;
	uint8_t *data;
	int8_t peak;
};

extern Options g_options;
extern const char *g_caption;

#endif // INTERN_H__
