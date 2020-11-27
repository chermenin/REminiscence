
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef OGG_PLAYER_H__
#define OGG_PLAYER_H__

#include "intern.h"

struct FileSystem;
struct Mixer;
struct OggDecoder_impl;

struct OggPlayer {
	OggPlayer(Mixer *mixer, FileSystem *fs);
	~OggPlayer();

	bool playTrack(int num);
	void stopTrack();
	void pauseTrack();
	void resumeTrack();
	bool isPlaying() const { return _impl != 0; }
	bool mix(int16_t *buf, int len);
	static bool mixCallback(void *param, int16_t *buf, int len);

	Mixer *_mix;
	FileSystem *_fs;
	OggDecoder_impl *_impl;
};

#endif // OGG_PLAYER_H__

