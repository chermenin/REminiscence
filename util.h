
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef UTIL_H__
#define UTIL_H__

#include "intern.h"

enum {
	DBG_INFO   = 1 << 0,
	DBG_RES    = 1 << 1,
	DBG_MENU   = 1 << 2,
	DBG_UNPACK = 1 << 3,
	DBG_PGE    = 1 << 4,
	DBG_VIDEO  = 1 << 5,
	DBG_GAME   = 1 << 6,
	DBG_COL    = 1 << 7,
	DBG_SND    = 1 << 8,
	DBG_CUT    = 1 << 9,
	DBG_MOD    = 1 << 10,
	DBG_SFX    = 1 << 11,
	DBG_FILE   = 1 << 12,
	DBG_DEMO   = 1 << 13
};

extern uint16_t g_debugMask;

extern void debug(uint16_t cm, const char *msg, ...); // __attribute__((__format__(__printf__, 2, 3)))
extern void error(const char *msg, ...);              // __attribute__((__format__(__printf__, 1, 2)))
extern void warning(const char *msg, ...);            // __attribute__((__format__(__printf__, 1, 2)))

#endif // UTIL_H__
