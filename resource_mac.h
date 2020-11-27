
#ifndef RESOURCE_MAC_H__
#define RESOURCE_MAC_H__

#include <stdint.h>
#include "file.h"

struct ResourceMacMap {
	uint16_t typesOffset;
	uint16_t namesOffset;
	uint16_t typesCount;
};

struct ResourceMacType {
	unsigned char id[4];
	uint16_t count;
	uint16_t startOffset;
};

enum {
	kResourceMacEntryNameLength = 64
};

struct ResourceMacEntry {
	uint16_t id;
	uint16_t nameOffset;
	uint32_t dataOffset;
	char name[kResourceMacEntryNameLength];
};

struct ResourceMac {

	static const char *FILENAME1;
	static const char *FILENAME2;

	File _f;

	uint32_t _dataOffset;
	ResourceMacMap _map;
	ResourceMacType *_types;
	ResourceMacEntry **_entries;

	ResourceMac(const char *filePath, FileSystem *);
	~ResourceMac();

	bool isOpen() const { return _entries != 0; }
	void load();
	void loadResourceFork(uint32_t offset, uint32_t size);
	const ResourceMacEntry *findEntry(const char *name) const;
};

#endif

