
#include "resource_aba.h"
#include "unpack.h"
#include "util.h"

const char *ResourceAba::FILENAME = "DEMO_UK.ABA";

ResourceAba::ResourceAba(FileSystem *fs)
	: _fs(fs) {
	_entries = 0;
	_entriesCount = 0;
}

ResourceAba::~ResourceAba() {
	free(_entries);
}

static int compareAbaEntry(const void *a, const void *b) {
	return strcasecmp(((ResourceAbaEntry *)a)->name, ((ResourceAbaEntry *)b)->name);
}

void ResourceAba::readEntries() {
	if (_f.open(FILENAME, "rb", _fs)) {
		_entriesCount = _f.readUint16BE();
		_entries = (ResourceAbaEntry *)calloc(_entriesCount, sizeof(ResourceAbaEntry));
		if (!_entries) {
			error("Failed to allocate %d _entries", _entriesCount);
			return;
		}
		const int entrySize = _f.readUint16BE();
		assert(entrySize == 30);
		uint32_t nextOffset = 0;
		for (int i = 0; i < _entriesCount; ++i) {
			_f.read(_entries[i].name, sizeof(_entries[i].name));
			_entries[i].offset = _f.readUint32BE();
			_entries[i].compressedSize = _f.readUint32BE();
			_entries[i].size = _f.readUint32BE();
			const uint32_t tag = _f.readUint32BE();
			assert(tag == TAG);
			debug(DBG_RES, "'%s' offset 0x%X size %d/%d", _entries[i].name,  _entries[i].offset, _entries[i].compressedSize, _entries[i].size);
			if (i != 0) {
				assert(nextOffset == _entries[i].offset);
			}
			nextOffset = _entries[i].offset + _entries[i].compressedSize;
		}
		qsort(_entries, _entriesCount, sizeof(ResourceAbaEntry), compareAbaEntry);
	}
}

const ResourceAbaEntry *ResourceAba::findEntry(const char *name) const {
	ResourceAbaEntry tmp;
	strcpy(tmp.name, name);
	return (const ResourceAbaEntry *)bsearch(&tmp, _entries, _entriesCount, sizeof(ResourceAbaEntry), compareAbaEntry);
}

uint8_t *ResourceAba::loadEntry(const char *name, uint32_t *size) {
	uint8_t *dst = 0;
	const ResourceAbaEntry *e = findEntry(name);
	if (e) {
		if (size) {
			*size = e->size;
		}
		uint8_t *tmp = (uint8_t *)malloc(e->compressedSize);
		if (!tmp) {
			error("Failed to allocate %d bytes", e->compressedSize);
			return 0;
		}
		_f.seek(e->offset);
		_f.read(tmp, e->compressedSize);
		if (e->compressedSize == e->size) {
			dst = tmp;
		} else {
			dst = (uint8_t *)malloc(e->size);
			if (!dst) {
				error("Failed to allocate %d bytes", e->size);
				free(tmp);
				return 0;
			}
			const bool ret = bytekiller_unpack(dst, e->size, tmp, e->compressedSize);
			if (!ret) {
				error("Bad CRC for '%s'", name);
			}
			free(tmp);
		}
	}
	return dst;
}
