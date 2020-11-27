
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef MOD_PLAYER_H__
#define MOD_PLAYER_H__

#include "intern.h"

struct FileSystem;
struct Mixer;
struct ModPlayer_impl;

struct ModPlayer {

	static const uint16_t _periodTable[];
	static const char *_modulesFiles[][2];
	static const int _modulesFilesCount;

	bool _isAmiga;
	bool _playing;
	Mixer *_mix;
        FileSystem *_fs;
	ModPlayer_impl *_impl;

        ModPlayer(Mixer *mixer, FileSystem *fs);
	~ModPlayer();

	void play(int num);
	void stop();

	static bool mixCallback(void *param, int16_t *buf, int len);
};

#endif // MOD_PLAYER_H__
