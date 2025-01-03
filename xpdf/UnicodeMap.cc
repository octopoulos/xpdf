//========================================================================
//
// UnicodeMap.cc
//
// Copyright 2001-2003 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <stdio.h>
#include <string.h>
#include "gmem.h"
#include "gmempp.h"
#include "gfile.h"
#include "GList.h"
#include "Error.h"
#include "GlobalParams.h"
#include "UnicodeMap.h"

//------------------------------------------------------------------------

#define maxExtCode 16

struct UnicodeMapExt
{
	Unicode  u;                // Unicode char
	char     code[maxExtCode]; //
	uint32_t nBytes;           //
};

//------------------------------------------------------------------------

UnicodeMap* UnicodeMap::parse(const std::string& encodingNameA)
{
	char *tok1, *tok2, *tok3;

	FILE* f;
	if (!(f = globalParams->getUnicodeMapFile(encodingNameA)))
	{
		error(errSyntaxError, -1, "Couldn't find unicodeMap file for the '{}' encoding", encodingNameA);
		return nullptr;
	}

	UnicodeMap* map = new UnicodeMap(encodingNameA);

	int size      = 8;
	map->ranges   = (UnicodeMapRange*)gmallocn(size, sizeof(UnicodeMapRange));
	int eMapsSize = 0;

	char buf[256];
	int  line = 1;
	while (getLine(buf, sizeof(buf), f))
	{
		if ((tok1 = strtok(buf, " \t\r\n")) && (tok2 = strtok(nullptr, " \t\r\n")))
		{
			if (!(tok3 = strtok(nullptr, " \t\r\n")))
			{
				tok3 = tok2;
				tok2 = tok1;
			}
			const int nBytes = (int)strlen(tok3) / 2;
			if (nBytes <= 4)
			{
				if (map->len == size)
				{
					size *= 2;
					map->ranges = (UnicodeMapRange*)
					    greallocn(map->ranges, size, sizeof(UnicodeMapRange));
				}
				auto* range = &map->ranges[map->len];
				sscanf(tok1, "%x", &range->start);
				sscanf(tok2, "%x", &range->end);
				sscanf(tok3, "%x", &range->code);
				range->nBytes = nBytes;
				++map->len;
			}
			else if (tok2 == tok1)
			{
				if (map->eMapsLen == eMapsSize)
				{
					eMapsSize += 16;
					map->eMaps = (UnicodeMapExt*)
					    greallocn(map->eMaps, eMapsSize, sizeof(UnicodeMapExt));
				}
				auto* eMap = &map->eMaps[map->eMapsLen];
				sscanf(tok1, "%x", &eMap->u);
				for (int i = 0; i < nBytes; ++i)
				{
					int x;
					sscanf(tok3 + i * 2, "%2x", &x);
					eMap->code[i] = (char)x;
				}
				eMap->nBytes = nBytes;
				++map->eMapsLen;
			}
			else
			{
				error(errSyntaxError, -1, "Bad line ({}) in unicodeMap file for the '{}' encoding", line, encodingNameA);
			}
		}
		else
		{
			error(errSyntaxError, -1, "Bad line ({}) in unicodeMap file for the '{}' encoding", line, encodingNameA);
		}
		++line;
	}

	fclose(f);

	return map;
}

UnicodeMap::UnicodeMap(const std::string& encodingNameA)
{
	encodingName = encodingNameA;
	unicodeOut   = false;
	kind         = unicodeMapUser;
	ranges       = nullptr;
	len          = 0;
	eMaps        = nullptr;
	eMapsLen     = 0;
	refCnt       = 1;
}

UnicodeMap::UnicodeMap(const char* encodingNameA, bool unicodeOutA, UnicodeMapRange* rangesA, int lenA)
{
	encodingName = encodingNameA;
	unicodeOut   = unicodeOutA;
	kind         = unicodeMapResident;
	ranges       = rangesA;
	len          = lenA;
	eMaps        = nullptr;
	eMapsLen     = 0;
	refCnt       = 1;
}

UnicodeMap::UnicodeMap(const char* encodingNameA, bool unicodeOutA, UnicodeMapFunc funcA)
{
	encodingName = encodingNameA;
	unicodeOut   = unicodeOutA;
	kind         = unicodeMapFunc;
	func         = funcA;
	eMaps        = nullptr;
	eMapsLen     = 0;
	refCnt       = 1;
}

UnicodeMap::~UnicodeMap()
{
	if (kind == unicodeMapUser && ranges) gfree(ranges);
	if (eMaps) gfree(eMaps);
}

void UnicodeMap::incRefCnt()
{
#if MULTITHREADED
	gAtomicIncrement(&refCnt);
#else
	++refCnt;
#endif
}

void UnicodeMap::decRefCnt()
{
	bool done;

#if MULTITHREADED
	done = gAtomicDecrement(&refCnt) == 0;
#else
	done = --refCnt == 0;
#endif
	if (done) delete this;
}

bool UnicodeMap::match(const std::string& encodingNameA)
{
	return encodingName == encodingNameA;
}

int UnicodeMap::mapUnicode(Unicode u, char* buf, int bufSize)
{
	if (kind == unicodeMapFunc)
		return (*func)(u, buf, bufSize);

	int a = 0;
	int b = len;
	if (u >= ranges[a].start)
	{
		// invariant: ranges[a].start <= u < ranges[b].start
		while (b - a > 1)
		{
			const int m = (a + b) / 2;
			if (u >= ranges[m].start)
				a = m;
			else if (u < ranges[m].start)
				b = m;
		}
		if (u <= ranges[a].end)
		{
			const int n = ranges[a].nBytes;
			if (n > bufSize)
				return 0;
			uint32_t code = ranges[a].code + (u - ranges[a].start);
			for (int i = n - 1; i >= 0; --i)
			{
				buf[i] = (char)(code & 0xff);
				code >>= 8;
			}
			return n;
		}
	}

	for (int i = 0; i < eMapsLen; ++i)
	{
		if (eMaps[i].u == u)
		{
			const int n = eMaps[i].nBytes;
			for (int j = 0; j < n; ++j)
				buf[j] = eMaps[i].code[j];
			return n;
		}
	}

	return 0;
}

//------------------------------------------------------------------------

UnicodeMapCache::UnicodeMapCache()
{
	for (int i = 0; i < unicodeMapCacheSize; ++i)
		cache[i] = nullptr;
}

UnicodeMapCache::~UnicodeMapCache()
{
	for (int i = 0; i < unicodeMapCacheSize; ++i)
		if (cache[i])
			cache[i]->decRefCnt();
}

UnicodeMap* UnicodeMapCache::getUnicodeMap(const std::string& encodingName)
{
	UnicodeMap* map;

	if (cache[0] && cache[0]->match(encodingName))
	{
		cache[0]->incRefCnt();
		return cache[0];
	}
	for (int i = 1; i < unicodeMapCacheSize; ++i)
	{
		if (cache[i] && cache[i]->match(encodingName))
		{
			map = cache[i];
			for (int j = i; j >= 1; --j)
				cache[j] = cache[j - 1];
			cache[0] = map;
			map->incRefCnt();
			return map;
		}
	}
	if ((map = UnicodeMap::parse(encodingName)))
	{
		if (cache[unicodeMapCacheSize - 1])
			cache[unicodeMapCacheSize - 1]->decRefCnt();
		for (int j = unicodeMapCacheSize - 1; j >= 1; --j)
			cache[j] = cache[j - 1];
		cache[0] = map;
		map->incRefCnt();
		return map;
	}
	return nullptr;
}
