
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "file.h"
#include "fs.h"
#include "mixer.h"
#include "seq_player.h"
#include "systemstub.h"
#include "util.h"

bool SeqDemuxer::open(File *f) {
	_f = f;
	_fileSize = _f->size();
	memset(_buffers, 0, sizeof(_buffers));
	_frameOffset = 0;
	return readHeader();
}

void SeqDemuxer::close() {
	_f = 0;
	for (int i = 0; i < kBuffersCount; ++i) {
		free(_buffers[i].data);
	}
	memset(_buffers, 0, sizeof(_buffers));
}

bool SeqDemuxer::readHeader() {
	for (int i = 0; i < 256; i += 4) {
		if (_f->readUint32LE() != 0) {
			return false;
		}
	}
	for (int i = 0; i < kBuffersCount; ++i) {
		const int size = _f->readUint16LE();
		if (size != 0) {
			_buffers[i].size = 0;
			_buffers[i].avail = size;
			_buffers[i].data = (uint8_t *)malloc(size);
			if (!_buffers[i].data) {
				error("Unable to allocate %d bytes for SEQ buffer %d", size, i);
			}
		}
	}
	for (int i = 1; i <= 100; ++i) {
		readFrameData();
	}
	return true;
}

bool SeqDemuxer::readFrameData() {
	_frameOffset += kFrameSize;
	if (_frameOffset >= _fileSize) {
		return false;
	}
	_f->seek(_frameOffset);
	_audioDataOffset = _f->readUint16LE();
	_audioDataSize = (_audioDataOffset != 0) ? kAudioBufferSize * 2 : 0;
	_paletteDataOffset = _f->readUint16LE();
	_paletteDataSize = (_paletteDataOffset != 0) ? 768 : 0;
	uint8_t num[4];
	for (int i = 0; i < 4; ++i) {
		num[i] = _f->readByte();
	}
	uint16_t offsets[4];
	for (int i = 0; i < 4; ++i) {
		offsets[i] = _f->readUint16LE();
	}
	for (int i = 0; i < 3; ++i) {
		if (offsets[i] != 0) {
			int e = i + 1;
			while (e < 3 && offsets[e] == 0) {
				++e;
			}
			fillBuffer(num[i + 1], offsets[i], offsets[e] - offsets[i]);
		}
	}
	if (num[0] != 255) {
		assert(num[0] < kBuffersCount);
		_videoData = num[0];
	} else {
		_videoData = -1;
	}
	return !_f->ioErr();
}

void SeqDemuxer::fillBuffer(int num, int offset, int size) {
	assert(num < kBuffersCount);
	_f->seek(_frameOffset + offset);
	assert(_buffers[num].size + size <= _buffers[num].avail);
	_f->read(_buffers[num].data + _buffers[num].size, size);
	_buffers[num].size += size;
}

void SeqDemuxer::clearBuffer(int num) {
	_buffers[num].size = 0;
}

void SeqDemuxer::readPalette(uint8_t *dst) {
	_f->seek(_frameOffset + _paletteDataOffset);
	_f->read(dst, 256 * 3);
}

void SeqDemuxer::readAudio(int16_t *dst) {
	_f->seek(_frameOffset + _audioDataOffset);
	for (int i = 0; i < kAudioBufferSize; ++i) {
		dst[i] = _f->readUint16BE();
	}
}

struct BitStream {
	BitStream(const uint8_t *src)
		: _src(src) {
		_bits = READ_LE_UINT16(_src); _src += 2;
		_len = 16;
	}
	int getBits(int count) {
		if (count > _len) {
			_bits |= READ_LE_UINT16(_src) << _len; _src += 2;
			_len += 16;
		}
		assert(count <= _len);
		const int x = _bits & ((1 << count) - 1);
		_bits >>= count;
		_len -= count;
		return x;
	}
	int getSignedBits(int count) {
		const int32_t x = getBits(count);
		return (x << (32 - count)) >> (32 - count);
	}

	const uint8_t *_src;
	int _len;
	uint32_t _bits;
};

static const uint8_t *decodeSeqOp1Helper(const uint8_t *src, uint8_t *dst, int dstSize) {
	int codes[64], count = 0;
	BitStream bs(src);
	for (int i = 0, sz = 0; i < 64 && sz < 64; ++i) {
		codes[i] = bs.getSignedBits(4);
		sz += ABS(codes[i]);
		count += 4;
	}
	src += (count + 7) / 8;
	for (int i = 0; i < 64 && dstSize > 0; ++i) {
		int len = codes[i];
		if (len < 0) {
			len = -len;
			memset(dst, *src++, MIN(len, dstSize));
		} else {
			memcpy(dst, src, MIN(len, dstSize));
			src += len;
		}
		dst += len;
		dstSize -= len;
	}
	return src;
}

static const uint8_t *decodeSeqOp1(uint8_t *dst, int pitch, const uint8_t *src) {
	const int len = *src++;
	if (len & 0x80) {
		uint8_t buf[8 * 8];
		switch (len & 3) {
		case 1:
			src = decodeSeqOp1Helper(src, buf, sizeof(buf));
			for (int y = 0; y < 8; ++y) {
				memcpy(dst, buf + y * 8, 8);
				dst += pitch;
			}
			break;
		case 2:
			src = decodeSeqOp1Helper(src, buf, sizeof(buf));
			for (int i = 0; i < 8; i++) {
				for (int y = 0; y < 8; ++y) {
					dst[y * pitch] = buf[i * 8 + y];
				}
				++dst;
			}
			break;
		}
	} else {
		static const uint8_t log2_16[] = { 0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3 };
		BitStream bs(src + len);
		assert(len <= 16);
		const int bits = log2_16[len - 1] + 1;
		for (int y = 0; y < 8; ++y) {
			for (int x = 0; x < 8; ++x) {
				dst[y * pitch + x] = src[bs.getBits(bits)];
			}
		}
		src += len + bits * 8;
	}
	return src;
}

static const uint8_t *decodeSeqOp2(uint8_t *dst, int pitch, const uint8_t *src) {
	for (int y = 0; y < 8; ++y) {
		memcpy(dst + y * pitch, src, 8);
		src += 8;
	}
	return src;
}

static const uint8_t *decodeSeqOp3(uint8_t *dst, int pitch, const uint8_t *src) {
	int pos;
	do {
		pos = *src++;
		const int offset = ((pos >> 3) & 7) * pitch + (pos & 7);
		dst[offset] = *src++;
	} while ((pos & 0x80) == 0);
	return src;
}

SeqPlayer::SeqPlayer(SystemStub *stub, Mixer *mixer)
	: _stub(stub), _buf(0), _mix(mixer) {
	_soundQueuePreloadSize = 0;
	_soundQueue = 0;
}

SeqPlayer::~SeqPlayer() {
}

void SeqPlayer::play(File *f) {
	if (_demux.open(f)) {
		uint8_t palette[256 * 3];
		_stub->getPalette(palette, 256);
		_mix->setPremixHook(mixCallback, this);
		memset(_buf, 0, 256 * 224);
		bool clearScreen = true;
		while (true) {
			const uint32_t nextFrameTimeStamp = _stub->getTimeStamp() + 1000 / 25;
			_stub->processEvents();
			if (_stub->_pi.quit || _stub->_pi.backspace) {
				_stub->_pi.backspace = false;
				break;
			}
			if (!_demux.readFrameData()) {
				break;
			}
			if (_demux._audioDataSize != 0) {
				SoundBufferQueue *sbq = (SoundBufferQueue *)malloc(sizeof(SoundBufferQueue));
				if (sbq) {
					sbq->data = (int16_t *)calloc(SeqDemuxer::kAudioBufferSize, sizeof(int16_t));
					if (sbq->data) {
						_demux.readAudio(sbq->data);
						sbq->size = SeqDemuxer::kAudioBufferSize;
						sbq->read = 0;
						sbq->next = 0;
					} else {
						free(sbq);
						sbq = 0;
					}
				}
				if (sbq) {
					LockAudioStack las(_stub);
					if (!_soundQueue) {
						_soundQueue = sbq;
					} else {
						SoundBufferQueue *p = _soundQueue;
						while (p->next) {
							p = p->next;
						}
						p->next = sbq;
					}
					if (_soundQueuePreloadSize < kSoundPreloadSize) {
						++_soundQueuePreloadSize;
					}
				}
			}
			if (_demux._paletteDataSize != 0) {
				uint8_t buf[256 * 3];
				_demux.readPalette(buf);
				for (int i = 0; i < 256 * 3; ++i) {
					buf[i] = (buf[i] << 2) | (buf[i] & 3);
				}
				_stub->setPalette(buf, 256);
			}
			if (_demux._videoData != -1) {
				const int y0 = (224 - kVideoHeight) / 2;
				const uint8_t *src = _demux._buffers[_demux._videoData].data;
				_demux.clearBuffer(_demux._videoData);
				BitStream bs(src); src += 128;
				for (int y = 0; y < kVideoHeight; y += 8) {
					for (int x = 0; x < kVideoWidth; x += 8) {
						const int offset = (y0 + y) * 256 + x;
						switch (bs.getBits(2)) {
						case 1:
							src = decodeSeqOp1(_buf + offset, 256, src);
							break;
						case 2:
							src = decodeSeqOp2(_buf + offset, 256, src);
							break;
						case 3:
							src = decodeSeqOp3(_buf + offset, 256, src);
							break;
						}
					}
				}
				if (clearScreen) {
					clearScreen = false;
					_stub->copyRect(0, 0, kVideoWidth, 224, _buf, 256);
				} else {
					_stub->copyRect(0, y0, kVideoWidth, kVideoHeight, _buf, 256);
				}
				_stub->updateScreen(0);
			}
			const int diff = nextFrameTimeStamp - _stub->getTimeStamp();
			if (diff > 0) {
				_stub->sleep(diff);
			}
		}
		// restore level palette
		_stub->setPalette(palette, 256);
		_mix->setPremixHook(0, 0);
		_demux.close();
		// flush sound queue
		LockAudioStack las(_stub);
		while (_soundQueue) {
			SoundBufferQueue *next = _soundQueue->next;
			free(_soundQueue->data);
			free(_soundQueue);
			_soundQueue = next;
		}
		_soundQueuePreloadSize = 0;
	}
}

bool SeqPlayer::mix(int16_t *buf, int samples) {
	if (_soundQueuePreloadSize < kSoundPreloadSize) {
		return true;
	}
	while (_soundQueue && samples > 0) {
		*buf++ = _soundQueue->data[_soundQueue->read];
		++_soundQueue->read;
		if (_soundQueue->read == _soundQueue->size) {
			SoundBufferQueue *next = _soundQueue->next;
			free(_soundQueue->data);
			free(_soundQueue);
			_soundQueue = next;
		}
		--samples;
	}
	return true;
}

bool SeqPlayer::mixCallback(void *param, int16_t *buf, int len) {
	return ((SeqPlayer *)param)->mix(buf, len);
}

