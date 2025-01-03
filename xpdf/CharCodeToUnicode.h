//========================================================================
//
// CharCodeToUnicode.h
//
// Mapping from character codes to Unicode.
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

struct CharCodeToUnicodeString;

//------------------------------------------------------------------------

class CharCodeToUnicode
{
public:
	// Create an identity mapping (Unicode = CharCode).
	static CharCodeToUnicode* makeIdentityMapping();

	// Read the CID-to-Unicode mapping for <collection> from the file
	// specified by <fileName>.  Sets the initial reference count to 1.
	// Returns nullptr on failure.
	static CharCodeToUnicode* parseCIDToUnicode(const std::string& fileName, const std::string& collection);

	// Create a Unicode-to-Unicode mapping from the file specified by
	// <fileName>.  Sets the initial reference count to 1.  Returns nullptr
	// on failure.
	static CharCodeToUnicode* parseUnicodeToUnicode(const std::string& fileName);

	// Create the CharCode-to-Unicode mapping for an 8-bit font.
	// <toUnicode> is an array of 256 Unicode indexes.  Sets the initial
	// reference count to 1.
	static CharCodeToUnicode* make8BitToUnicode(Unicode* toUnicode);

	// Create the CharCode-to-Unicode mapping for a 16-bit font.
	// <toUnicode> is an array of 65536 Unicode indexes.  Sets the
	// initial reference count to 1.
	static CharCodeToUnicode* make16BitToUnicode(Unicode* toUnicode);

	// Parse a ToUnicode CMap for an 8- or 16-bit font.
	static CharCodeToUnicode* parseCMap(const std::string& buf, int nBits);

	// Parse a ToUnicode CMap for an 8- or 16-bit font, merging it into <this>.
	void mergeCMap(const std::string& buf, int nBits);

	~CharCodeToUnicode();

	void incRefCnt();
	void decRefCnt();

	// Return true if this mapping matches the specified <tagA>.
	bool match(const std::string& tagA);

	// Set the mapping for <c>.
	void setMapping(CharCode c, Unicode* u, int len);

	// Map a CharCode to Unicode.
	int mapToUnicode(CharCode c, Unicode* u, int size);

	// Return the mapping's length, i.e., one more than the max char
	// code supported by the mapping.
	CharCode getLength() { return mapLen; }

	bool isIdentity() { return !map; }

private:
	bool parseCMap1(int (*getCharFunc)(void*), void* data, int nBits);
	void addMapping(CharCode code, char* uStr, int n, int offset);
	int  parseUTF16String(char* uStr, int n, Unicode* uOut);
	void addMappingInt(CharCode code, Unicode u);
	CharCodeToUnicode();
	CharCodeToUnicode(const std::string& tagA);
	CharCodeToUnicode(const std::string& tagA, Unicode* mapA, CharCode mapLenA, bool copyMap, CharCodeToUnicodeString* sMapA, int sMapLenA, int sMapSizeA);

	std::string              tag      = "";      //
	Unicode*                 map      = nullptr; //
	CharCode                 mapLen   = 0;       //
	CharCodeToUnicodeString* sMap     = nullptr; //
	int                      sMapLen  = 0;       //
	int                      sMapSize = 0;       //
	REFCNT_TYPE              refCnt   = 0;       //
};

//------------------------------------------------------------------------

class CharCodeToUnicodeCache
{
public:
	CharCodeToUnicodeCache(int sizeA);
	~CharCodeToUnicodeCache();

	// Get the CharCodeToUnicode object for <tag>.  Increments its
	// reference count; there will be one reference for the cache plus
	// one for the caller of this function.  Returns nullptr on failure.
	CharCodeToUnicode* getCharCodeToUnicode(const std::string& tag);

	// Insert <ctu> into the cache, in the most-recently-used position.
	void add(CharCodeToUnicode* ctu);

private:
	CharCodeToUnicode** cache = nullptr; //
	int                 size  = 0;       //
};
