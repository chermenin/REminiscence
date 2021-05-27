/*
 * REminiscence / REinforced
 * Copyright (C) 2020-2021 Alex Chermenin (alex@chermenin.ru)
 */

#include "systemstub.h"

ScalerParameters ScalerParameters::defaults() {
	ScalerParameters params;
	params.type = kScalerTypeInternal;
	params.name[0] = 0;
	params.factor = _internalScaler.factorMin + (_internalScaler.factorMax - _internalScaler.factorMin) / 2;
	return params;
}
