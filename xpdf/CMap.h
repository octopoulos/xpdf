//========================================================================
//
// CMap.h
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

class Object;
class Stream;
struct CMapVectorEntry;
class CMapCache;

//------------------------------------------------------------------------

class CMap
{
public:
	// Parse a CMap from <obj>, which can be a name or a stream.  Sets
	// the initial reference count to 1.  Returns nullptr on failure.
	static CMap* parse(CMapCache* cache, const std::string& collectionA, Object* obj);

	// Create the CMap specified by <collection> and <cMapName>.  Sets
	// the initial reference count to 1.  Returns nullptr on failure.
	static CMap* parse(CMapCache* cache, const std::string& collectionA, const std::string& cMapNameA);

	// Parse a CMap from <str>.  Sets the initial reference count to 1.
	// Returns nullptr on failure.
	static CMap* parse(CMapCache* cache, const std::string& collectionA, Stream* str);

	~CMap();

	void incRefCnt();
	void decRefCnt();

	// Return collection name (<registry>-<ordering>).
	std::string getCollection() { return collection; }

	// Return true if this CMap matches the specified <collectionA>, and
	// <cMapNameA>.
	bool match(const std::string& collectionA, const std::string& cMapNameA);

	// Return the CID corresponding to the character code starting at
	// <s>, which contains <len> bytes.  Sets *<c> to the char code, and
	// *<nUsed> to the number of bytes used by the char code.
	CID getCID(const char* s, int len, CharCode* c, int* nUsed);

	// Return the writing mode (0=horizontal, 1=vertical).
	int getWMode() { return wMode; }

private:
	void parse2(CMapCache* cache, int (*getCharFunc)(void*), void* data);
	CMap(const std::string& collectionA, const std::string& cMapNameA);
	CMap(const std::string& collectionA, const std::string& cMapNameA, int wModeA);
	void useCMap(CMapCache* cache, char* useName);
	void useCMap(CMapCache* cache, Object* obj);
	void copyVector(CMapVectorEntry* dest, CMapVectorEntry* src);
	void addCIDs(uint32_t start, uint32_t end, uint32_t nBytes, CID firstCID);
	void freeCMapVector(CMapVectorEntry* vec);

	std::string      collection; //
	std::string      cMapName;   //
	bool             isIdent;    // true if this CMap is an identity mapping, or is based on one (via usecmap)
	int              wMode;      // writing mode (0=horizontal, 1=vertical)
	CMapVectorEntry* vector;     // vector for first byte (nullptr for  identity CMap)
#if MULTITHREADED
	GAtomicCounter refCnt;
#else
	int refCnt;
#endif
};

//------------------------------------------------------------------------

#define cMapCacheSize 4

class CMapCache
{
public:
	CMapCache();
	~CMapCache();

	// Get the <cMapName> CMap for the specified character collection.
	// Increments its reference count; there will be one reference for
	// the cache plus one for the caller of this function.  Returns nullptr
	// on failure.
	CMap* getCMap(const std::string& collection, const std::string& cMapName);

private:
	CMap* cache[cMapCacheSize];
};
