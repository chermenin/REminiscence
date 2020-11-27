
#include "cpc_player.h"
#include "mixer.h"
#include "util.h"

static const char *_tunes[] = {
	"Options.Cpc",
	"Jungle.Cpc",
	"Subway.Cpc",
	"TVshow.Cpc",
	"City.Cpc",
	"Alien.Cpc"
};

CpcPlayer::CpcPlayer(Mixer *mixer, FileSystem *fs)
	: _mix(mixer), _fs(fs) {
}

CpcPlayer::~CpcPlayer() {
}

bool CpcPlayer::playTrack(int num) {
	_compression[0] = 0;
	const int tuneNum = num - 2;
	if (tuneNum >= 0 && tuneNum < ARRAYSIZE(_tunes) && _f.open(_tunes[tuneNum], "rb", _fs)) {
		_pos = 0;
		_sampleL = _sampleR = 0;
		while (nextChunk()) {
			if (_compression[0]) {
				_restartPos = _nextPos;
				_mix->setPremixHook(mixCallback, this);
				return true;
			}
		}
	}
	return false;
}

void CpcPlayer::stopTrack() {
	_f.close();
}

void CpcPlayer::pauseTrack() {
	_mix->setPremixHook(0, 0);
}

void CpcPlayer::resumeTrack() {
	_mix->setPremixHook(mixCallback, this);
}

bool CpcPlayer::nextChunk() {
	bool found = false;
	while (!_f.ioErr() && !found) {
		const uint32_t pos = _pos;
		char tag[4];
		_f.read(tag, sizeof(tag));
		const uint32_t len = _f.readUint32BE();
		_nextPos = pos + len;
		if (memcmp(tag, "SNDS", 4) == 0) {
			_f.readUint32BE();
			_f.readUint32BE();
			char type[4];
			_f.read(type, sizeof(type));
			if (memcmp(type, "SHDR", 4) == 0) {
				uint8_t buf[24];
				_f.read(buf, sizeof(buf));
				const uint32_t rate = _f.readUint32BE();
				const uint32_t channels = _f.readUint32BE();
				if (channels != 2 || rate != _mix->getSampleRate()) {
					warning("Unsupported CPC tune channels %d rate %d", channels, rate);
					break;
				}
				_f.read(_compression, sizeof(_compression) - 1);
				_compression[sizeof(_compression) - 1] = 0;
				if (strcmp(_compression, "SDX2") != 0) {
					warning("Unsupported CPC compression '%s'", _compression);
					break;
				}
				found = true;
			} else if (memcmp(type, "SSMP", 4) == 0) {
				_samplesLeft = _f.readUint32BE();
				found = true;
				break;
			} else {
				warning("Unhandled SNDS chunk '%c%c%c%c'", type[0], type[1], type[2], type[3]);
			}
		} else if (memcmp(tag, "SHDR", 4) == 0 || memcmp(tag, "FILL", 4) == 0 || memcmp(tag, "CTRL", 4) == 0) {
			// ignore
		} else {
			warning("Unhandled chunk '%c%c%c%c' size %d", tag[0], tag[1], tag[2], tag[3], len);
		}
		_f.seek(_nextPos);
		_pos = _nextPos;
	}
	return found;
}

static int16_t decodeSDX2(int16_t prev, int8_t data) {
	const int sqr = data * ABS(data) * 2;
	return (data & 1) != 0 ? prev + sqr : sqr;
}

int8_t CpcPlayer::readSampleData() {
	if (_samplesLeft <= 0) {
		_f.seek(_nextPos);
		_pos = _nextPos;
		if (!nextChunk()) {
			// rewind
			_f.seek(_restartPos);
			nextChunk();
		}
	}
	const int8_t data = _f.readByte();
	--_samplesLeft;
	return data;
}

bool CpcPlayer::mix(int16_t *buf, int len) {
	for (int i = 0; i < len; ++i) {
		_sampleL = decodeSDX2(_sampleL, readSampleData());
		_sampleR = decodeSDX2(_sampleR, readSampleData());
		*buf++ = (_sampleL + _sampleR) / 2;
	}
	return true;
}

bool CpcPlayer::mixCallback(void *param, int16_t *buf, int len) {
	return ((CpcPlayer *)param)->mix(buf, len);
}
