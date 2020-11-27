
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef FILE_H__
#define FILE_H__

#include "intern.h"

struct File_impl;
struct FileSystem;

struct File {
	File();
	~File();

	File_impl *_impl;

	bool open(const char *filename, const char *mode, FileSystem *fs);
	bool open(const char *filename, const char *mode, const char *directory);
	void openMemoryBuffer(int initialCapacity);
	void close();
	bool ioErr() const;
	uint32_t size();
	void seek(int32_t off);
	uint32_t read(void *ptr, uint32_t len);
	uint8_t readByte();
	uint16_t readUint16LE();
	uint32_t readUint32LE();
	uint16_t readUint16BE();
	uint32_t readUint32BE();
	uint32_t write(const void *ptr, uint32_t size);
	void writeByte(uint8_t b);
	void writeUint16LE(uint16_t n);
	void writeUint32LE(uint32_t n);
	void writeUint16BE(uint16_t n);
	void writeUint32BE(uint32_t n);
};

void dumpFile(const char *filename, const uint8_t *p, int size);

#endif // FILE_H__
