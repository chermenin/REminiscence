
#ifndef CPC_PLAYER_H__
#define CPC_PLAYER_H__

#include "intern.h"
#include "file.h"

struct FileSystem;
struct Mixer;

struct CpcPlayer {

	Mixer *_mix;
	FileSystem *_fs;
	File _f;
	uint32_t _pos;
	uint32_t _nextPos;
	uint32_t _restartPos;
	char _compression[5];
	int _samplesLeft;
	int16_t _sampleL, _sampleR;

	CpcPlayer(Mixer *mixer, FileSystem *fs);
	~CpcPlayer();

	bool playTrack(int num);
	void stopTrack();
	void pauseTrack();
	void resumeTrack();

	bool nextChunk();
	int8_t readSampleData();
	bool mix(int16_t *buf, int len);
	static bool mixCallback(void *param, int16_t *buf, int len);
};

#endif
