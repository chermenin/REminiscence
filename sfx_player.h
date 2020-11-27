
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef SFX_PLAYER_H__
#define SFX_PLAYER_H__

#include "intern.h"

struct Mixer;

struct SfxPlayer {
	enum {
		NUM_SAMPLES = 5,
		NUM_CHANNELS = 3,
		FRAC_BITS = 12,
		PAULA_FREQ = 3546897
	};

	struct Module {
		const uint8_t *sampleData[NUM_SAMPLES];
		const uint8_t *moduleData;
	};

	struct SampleInfo {
		uint16_t len;
		uint16_t vol;
		uint16_t loopPos;
		uint16_t loopLen;
		int freq;
		int pos;
		const uint8_t *data;

		int8_t getPCM(int offset) const {
			if (offset < 0) {
				offset = 0;
			} else if (offset >= (int)len) {
				offset = len - 1;
			}
			return (int8_t)data[offset];
		}
	};

	static const uint8_t _musicData68[];
	static const uint8_t _musicData70[];
	static const uint8_t _musicData72[];
	static const uint8_t _musicData73[];
	static const uint8_t _musicData74[];
	static const uint8_t _musicData75[];
	static const uint8_t _musicDataSample1[];
	static const uint8_t _musicDataSample2[]; // tick
	static const uint8_t _musicDataSample3[]; // bell
	static const uint8_t _musicDataSample4[];
	static const uint8_t _musicDataSample5[];
	static const uint8_t _musicDataSample6[];
	static const uint8_t _musicDataSample7[];
	static const uint8_t _musicDataSample8[];
	static const Module _module68;
	static const Module _module70;
	static const Module _module72;
	static const Module _module73;
	static const Module _module74;
	static const Module _module75;
	static const uint16_t _periodTable[];

	const Module *_mod;
	bool _playing;
	int _samplesLeft;
	uint16_t _curOrder;
	uint16_t _numOrders;
	uint16_t _orderDelay;
	const uint8_t *_modData;
	SampleInfo _samples[NUM_CHANNELS];
	Mixer *_mix;

	SfxPlayer(Mixer *mixer);

	void play(uint8_t num);
	void stop();
	void playSample(int channel, const uint8_t *sampleData, uint16_t period);
	void handleTick();
	void mixSamples(int16_t *samples, int samplesLen);

	bool mix(int16_t *buf, int len);
	static bool mixCallback(void *param, int16_t *buf, int len);
};

#endif // SFX_PLAYER_H__
