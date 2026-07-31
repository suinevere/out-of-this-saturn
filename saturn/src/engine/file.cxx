/* Raw - Another World Interpreter
 * Copyright (C) 2004 Gregory Montoir
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef USE_ZLIB
#include "zlib.h"
#endif
#include "file.h"
#ifdef OOTW_SATURN
#include "disc.h"
#endif

struct File_impl {
	bool _ioErr;
	File_impl() : _ioErr(false) {}
  virtual ~File_impl() {}
	virtual bool open(const char *path, const char *mode) = 0;
	virtual void close() = 0;
	virtual void seek(int32_t off) = 0;
	virtual void read(void *ptr, uint32_t size) = 0;
	virtual void write(void *ptr, uint32_t size) = 0;
	virtual uint32_t tell() { return 0; }
};

struct memFile : File_impl {
	uint8_t *_buf;
	uint32_t _size;
	uint32_t _pos;
	bool _write;

	memFile(uint8_t *buf, uint32_t size, bool write)
		: _buf(buf), _size(size), _pos(0), _write(write) {}

	bool open(const char *path, const char *mode) {
		(void)path;
		(void)mode;
		return true;
	}
	void close() {}
	void seek(int32_t off) {
		if (off >= 0 && (uint32_t)off <= _size) {
			_pos = (uint32_t)off;
		} else {
			_ioErr = true;
		}
	}
	void read(void *ptr, uint32_t size) {
		if (_pos + size > _size) {
			_ioErr = true;
			return;
		}
		memcpy(ptr, _buf + _pos, size);
		_pos += size;
	}
	void write(void *ptr, uint32_t size) {
		if (!_write || _pos + size > _size) {
			_ioErr = true;
			return;
		}
		memcpy(_buf + _pos, ptr, size);
		_pos += size;
	}
	uint32_t tell() { return _pos; }
};

struct stdFile : File_impl {
	FILE *_fp;
	stdFile() : _fp(0) {}
	bool open(const char *path, const char *mode) {
		_ioErr = false;
		_fp = fopen(path, mode);
		return (_fp != NULL);
	}
	void close() {
		if (_fp) {
			fclose(_fp);
			_fp = 0;
		}
	}
	void seek(int32_t off) {
		if (_fp) {
			fseek(_fp, off, SEEK_SET);
		}
	}
	void read(void *ptr, uint32_t size) {
		if (_fp) {
			uint32_t r = fread(ptr, 1, size, _fp);
			if (r != size) {
				_ioErr = true;
			}
		}
	}
	void write(void *ptr, uint32_t size) {
		if (_fp) {
			uint32_t r = fwrite(ptr, 1, size, _fp);
			if (r != size) {
				_ioErr = true;
			}
		}
	}
};

#ifdef OOTW_SATURN
struct saturnFile : File_impl {
	DiscFile *_fp;
	int32_t _pos;
	saturnFile() : _fp(0), _pos(0) {}
	bool open(const char *path, const char *mode) {
		(void)mode;
		_ioErr = false;
		_pos = 0;
		_fp = disc_open(path);
		return (_fp != NULL);
	}
	void close() {
		if (_fp) {
			disc_close(_fp);
			_fp = 0;
		}
	}
	void seek(int32_t off) {
		_pos = off;
	}
	void read(void *ptr, uint32_t size) {
		if (_fp) {
			int32_t r = disc_read(_fp, _pos, ptr, (int32_t)size);
			if (r < 0 || (uint32_t)r != size) {
				_ioErr = true;
			}
			if (r > 0) {
				_pos += r;
			}
		}
	}
	void write(void *ptr, uint32_t size) {
		(void)ptr; (void)size;
		_ioErr = true;
	}
};
#endif

#ifdef USE_ZLIB
struct zlibFile : File_impl {
	gzFile _fp;
	zlibFile() : _fp(0) {}
	bool open(const char *path, const char *mode) {
		_ioErr = false;
		_fp = gzopen(path, mode);
		return (_fp != NULL);
	}
	void close() {
		if (_fp) {
			gzclose(_fp);
			_fp = 0;
		}
	}
	void seek(int32_t off) {
		if (_fp) {
			gzseek(_fp, off, SEEK_SET);
		}
	}
	void read(void *ptr, uint32_t size) {
		if (_fp) {
			uint32_t r = gzread(_fp, ptr, size);
			if (r != size) {
				_ioErr = true;
			}
		}
	}
	void write(void *ptr, uint32_t size) {
		if (_fp) {
			uint32_t r = gzwrite(_fp, ptr, size);
			if (r != size) {
				_ioErr = true;
			}
		}
	}
};
#endif

File::File(bool gzipped) {
#if defined(OOTW_SATURN)
	(void)gzipped;
	_impl = new saturnFile;
#elif defined(USE_ZLIB)
	if (gzipped) {
		_impl = new zlibFile;
	} else {
		_impl = new stdFile;
	}
#else
	(void)gzipped;
	_impl = new stdFile;
#endif
}

File::~File() {
	_impl->close();
	delete _impl;
}

bool File::open(const char *filename, const char *directory, const char *mode) {	
	_impl->close();
	char buf[512];
	sprintf(buf, "%s/%s", directory, filename);
	char *p = buf + strlen(directory) + 1;
	string_lower(p);
	bool opened = _impl->open(buf, mode);
	if (!opened) {
		string_upper(p);
		opened = _impl->open(buf, mode);
	}
	return opened;
}

bool File::openMemory(void *buf, uint32_t size, bool write) {
	if (buf == 0 || size == 0) {
		return false;
	}
	_impl->close();
	delete _impl;
	_impl = new memFile((uint8_t *)buf, size, write);
	return true;
}

void File::close() {
	_impl->close();
}

bool File::ioErr() const {
	return _impl->_ioErr;
}

void File::seek(int32_t off) {
	_impl->seek(off);
}

uint32_t File::tell() {
	return _impl->tell();
}

void File::read(void *ptr, uint32_t size) {
	_impl->read(ptr, size);
}

uint8_t File::readByte() {
	uint8_t b;
	read(&b, 1);
	return b;
}

uint16_t File::readUint16BE() {
	uint8_t hi = readByte();
	uint8_t lo = readByte();
	return (hi << 8) | lo;
}

uint32_t File::readUint32BE() {
	uint16_t hi = readUint16BE();
	uint16_t lo = readUint16BE();
	return (hi << 16) | lo;
}

void File::write(void *ptr, uint32_t size) {
	_impl->write(ptr, size);
}

void File::writeByte(uint8_t b) {
	write(&b, 1);
}

void File::writeUint16BE(uint16_t n) {
	writeByte(n >> 8);
	writeByte(n & 0xFF);
}

void File::writeUint32BE(uint32_t n) {
	writeUint16BE(n >> 16);
	writeUint16BE(n & 0xFFFF);
}
