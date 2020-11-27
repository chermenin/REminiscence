
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef SEQ_PLAYER_H__
#define SEQ_PLAYER_H__

#include "intern.h"

struct File;
struct SystemStub;
struct Mixer;

struct SeqDemuxer {
	enum {
		kFrameSize = 6144,
		kAudioBufferSize = 882,
		kBuffersCount = 30
	};

	bool open(File *f);
	void close();

	bool readHeader();
	bool readFrameData();
	void fillBuffer(int num, int offset, int size);
	void clearBuffer(int num);
	void readPalette(uint8_t *dst);
	void readAudio(int16_t *dst);

	int _frameOffset;
	int _audioDataOffset;
	int _audioDataSize;
	int _paletteDataOffset;
	int _paletteDataSize;
	int _videoData;
	struct {
		int size;
		int avail;
		uint8_t *data;
	} _buffers[kBuffersCount];
	int _fileSize;
	File *_f;
};

struct SeqPlayer {
	enum {
		kVideoWidth = 256,
		kVideoHeight = 128,
		kSoundPreloadSize = 4
	};

	static const char *_namesTable[];

	struct SoundBufferQueue {
		int16_t *data;
		int size;
		int read;
		SoundBufferQueue *next;
	};

	SeqPlayer(SystemStub *stub, Mixer *mixer);
	~SeqPlayer();

	void setBackBuffer(uint8_t *buf) { _buf = buf; }
	void play(File *f);
	bool mix(int16_t *buf, int len);
	static bool mixCallback(void *param, int16_t *buf, int len);

	SystemStub *_stub;
	uint8_t *_buf;
	Mixer *_mix;
	SeqDemuxer _demux;
	int _soundQueuePreloadSize;
	SoundBufferQueue *_soundQueue;
};

#endif // SEQ_PLAYER_H__

