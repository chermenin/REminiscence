
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifdef USE_TREMOR
#include <tremor/ivorbisfile.h>
#endif
#include "file.h"
#include "mixer.h"
#include "ogg_player.h"
#include "util.h"

#ifdef USE_TREMOR
struct VorbisFile: File {
	uint32_t offset;

	static size_t readHelper(void *ptr, size_t size, size_t nmemb, void *datasource) {
		VorbisFile *vf = (VorbisFile *)datasource;
		if (size != 0 && nmemb != 0) {
			const int n = vf->read(ptr, size * nmemb);
			if (n > 0) {
				vf->offset += n;
				return n / size;
			}
		}
		return 0;
	}
	static int seekHelper(void *datasource, ogg_int64_t offset, int whence) {
		VorbisFile *vf = (VorbisFile *)datasource;
		switch (whence) {
		case SEEK_SET:
			vf->offset = offset;
			break;
		case SEEK_CUR:
			vf->offset += offset;
			break;
		case SEEK_END:
			vf->offset = vf->size() + offset;
			break;
		}
		vf->seek(vf->offset);
		return 0;
	}
	static int closeHelper(void *datasource) {
		VorbisFile *vf = (VorbisFile *)datasource;
		vf->close();
		delete vf;
		return 0;
	}
	static long tellHelper(void *datasource) {
		VorbisFile *vf = (VorbisFile *)datasource;
		return vf->offset;
	}
};

struct OggDecoder_impl {
	OggDecoder_impl()
		: _open(false), _readBuf(0), _readBufSize(0) {
	}
	~OggDecoder_impl() {
		free(_readBuf);
		_readBuf = 0;
		if (_open) {
			ov_clear(&_ovf);
		}
	}

	bool load(VorbisFile *f, int mixerSampleRate) {
		ov_callbacks ovcb;
		ovcb.read_func  = VorbisFile::readHelper;
		ovcb.seek_func  = VorbisFile::seekHelper;
		ovcb.close_func = VorbisFile::closeHelper;
		ovcb.tell_func  = VorbisFile::tellHelper;
		if (ov_open_callbacks(f, &_ovf, 0, 0, ovcb) < 0) {
			warning("Invalid .ogg file");
			return false;
		}
		_open = true;
		vorbis_info *vi = ov_info(&_ovf, -1);
		if ((vi->channels != 1 && vi->channels != 2) || vi->rate != mixerSampleRate) {
			warning("Unhandled ogg/pcm format ch %d rate %d", vi->channels, vi->rate);
			return false;
		}
		_channels = vi->channels;
		return true;
	}
	int read(int16_t *dst, int samples) {
		int size = samples * _channels * sizeof(int16_t);
		if (size > _readBufSize) {
			_readBufSize = size;
			free(_readBuf);
			_readBuf = (int16_t *)malloc(_readBufSize);
			if (!_readBuf) {
				return 0;
			}
		}
		int count = 0;
		while (size > 0) {
			const int len = ov_read(&_ovf, (char *)_readBuf, size, 0);
			if (len < 0) {
				// error in decoder
				return count;
			} else if (len == 0) {
				// loop
				ov_raw_seek(&_ovf, 0);
				continue;
			}
			assert((len & 1) == 0);
			switch (_channels) {
			case 2:
				assert((len & 3) == 0);
				for (int i = 0; i < len / 2; i += 2) {
					const int16_t s16 = (_readBuf[i] + _readBuf[i + 1]) / 2;
					*dst = ADDC_S16(*dst, s16);
					++dst;
				}
				break;
			case 1:
				for (int i = 0; i < len / 2; ++i) {
					*dst = ADDC_S16(*dst, _readBuf[i]);
					++dst;
				}
				break;
			}
			size -= len;
			count += len;
		}
		assert(size == 0);
		return count;
	}

	OggVorbis_File _ovf;
	int _channels;
	bool _open;
	int16_t *_readBuf;
	int _readBufSize;
};
#endif

OggPlayer::OggPlayer(Mixer *mixer, FileSystem *fs)
	: _mix(mixer), _fs(fs), _impl(0) {
}

OggPlayer::~OggPlayer() {
#ifdef USE_TREMOR
	delete _impl;
#endif
}

bool OggPlayer::playTrack(int num) {
#ifdef USE_TREMOR
	stopTrack();
	char buf[16];
	snprintf(buf, sizeof(buf), "track%02d.ogg", num);
	VorbisFile *vf = new VorbisFile;
	if (vf->open(buf, "rb", _fs)) {
		vf->offset = 0;
		_impl = new OggDecoder_impl();
		if (_impl->load(vf, _mix->getSampleRate())) {
			debug(DBG_INFO, "Playing '%s'", buf);
			_mix->setPremixHook(mixCallback, this);
			return true;
		}
	}
	delete vf;
#endif
	return false;
}

void OggPlayer::stopTrack() {
#ifdef USE_TREMOR
	_mix->setPremixHook(0, 0);
	delete _impl;
	_impl = 0;
#endif
}

void OggPlayer::pauseTrack() {
#ifdef USE_TREMOR
	if (_impl) {
		_mix->setPremixHook(0, 0);
	}
#endif
}

void OggPlayer::resumeTrack() {
#ifdef USE_TREMOR
	if (_impl) {
		_mix->setPremixHook(mixCallback, this);
	}
#endif
}

bool OggPlayer::mix(int16_t *buf, int len) {
#ifdef USE_TREMOR
	if (_impl) {
		return _impl->read(buf, len) != 0;
	}
#endif
	return false;
}

bool OggPlayer::mixCallback(void *param, int16_t *buf, int len) {
	return ((OggPlayer *)param)->mix(buf, len);
}

