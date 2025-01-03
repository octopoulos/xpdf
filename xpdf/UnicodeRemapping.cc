//========================================================================
//
// UnicodeRemapping.cc
//
// Copyright 2018 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <stdio.h>
#include <string.h>
#include "gmem.h"
#include "gmempp.h"
#include "gfile.h"
#include "Error.h"
#include "UnicodeRemapping.h"

//------------------------------------------------------------------------

#define maxUnicodeString 8

struct UnicodeRemappingString
{
	Unicode in                    = 0;  //
	Unicode out[maxUnicodeString] = {}; //
	int     len                   = 0;  //
};

//------------------------------------------------------------------------

static int hexCharVals[256] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0x
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 1x
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 2x
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1,           // 3x
	-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 4x
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 5x
	-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 6x
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 7x
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 8x
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 9x
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // Ax
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // Bx
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // Cx
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // Dx
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // Ex
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1  // Fx
};

// Parse a <len>-byte hex string <s> into *<val>.  Returns false on
// error.
static bool parseHex(char* s, int len, uint32_t* val)
{
	*val = 0;
	for (int i = 0; i < len; ++i)
	{
		const int x = hexCharVals[s[i] & 0xff];
		if (x < 0) return false;
		*val = (*val << 4) + x;
	}
	return true;
}

//------------------------------------------------------------------------

UnicodeRemapping::UnicodeRemapping()
{
	for (int i = 0; i < 256; ++i)
		page0[i] = (Unicode)i;
}

UnicodeRemapping::~UnicodeRemapping()
{
	gfree(sMap);
}

void UnicodeRemapping::addRemapping(Unicode in, Unicode* out, int len)
{
	if (in < 256 && len == 1)
	{
		page0[in] = out[0];
	}
	else
	{
		if (in < 256)
			page0[in] = 0xffffffff;
		if (sMapLen == sMapSize)
		{
			sMapSize += 16;
			sMap = (UnicodeRemappingString*)greallocn(sMap, sMapSize, sizeof(UnicodeRemappingString));
		}
		const int i = findSMap(in);
		if (i < sMapLen)
			memmove(sMap + i + 1, sMap + i, (sMapLen - i) * sizeof(UnicodeRemappingString));
		sMap[i].in = in;
		int j;
		for (j = 0; j < len && j < maxUnicodeString; ++j)
			sMap[i].out[j] = out[j];
		sMap[i].len = j;
		++sMapLen;
	}
}

void UnicodeRemapping::parseFile(const std::string& fileName)
{
	FILE*   f;
	char    buf[256];
	Unicode in;
	Unicode out[maxUnicodeString];
	char*   tok;

	if (!(f = openFile(fileName.c_str(), "r")))
	{
		error(errSyntaxError, -1, "Couldn't open unicodeRemapping file '{}'", fileName);
		return;
	}

	int line = 0;
	while (getLine(buf, sizeof(buf), f))
	{
		++line;
		if (!(tok = strtok(buf, " \t\r\n")) || !parseHex(tok, (int)strlen(tok), &in))
		{
			error(errSyntaxWarning, -1, "Bad line ({}) in unicodeRemapping file '{}'", line, fileName);
			continue;
		}
		int n = 0;
		while (n < maxUnicodeString)
		{
			if (!(tok = strtok(nullptr, " \t\r\n")))
				break;
			if (!parseHex(tok, (int)strlen(tok), &out[n]))
			{
				error(errSyntaxWarning, -1, "Bad line ({}) in unicodeRemapping file '{}'", line, fileName);
				break;
			}
			++n;
		}
		addRemapping(in, out, n);
	}

	fclose(f);
}

// Determine the location in sMap to insert/replace the entry for [u].
int UnicodeRemapping::findSMap(Unicode u)
{
	int a = -1;
	int b = sMapLen;
	// invariant: sMap[a].in < u <= sMap[b].in
	while (b - a > 1)
	{
		const int m = (a + b) / 2;
		if (sMap[m].in < u)
			a = m;
		else
			b = m;
	}
	return b;
}

int UnicodeRemapping::map(Unicode in, Unicode* out, int size)
{
	if (in < 256 && page0[in] != 0xffffffff)
	{
		out[0] = page0[in];
		return 1;
	}

	int a = -1;
	int b = sMapLen;
	// invariant: sMap[a].in < in < sMap[b].in
	while (b - a > 1)
	{
		const int m = (a + b) / 2;
		if (sMap[m].in < in)
		{
			a = m;
		}
		else if (in < sMap[m].in)
		{
			b = m;
		}
		else
		{
			int i;
			for (i = 0; i < sMap[m].len && i < size; ++i)
				out[i] = sMap[m].out[i];
			return i;
		}
	}

	out[0] = in;
	return 1;
}
