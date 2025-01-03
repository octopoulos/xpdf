//========================================================================
//
// UnicodeMap.h
//
// Mapping from Unicode to an encoding.
//
// Copyright 2001-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "CharTypes.h"

#if MULTITHREADED
#	include "GMutex.h"
#endif

//------------------------------------------------------------------------

enum UnicodeMapKind
{
	unicodeMapUser,     // read from a file
	unicodeMapResident, // static list of ranges
	unicodeMapFunc      // function pointer
};

typedef int (*UnicodeMapFunc)(Unicode u, char* buf, int bufSize);

struct UnicodeMapRange
{
	Unicode  start  = 0; //
	Unicode  end    = 0; // range of Unicode chars
	uint32_t code   = 0; //
	uint32_t nBytes = 0; // first output code
};

struct UnicodeMapExt;

//------------------------------------------------------------------------

class UnicodeMap
{
public:
	// Create the UnicodeMap specified by <encodingName>.  Sets the
	// initial reference count to 1.  Returns nullptr on failure.
	static UnicodeMap* parse(const std::string& encodingNameA);

	// Create a resident UnicodeMap.
	UnicodeMap(const char* encodingNameA, bool unicodeOutA, UnicodeMapRange* rangesA, int lenA);

	// Create a resident UnicodeMap that uses a function instead of a
	// list of ranges.
	UnicodeMap(const char* encodingNameA, bool unicodeOutA, UnicodeMapFunc funcA);

	~UnicodeMap();

	void incRefCnt();
	void decRefCnt();

	std::string getEncodingName() { return encodingName; }

	bool isUnicode() { return unicodeOut; }

	// Return true if this UnicodeMap matches the specified
	// <encodingNameA>.
	bool match(const std::string& encodingNameA);

	// Map Unicode to the target encoding.  Fills in <buf> with the
	// output and returns the number of bytes used.  Output will be
	// truncated at <bufSize> bytes.  No string terminator is written.
	// Returns 0 if no mapping is found.
	int mapUnicode(Unicode u, char* buf, int bufSize);

private:
	UnicodeMap(const std::string& encodingNameA);

	std::string    encodingName = "";             //
	UnicodeMapKind kind         = unicodeMapUser; //
	bool           unicodeOut   = false;          //

	union
	{
		UnicodeMapRange* ranges; // (user, resident)
		UnicodeMapFunc   func;   // (func)
	};

	int            len      = 0;       // (user, resident)
	UnicodeMapExt* eMaps    = nullptr; // (user)
	int            eMapsLen = 0;       // (user)
	REFCNT_TYPE    refCnt   = 0;       //
};

//------------------------------------------------------------------------

#define unicodeMapCacheSize 4

class UnicodeMapCache
{
public:
	UnicodeMapCache();
	~UnicodeMapCache();

	// Get the UnicodeMap for <encodingName>.  Increments its reference
	// count; there will be one reference for the cache plus one for the
	// caller of this function.  Returns nullptr on failure.
	UnicodeMap* getUnicodeMap(const std::string& encodingName);

private:
	UnicodeMap* cache[unicodeMapCacheSize] = {}; //
};
