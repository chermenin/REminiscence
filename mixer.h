
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef MIXER_H__
#define MIXER_H__

#include "intern.h"
#include "cpc_player.h"
#include "mod_player.h"
#include "ogg_player.h"
#include "sfx_player.h"

struct MixerChunk {
	const uint8_t *data;
	uint32_t len;

	MixerChunk()
		: data(0), len(0) {
	}

	int8_t getPCM(int offset) const {
		if (offset < 0) {
			offset = 0;
		} else if (offset >= (int)len) {
			offset = len - 1;
		}
		return (int8_t)data[offset];
	}
};

struct MixerChannel {
	uint8_t active;
	uint8_t volume;
	MixerChunk chunk;
	uint32_t chunkPos;
	uint32_t chunkInc;
};

struct FileSystem;
struct SystemStub;

struct Mixer {
	typedef bool (*PremixHook)(void *userData, int16_t *buf, int len);

	enum MusicType {
		MT_NONE,
		MT_MOD,
		MT_OGG,
		MT_SFX,
		MT_CPC,
	};

	enum {
		MUSIC_TRACK = 1000,
		NUM_CHANNELS = 4,
		FRAC_BITS = 12,
		MAX_VOLUME = 64
	};

	FileSystem *_fs;
	SystemStub *_stub;
	MixerChannel _channels[NUM_CHANNELS];
	PremixHook _premixHook;
	void *_premixHookData;
	MusicType _backgroundMusicType;
	MusicType _musicType;
	CpcPlayer _cpc;
	ModPlayer _mod;
	OggPlayer _ogg;
	SfxPlayer _sfx;
	int _musicTrack;

	Mixer(FileSystem *fs, SystemStub *stub);
	void init();
	void free();
	void setPremixHook(PremixHook premixHook, void *userData);
	void play(const uint8_t *data, uint32_t len, uint16_t freq, uint8_t volume);
	bool isPlaying(const uint8_t *data) const;
	uint32_t getSampleRate() const;
	void stopAll();
	void playMusic(int num);
	void stopMusic();
	void mix(int16_t *buf, int len);

	static void mixCallback(void *param, int16_t *buf, int len);
};

#endif // MIXER_H__
