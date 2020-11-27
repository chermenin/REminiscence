
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef UNPACK_H__
#define UNPACK_H__

#include "intern.h"

extern bool bytekiller_unpack(uint8_t *dst, int dstSize, const uint8_t *src, int srcSize);

#endif // UNPACK_H__
