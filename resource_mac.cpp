
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "fs.h"
#include "resource_mac.h"
#include "util.h"

const char *ResourceMac::FILENAME1 = "Flashback.bin";
const char *ResourceMac::FILENAME2 = "Flashback.rsrc";

ResourceMac::ResourceMac(const char *filePath, FileSystem *fs)
	: _dataOffset(0), _types(0), _entries(0) {
	memset(&_map, 0, sizeof(_map));
	_f.open(filePath, "rb", fs);
}

ResourceMac::~ResourceMac() {
	if (_entries) {
		for (int i = 0; i < _map.typesCount; ++i) {
			free(_entries[i]);
		}
		free(_entries);
	}
	free(_types);
}

void ResourceMac::load() {
	const uint32_t sig = _f.readUint32BE();
	if (sig == 0x00051607) { // AppleDouble
		debug(DBG_INFO, "Load Macintosh data from AppleDouble");
		_f.seek(24);
		const int count = _f.readUint16BE();
		for (int i = 0; i < count; ++i) {
			const int id = _f.readUint32BE();
			const int offset = _f.readUint32BE();
			const int length = _f.readUint32BE();
			if (id == 2) { // resource fork
				loadResourceFork(offset, length);
				break;
			}
		}
	} else { // MacBinary
		debug(DBG_INFO, "Load Macintosh data from MacBinary");
		_f.seek(83);
		uint32_t dataSize = _f.readUint32BE();
		uint32_t resourceOffset = 128 + ((dataSize + 127) & ~127);
		loadResourceFork(resourceOffset, dataSize);
	}
}

void ResourceMac::loadResourceFork(uint32_t resourceOffset, uint32_t dataSize) {
	_f.seek(resourceOffset);
	_dataOffset = resourceOffset + _f.readUint32BE();
	uint32_t mapOffset = resourceOffset + _f.readUint32BE();

	_f.seek(mapOffset + 22);
	_f.readUint16BE();
	_map.typesOffset = _f.readUint16BE();
	_map.namesOffset = _f.readUint16BE();
	_map.typesCount = _f.readUint16BE() + 1;

	_f.seek(mapOffset + _map.typesOffset + 2);
	_types = (ResourceMacType *)calloc(_map.typesCount, sizeof(ResourceMacType));
	for (int i = 0; i < _map.typesCount; ++i) {
		_f.read(_types[i].id, 4);
		_types[i].count = _f.readUint16BE() + 1;
		_types[i].startOffset = _f.readUint16BE();
	}
	_entries = (ResourceMacEntry **)calloc(_map.typesCount, sizeof(ResourceMacEntry *));
	for (int i = 0; i < _map.typesCount; ++i) {
		_f.seek(mapOffset + _map.typesOffset + _types[i].startOffset);
		_entries[i] = (ResourceMacEntry *)calloc(_types[i].count, sizeof(ResourceMacEntry));
		for (int j = 0; j < _types[i].count; ++j) {
			_entries[i][j].id = _f.readUint16BE();
			_entries[i][j].nameOffset = _f.readUint16BE();
			_entries[i][j].dataOffset = _f.readUint32BE() & 0x00FFFFFF;
			_f.readUint32BE();
		}
		for (int j = 0; j < _types[i].count; ++j) {
			_entries[i][j].name[0] = '\0';
			if (_entries[i][j].nameOffset != 0xFFFF) {
				_f.seek(mapOffset + _map.namesOffset + _entries[i][j].nameOffset);
				const int len = _f.readByte();
				assert(len < kResourceMacEntryNameLength - 1);
				_f.read(_entries[i][j].name, len);
				_entries[i][j].name[len] = '\0';
			}
		}
	}
}

const ResourceMacEntry *ResourceMac::findEntry(const char *name) const {
	for (int type = 0; type < _map.typesCount; ++type) {
		for (int i = 0; i < _types[type].count; ++i) {
			if (strcmp(name, _entries[type][i].name) == 0) {
				return &_entries[type][i];
			}
		}
	}
	return 0;
}

