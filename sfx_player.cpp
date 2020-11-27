
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "mixer.h"
#include "sfx_player.h"
#include "util.h"

// volume instruments are either equal to 64 or 32 (this corresponds to aud0vol)
// use one third of the volume for master (for comparison, modplug uses a master volume of 128, max 512)
static const int kMasterVolume = 64 * 3;

// 12 dB/oct Butterworth low-pass filter at 3.3 kHz
static const bool kLowPassFilter = true;

#define NZEROS 2
#define NPOLES 2
static float bw_xf[NZEROS+1], bw_yf[NPOLES+1];
static const float GAIN = 7.655158005e+00;

static void butterworth(int16_t *p, int len) {
	for (int i = 0; i < len; ++i) {
		bw_xf[0] = bw_xf[1]; bw_xf[1] = bw_xf[2];
		bw_xf[2] = p[i] / GAIN;
		bw_yf[0] = bw_yf[1]; bw_yf[1] = bw_yf[2];
		bw_yf[2] = (bw_xf[0] + bw_xf[2]) + 2 * bw_xf[1] + (-0.2729352339 * bw_yf[0]) + (0.7504117278 * bw_yf[1]);
		p[i] = (int16_t)CLIP(bw_yf[2], -32768.f, 32767.f);
        }
}

SfxPlayer::SfxPlayer(Mixer *mixer)
	: _mod(0), _playing(false), _mix(mixer) {
}

void SfxPlayer::play(uint8_t num) {
	debug(DBG_SFX, "SfxPlayer::play(%d)", num);
	if (!_playing) {
		assert(num >= 68 && num <= 75);
		static const Module *modTable[] = {
			&_module68, &_module68, &_module70, &_module70,
			&_module72, &_module73, &_module74, &_module75
		};
		_mod = modTable[num - 68];
		_curOrder = 0;
		_numOrders = READ_BE_UINT16(_mod->moduleData);
		_orderDelay = 0;
		_modData = _mod->moduleData + 0x22;
		memset(_samples, 0, sizeof(_samples));
		_samplesLeft = 0;
		_mix->setPremixHook(mixCallback, this);
		_playing = true;
		if (kLowPassFilter) {
			memset(bw_xf, 0, sizeof(bw_xf));
			memset(bw_yf, 0, sizeof(bw_yf));
		}
	}
}

void SfxPlayer::stop() {
	if (_playing) {
		_mix->setPremixHook(0, 0);
		_playing = false;
	}
}

void SfxPlayer::playSample(int channel, const uint8_t *sampleData, uint16_t period) {
	assert(channel < NUM_CHANNELS);
	SampleInfo *si = &_samples[channel];
	si->len = READ_BE_UINT16(sampleData); sampleData += 2;
	si->vol = READ_BE_UINT16(sampleData); sampleData += 2;
	si->loopPos = READ_BE_UINT16(sampleData); sampleData += 2;
	si->loopLen = READ_BE_UINT16(sampleData); sampleData += 2;
	si->freq = PAULA_FREQ / period;
	si->pos = 0;
	si->data = sampleData;
}

void SfxPlayer::handleTick() {
	if (!_playing) {
		return;
	}
	if (_orderDelay != 0) {
		--_orderDelay;
		// check for end of song
		if (_orderDelay == 0 && _modData == 0) {
			_playing = false;
		}
	} else {
		_orderDelay = READ_BE_UINT16(_mod->moduleData + 2);
		debug(DBG_SFX, "curOrder=%d/%d _orderDelay=%d\n", _curOrder, _numOrders, _orderDelay);
		int16_t period = 0;
		for (int ch = 0; ch < 3; ++ch) {
			const uint8_t *sampleData = 0;
			uint8_t b = *_modData++;
			if (b != 0) {
				--b;
				assert(b < 5);
				period = READ_BE_UINT16(_mod->moduleData + 4 + b * 2);
				sampleData = _mod->sampleData[b];
			}
			b = *_modData++;
			if (b != 0) {
				int16_t per = period + (b - 1);
				if (per >= 0 && per < 40) {
					per = _periodTable[per];
				} else if (per == -3) {
					per = 0xA0;
				} else {
					per = 0x71;
				}
				playSample(ch, sampleData, per);
			}
		}
		++_curOrder;
		if (_curOrder >= _numOrders) {
			debug(DBG_SFX, "End of song");
			_orderDelay += 20;
			_modData = 0;
		}
	}
}

void SfxPlayer::mixSamples(int16_t *buf, int samplesLen) {
	for (int i = 0; i < NUM_CHANNELS; ++i) {
		SampleInfo *si = &_samples[i];
		if (si->data) {
			int16_t *mixbuf = buf;
			int len = si->len << FRAC_BITS;
			int loopLen = si->loopLen << FRAC_BITS;
			int loopPos = si->loopPos << FRAC_BITS;
			int deltaPos = (si->freq << FRAC_BITS) / _mix->getSampleRate();
			int curLen = samplesLen;
			int pos = si->pos;
			while (curLen != 0) {
				int count;
				if (loopLen > (2 << FRAC_BITS)) {
					assert(si->loopPos + si->loopLen <= si->len);
					if (pos >= loopPos + loopLen) {
						pos -= loopLen;
					}
					count = MIN(curLen, (loopPos + loopLen - pos - 1) / deltaPos + 1);
					curLen -= count;
				} else {
					if (pos >= len) {
						count = 0;
					} else {
						count = MIN(curLen, (len - pos - 1) / deltaPos + 1);
					}
					curLen = 0;
				}
				while (count--) {
					const int out = si->getPCM(pos >> FRAC_BITS) * si->vol / kMasterVolume;
					*mixbuf = ADDC_S16(*mixbuf, S8_to_S16(out));
					++mixbuf;
					pos += deltaPos;
				}
			}
			si->pos = pos;
		}
	}
}

bool SfxPlayer::mix(int16_t *buf, int len) {
	memset(buf, 0, sizeof(int16_t) * len);
	if (_playing) {
		const int samplesPerTick = _mix->getSampleRate() / 50;
		while (len != 0) {
			if (_samplesLeft == 0) {
				handleTick();
				_samplesLeft = samplesPerTick;
			}
			int count = _samplesLeft;
			if (count > len) {
				count = len;
			}
			_samplesLeft -= count;
			len -= count;
			mixSamples(buf, count);
			if (kLowPassFilter) {
				butterworth(buf, count);
			}
			buf += count;
		}
	}
	return _playing;
}

bool SfxPlayer::mixCallback(void *param, int16_t *samples, int len) {
	return ((SfxPlayer *)param)->mix(samples, len);
}
