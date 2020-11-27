
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "file.h"
#include "mixer.h"
#include "mod_player.h"
#include "util.h"

#ifdef USE_MODPLUG
#include <libmodplug/modplug.h>

struct ModPlayer_impl {

	ModPlugFile *_mf;
	ModPlug_Settings _settings;
	bool _repeatIntro;

	ModPlayer_impl()
		: _mf(0) {
	}

	void init(const int rate) {
		memset(&_settings, 0, sizeof(_settings));
		ModPlug_GetSettings(&_settings);
		_settings.mFlags = MODPLUG_ENABLE_OVERSAMPLING | MODPLUG_ENABLE_NOISE_REDUCTION;
		_settings.mChannels = 1;
		_settings.mBits = 16;
		_settings.mFrequency = rate;
		_settings.mResamplingMode = MODPLUG_RESAMPLE_FIR;
		_settings.mLoopCount = -1;
		ModPlug_SetSettings(&_settings);
	}

	bool load(File *f) {
		const uint32_t size = f->size();
		uint8_t *data = (uint8_t *)malloc(size);
		if (data) {
			f->read(data, size);
			_mf = ModPlug_Load(data, size);
		}
		return _mf != 0;
	}

	void unload() {
		if (_mf) {
			ModPlug_Unload(_mf);
			_mf = 0;
		}
	}

	bool mix(int16_t *buf, int len) {
		if (_mf) {
			const int order = ModPlug_GetCurrentOrder(_mf);
			if (order == 3 && _repeatIntro) {
				ModPlug_SeekOrder(_mf, 1);
				_repeatIntro = false;
			}
			const int count = ModPlug_Read(_mf, buf, len * sizeof(int16_t));
			// setting mLoopCount to non-zero does not trigger any looping in
			// my test and ModPlug_Read returns 0.
			// looking at the libmodplug-0.8.8 tarball, it seems the variable
			// m_nRepeatCount is commented in sndmix.cpp. Not sure how if this
			// is a known bug, we workaround it here.
			if (count == 0) {
				ModPlug_SeekOrder(_mf, 0);
			}
			return true;
		}
		return false;
	}
};

#else

struct ModPlayer_impl {
	enum {
		NUM_SAMPLES = 31,
		NUM_TRACKS = 4,
		NUM_PATTERNS = 128,
		FRAC_BITS = 12,
		PAULA_FREQ = 3546897
	};

	struct SampleInfo {
		char name[23];
		uint16_t len;
		uint8_t fineTune;
		uint8_t volume;
		uint16_t repeatPos;
		uint16_t repeatLen;
		int8_t *data;

		int8_t getPCM(int offset) const {
			if (offset < 0) {
				offset = 0;
			} else if (offset >= (int)len) {
				offset = len - 1;
			}
			return data[offset];
		}
	};

	struct ModuleInfo {
		char songName[21];
		SampleInfo samples[NUM_SAMPLES];
		uint8_t numPatterns;
		uint8_t patternOrderTable[NUM_PATTERNS];
		uint8_t *patternsTable;
	};

	struct Track {
		SampleInfo *sample;
		uint8_t volume;
		int pos;
		int freq;
		uint16_t period;
		uint16_t periodIndex;
		uint16_t effectData;
		int vibratoSpeed;
		int vibratoAmp;
		int vibratoPos;
		int portamento;
		int portamentoSpeed;
		int retriggerCounter;
		int delayCounter;
		int cutCounter;
	};

	bool _playing;
	int _mixingRate;
	ModuleInfo _modInfo;
	uint8_t _currentPatternOrder;
	uint8_t _currentPatternPos;
	uint8_t _currentTick;
	uint8_t _songSpeed;
	uint8_t _songTempo;
	int _patternDelay;
	int _patternLoopPos;
	int _patternLoopCount;
	int _samplesLeft;
	bool _repeatIntro;
	Track _tracks[NUM_TRACKS];

	ModPlayer_impl();

	void init(const int rate);
	uint16_t findPeriod(uint16_t period, uint8_t fineTune) const;
	bool load(File *f);
	void unload();
	void handleNote(int trackNum, uint32_t noteData);
	void handleTick();
	void applyVolumeSlide(int trackNum, int amount);
	void applyVibrato(int trackNum);
	void applyPortamento(int trackNum);
	void handleEffect(int trackNum, bool tick);
	void mixSamples(int16_t *buf, int len);
	bool mix(int16_t *buf, int len);
};

ModPlayer_impl::ModPlayer_impl()
	: _playing(false) {
	memset(&_modInfo, 0, sizeof(_modInfo));
}

uint16_t ModPlayer_impl::findPeriod(uint16_t period, uint8_t fineTune) const {
	for (int p = 0; p < 36; ++p) {
		if (ModPlayer::_periodTable[p] == period) {
			return fineTune * 36 + p;
		}
	}
	error("Invalid period=%d", period);
	return 0;
}

void ModPlayer_impl::init(const int rate) {
	_mixingRate = rate;
}

bool ModPlayer_impl::load(File *f) {
	f->read(_modInfo.songName, 20);
	_modInfo.songName[20] = 0;
	debug(DBG_MOD, "songName = '%s'", _modInfo.songName);

	for (int s = 0; s < NUM_SAMPLES; ++s) {
		SampleInfo *si = &_modInfo.samples[s];
		f->read(si->name, 22);
		si->name[22] = 0;
		si->len = f->readUint16BE() * 2;
		si->fineTune = f->readByte();
		si->volume = f->readByte();
		si->repeatPos = f->readUint16BE() * 2;
		si->repeatLen = f->readUint16BE() * 2;
		si->data = 0;
		assert(si->len == 0 || si->repeatPos + si->repeatLen <= si->len);
		debug(DBG_MOD, "sample=%d name='%s' len=%d vol=%d", s, si->name, si->len, si->volume);
	}
	_modInfo.numPatterns = f->readByte();
	assert(_modInfo.numPatterns < NUM_PATTERNS);
	f->readByte(); // 0x7F
	f->read(_modInfo.patternOrderTable, NUM_PATTERNS);
	f->readUint32BE(); // 'M.K.', Protracker, 4 channels

	uint8_t n = 0;
	for (int i = 0; i < NUM_PATTERNS; ++i) {
		if (_modInfo.patternOrderTable[i] != 0) {
			n = MAX(n, _modInfo.patternOrderTable[i]);
		}
	}
	debug(DBG_MOD, "numPatterns=%d",n + 1);
	const int patternsSize = (n + 1) * 64 * 4 * 4; // 64 lines of 4 notes per channel
	_modInfo.patternsTable = (uint8_t *)malloc(patternsSize);
	if (!_modInfo.patternsTable) {
		warning("Unable to allocate %d bytes for .MOD patterns table", patternsSize);
		return false;
	}
	f->read(_modInfo.patternsTable, patternsSize);

	for (int s = 0; s < NUM_SAMPLES; ++s) {
		SampleInfo *si = &_modInfo.samples[s];
		if (si->len != 0) {
			si->data = (int8_t *)malloc(si->len);
			if (si->data) {
				f->read(si->data, si->len);
			}
		}
	}

	_currentPatternOrder = 0;
	_currentPatternPos = 0;
	_currentTick = 0;
	_patternDelay = 0;
	_songSpeed = 6;
	_songTempo = 125;
	_patternLoopPos = 0;
	_patternLoopCount = -1;
	_samplesLeft = 0;
	_repeatIntro = false;
	memset(_tracks, 0, sizeof(_tracks));
	_playing = true;

	return true;
}

void ModPlayer_impl::unload() {
	if (_modInfo.songName[0]) {
		free(_modInfo.patternsTable);
		for (int s = 0; s < NUM_SAMPLES; ++s) {
			free(_modInfo.samples[s].data);
		}
		memset(&_modInfo, 0, sizeof(_modInfo));
	}
	_playing = false;
}

void ModPlayer_impl::handleNote(int trackNum, uint32_t noteData) {
	Track *tk = &_tracks[trackNum];
	uint16_t sampleNum = ((noteData >> 24) & 0xF0) | ((noteData >> 12) & 0xF);
	uint16_t samplePeriod = (noteData >> 16) & 0xFFF;
	uint16_t effectData = noteData & 0xFFF;
	debug(DBG_MOD, "ModPlayer::handleNote(%d) p=%d/%d sampleNumber=0x%X samplePeriod=0x%X effectData=0x%X tk->period=%d", trackNum, _currentPatternPos, _currentPatternOrder, sampleNum, samplePeriod, effectData, tk->period);
	if (sampleNum != 0) {
		tk->sample = &_modInfo.samples[sampleNum - 1];
		tk->volume = tk->sample->volume;
		tk->pos = 0;
	}
	if (samplePeriod != 0) {
		tk->periodIndex = findPeriod(samplePeriod, tk->sample->fineTune);
		if ((effectData >> 8) != 0x3 && (effectData >> 8) != 0x5) {
			tk->period = ModPlayer::_periodTable[tk->periodIndex];
			tk->freq = PAULA_FREQ / tk->period;
		} else {
			tk->portamento = ModPlayer::_periodTable[tk->periodIndex];
		}
		tk->vibratoAmp = 0;
		tk->vibratoSpeed = 0;
		tk->vibratoPos = 0;
	}
	tk->effectData = effectData;
}

void ModPlayer_impl::applyVolumeSlide(int trackNum, int amount) {
	debug(DBG_MOD, "ModPlayer::applyVolumeSlide(%d, %d)", trackNum, amount);
	Track *tk = &_tracks[trackNum];
	int vol = tk->volume + amount;
	if (vol < 0) {
		vol = 0;
	} else if (vol > 64) {
		vol = 64;
	}
	tk->volume = vol;
}

void ModPlayer_impl::applyVibrato(int trackNum) {
	static const int8_t sineWaveTable[] = {
	   0,   24,   49,   74,   97,  120, -115,  -95,  -76,  -59,  -44,  -32,  -21,  -12,   -6,   -3,
	  -1,   -3,   -6,  -12,  -21,  -32,  -44,  -59,  -76,  -95, -115,  120,   97,   74,   49,   24,
	   0,  -24,  -49,  -74,  -97, -120,  115,   95,   76,   59,   44,   32,   21,   12,    6,    3,
	   1,    3,    6,   12,   21,   32,   44,   59,   76,   95,  115, -120,  -97,  -74,  -49,  -24
	};
	debug(DBG_MOD, "ModPlayer::applyVibrato(%d)", trackNum);
	Track *tk = &_tracks[trackNum];
	int vib = tk->vibratoAmp * sineWaveTable[tk->vibratoPos] / 128;
	if (tk->period + vib != 0) {
		tk->freq = PAULA_FREQ / (tk->period + vib);
	}
	tk->vibratoPos += tk->vibratoSpeed;
	if (tk->vibratoPos >= 64) {
		tk->vibratoPos = 0;
	}
}

void ModPlayer_impl::applyPortamento(int trackNum) {
	debug(DBG_MOD, "ModPlayer::applyPortamento(%d)", trackNum);
	Track *tk = &_tracks[trackNum];
	if (tk->period < tk->portamento) {
		tk->period = MIN(tk->period + tk->portamentoSpeed, tk->portamento);
	} else if (tk->period > tk->portamento) {
		tk->period = MAX(tk->period - tk->portamentoSpeed, tk->portamento);
	}
	if (tk->period != 0) {
		tk->freq = PAULA_FREQ / tk->period;
	}
}

void ModPlayer_impl::handleEffect(int trackNum, bool tick) {
	Track *tk = &_tracks[trackNum];
	uint8_t effectNum = tk->effectData >> 8;
	uint8_t effectXY = tk->effectData & 0xFF;
	uint8_t effectX = effectXY >> 4;
	uint8_t effectY = effectXY & 0xF;
	debug(DBG_MOD, "ModPlayer::handleEffect(%d) effectNum=0x%X effectXY=0x%X", trackNum, effectNum, effectXY);
	switch (effectNum) {
	case 0x0: // arpeggio
		if (tick && effectXY != 0) {
			uint16_t period = tk->period;
			switch (_currentTick & 3) {
			case 1:
				period = ModPlayer::_periodTable[tk->periodIndex + effectX];
				break;
			case 2:
				period = ModPlayer::_periodTable[tk->periodIndex + effectY];
				break;
			}
			tk->freq = PAULA_FREQ / period;
		}
		break;
	case 0x1: // portamento up
		if (tick) {
			tk->period -= effectXY;
			if (tk->period < 113) { // note B-3
				tk->period = 113;
			}
			tk->freq = PAULA_FREQ / tk->period;
		}
		break;
	case 0x2: // portamento down
		if (tick) {
			tk->period += effectXY;
			if (tk->period > 856) { // note C-1
				tk->period = 856;
			}
			tk->freq = PAULA_FREQ / tk->period;
		}
		break;
	case 0x3: // tone portamento
		if (!tick) {
        	if (effectXY != 0) {
        		tk->portamentoSpeed = effectXY;
        	}
		} else {
			applyPortamento(trackNum);
		}
		break;
	case 0x4: // vibrato
		if (!tick) {
			if (effectX != 0) {
				tk->vibratoSpeed = effectX;
			}
			if (effectY != 0) {
				tk->vibratoAmp = effectY;
			}
		} else {
			applyVibrato(trackNum);
		}
		break;
	case 0x5: // tone portamento + volume slide
		if (tick) {
			applyPortamento(trackNum);
			applyVolumeSlide(trackNum, effectX - effectY);
		}
		break;
	case 0x6: // vibrato + volume slide
		if (tick) {
			applyVibrato(trackNum);
			applyVolumeSlide(trackNum, effectX - effectY);
		}
		break;
	case 0x9: // set sample offset
		if (!tick) {
			tk->pos = effectXY << (8 + FRAC_BITS);
		}
		break;
	case 0xA: // volume slide
		if (tick) {
			applyVolumeSlide(trackNum, effectX - effectY);
		}
		break;
	case 0xB: // position jump
		if (!tick) {
			_currentPatternOrder = effectXY;
			_currentPatternPos = 0;
			assert(_currentPatternOrder < _modInfo.numPatterns);
		}
		break;
	case 0xC: // set volume
		if (!tick) {
			assert(effectXY <= 64);
			tk->volume = effectXY;
		}
		break;
	case 0xD: // pattern break
		if (!tick) {
			_currentPatternPos = effectX * 10 + effectY;
			assert(_currentPatternPos < 64);
			++_currentPatternOrder;
			debug(DBG_MOD, "_currentPatternPos = %d _currentPatternOrder = %d", _currentPatternPos, _currentPatternOrder);
		}
		break;
	case 0xE: // extended effects
		switch (effectX) {
		case 0x0: // set filter, ignored
			break;
		case 0x1: // fineslide up
			if (!tick) {
				tk->period -= effectY;
				if (tk->period < 113) { // B-3 note
					tk->period = 113;
				}
				tk->freq = PAULA_FREQ / tk->period;
			}
			break;
		case 0x2: // fineslide down
			if (!tick) {
				tk->period += effectY;
				if (tk->period > 856) { // C-1 note
					tk->period = 856;
				}
				tk->freq = PAULA_FREQ / tk->period;
			}
			break;
		case 0x6: // loop pattern
			if (!tick) {
				if (effectY == 0) {
					_patternLoopPos = _currentPatternPos | (_currentPatternOrder << 8);
					debug(DBG_MOD, "_patternLoopPos=%d/%d", _currentPatternPos, _currentPatternOrder);
				} else {
					if (_patternLoopCount == -1) {
						_patternLoopCount = effectY;
						_currentPatternPos = _patternLoopPos & 0xFF;
						_currentPatternOrder = _patternLoopPos >> 8;
					} else {
						--_patternLoopCount;
						if (_patternLoopCount != 0) {
							_currentPatternPos = _patternLoopPos & 0xFF;
							_currentPatternOrder = _patternLoopPos >> 8;
						} else {
							_patternLoopCount = -1;
						}
					}
					debug(DBG_MOD, "_patternLoopCount=%d", _patternLoopCount);
				}
			}
			break;
		case 0x9: // retrigger sample
			if (tick) {
				tk->retriggerCounter = effectY;
			} else {
				if (tk->retriggerCounter == 0) {
					tk->pos = 0;
					tk->retriggerCounter = effectY;
					debug(DBG_MOD, "retrigger sample=%d _songSpeed=%d", effectY, _songSpeed);
				}
				--tk->retriggerCounter;
			}
			break;
		case 0xA: // fine volume slide up
			if (!tick) {
				applyVolumeSlide(trackNum, effectY);
			}
			break;
		case 0xB: // fine volume slide down
			if (!tick) {
				applyVolumeSlide(trackNum, -effectY);
			}
			break;
		case 0xC: // cut sample
			if (!tick) {
				tk->cutCounter = effectY;
			} else {
				--tk->cutCounter;
				if (tk->cutCounter == 0) {
					tk->volume = 0;
				}
			}
		case 0xD: // delay sample
			if (!tick) {
				tk->delayCounter = effectY;
			} else {
				if (tk->delayCounter != 0) {
					--tk->delayCounter;
				}
			}
			break;
		case 0xE: // delay pattern
			if (!tick) {
				debug(DBG_MOD, "ModPlayer::handleEffect() _currentTick=%d delay pattern=%d", _currentTick, effectY);
				_patternDelay = effectY;
			}
			break;
		default:
			warning("Unhandled extended effect 0x%X params=0x%X", effectX, effectY);
			break;
		}
		break;
	case 0xF: // set speed
		if (!tick) {
			if (effectXY < 0x20) {
				_songSpeed = effectXY;
			} else {
				_songTempo = effectXY;
			}
		}
		break;
	default:
		warning("Unhandled effect 0x%X params=0x%X", effectNum, effectXY);
		break;
	}
}

void ModPlayer_impl::handleTick() {
	if (!_playing) {
		return;
	}
//	if (_patternDelay != 0) {
//		--_patternDelay;
//		return;
//	}
	if (_currentTick == 0) {
		debug(DBG_MOD, "_currentPatternOrder=%d _currentPatternPos=%d", _currentPatternOrder, _currentPatternPos);
		uint8_t currentPattern = _modInfo.patternOrderTable[_currentPatternOrder];
		const uint8_t *p = _modInfo.patternsTable + (currentPattern * 64 + _currentPatternPos) * 16;
		for (int i = 0; i < NUM_TRACKS; ++i) {
			uint32_t noteData = READ_BE_UINT32(p);
			handleNote(i, noteData);
			p += 4;
		}
		++_currentPatternPos;
		if (_currentPatternPos == 64) {
			++_currentPatternOrder;
			_currentPatternPos = 0;
			debug(DBG_MOD, "ModPlayer::handleTick() _currentPatternOrder = %d/%d", _currentPatternOrder, _modInfo.numPatterns);
			// On the amiga version, the introduction cutscene is shorter than the PC version ;
			// so the music module doesn't synchronize at all with the PC datafiles, here we
			// add a hack to let the music play longer
			if (_currentPatternOrder == 3 && _repeatIntro) {
				_currentPatternOrder = 1;
				_repeatIntro = false;
//				warning("Introduction module synchronization hack");
			}
		}
	}
	for (int i = 0; i < NUM_TRACKS; ++i) {
		handleEffect(i, (_currentTick != 0));
	}
	++_currentTick;
	if (_currentTick == _songSpeed) {
		_currentTick = 0;
	}
	if (_currentPatternOrder == _modInfo.numPatterns) {
		debug(DBG_MOD, "ModPlayer::handleEffect() _currentPatternOrder == _modInfo.numPatterns");
//		_playing = false;
		_currentPatternOrder = 0;
	}
}

void ModPlayer_impl::mixSamples(int16_t *buf, int samplesLen) {
	for (int i = 0; i < NUM_TRACKS; ++i) {
		Track *tk = &_tracks[i];
		if (tk->sample != 0 && tk->delayCounter == 0) {
			int16_t *mixbuf = buf;
			SampleInfo *si = tk->sample;
			int len = si->len << FRAC_BITS;
			int loopLen = si->repeatLen << FRAC_BITS;
			int loopPos = si->repeatPos << FRAC_BITS;
			int deltaPos = (tk->freq << FRAC_BITS) / _mixingRate;
			int curLen = samplesLen;
			int pos = tk->pos;
			while (curLen != 0) {
				int count;
				if (loopLen > (2 << FRAC_BITS)) {
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
					const int out = si->getPCM(pos >> FRAC_BITS) * tk->volume / 64;
					*mixbuf = ADDC_S16(*mixbuf, S8_to_S16(out));
					++mixbuf;
					pos += deltaPos;
				}
			}
			tk->pos = pos;
		}
	}
}

bool ModPlayer_impl::mix(int16_t *buf, int len) {
	memset(buf, 0, sizeof(int16_t) * len);
	if (_playing) {
		const int samplesPerTick = _mixingRate / (50 * _songTempo / 125);
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
			buf += count;
		}
	}
	return _playing;
}
#endif

ModPlayer::ModPlayer(Mixer *mixer, FileSystem *fs)
	: _playing(false), _mix(mixer), _fs(fs) {
	_impl = new ModPlayer_impl;
}

ModPlayer::~ModPlayer() {
	delete _impl;
}

void ModPlayer::play(int num) {
	if (num < _modulesFilesCount) {
		File f;
		for (uint8_t i = 0; i < ARRAYSIZE(_modulesFiles[num]); ++i) {
			if (f.open(_modulesFiles[num][i], "rb", _fs)) {
				_impl->init(_mix->getSampleRate());
				if (_impl->load(&f)) {
					_impl->_repeatIntro = (num == 0) && !_isAmiga;
					_mix->setPremixHook(mixCallback, _impl);
					_playing = true;
				}
				return;
			}
		}
	}
}

void ModPlayer::stop() {
	if (_playing) {
		_mix->setPremixHook(0, 0);
		_impl->unload();
		_playing = false;
	}
}

bool ModPlayer::mixCallback(void *param, int16_t *buf, int len) {
	return ((ModPlayer_impl *)param)->mix(buf, len);
}
