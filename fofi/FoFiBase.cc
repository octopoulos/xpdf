//========================================================================
//
// FoFiBase.cc
//
// Copyright 1999-2003 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <stdio.h>
#include <limits.h>
#include "gmem.h"
#include "gmempp.h"
#include "FoFiBase.h"

//------------------------------------------------------------------------
// FoFiBase
//------------------------------------------------------------------------

FoFiBase::FoFiBase(const char* fileA, size_t lenA, bool freeFileDataA)
{
	fileData = file = (uint8_t*)fileA;
	len             = lenA;
	freeFileData    = freeFileDataA;
}

FoFiBase::~FoFiBase()
{
	if (freeFileData)
		gfree(fileData);
}

char* FoFiBase::readFile(const char* fileName, int* fileLen)
{
	FILE* f;
	char* buf;
	int   n;

	if (!(f = fopen(fileName, "rb"))) return nullptr;
	fseek(f, 0, SEEK_END);
	n = (int)ftell(f);
	if (n < 0)
	{
		fclose(f);
		return nullptr;
	}
	fseek(f, 0, SEEK_SET);
	buf = (char*)gmalloc(n);
	if ((int)fread(buf, 1, n, f) != n)
	{
		gfree(buf);
		fclose(f);
		return nullptr;
	}
	fclose(f);
	*fileLen = n;
	return buf;
}

int FoFiBase::getS8(int pos, bool* ok)
{
	int x;

	if (pos < 0 || pos >= len)
	{
		*ok = false;
		return 0;
	}
	x = file[pos];
	if (x & 0x80)
		x |= ~0xff;
	return x;
}

int FoFiBase::getU8(int pos, bool* ok)
{
	if (pos < 0 || pos >= len)
	{
		*ok = false;
		return 0;
	}
	return file[pos];
}

int FoFiBase::getS16BE(int pos, bool* ok)
{
	int x;

	if (pos < 0 || pos > INT_MAX - 1 || pos + 1 >= len)
	{
		*ok = false;
		return 0;
	}
	x = file[pos];
	x = (x << 8) + file[pos + 1];
	if (x & 0x8000)
		x |= ~0xffff;
	return x;
}

int FoFiBase::getU16BE(int pos, bool* ok)
{
	int x;

	if (pos < 0 || pos > INT_MAX - 1 || pos + 1 >= len)
	{
		*ok = false;
		return 0;
	}
	x = file[pos];
	x = (x << 8) + file[pos + 1];
	return x;
}

int FoFiBase::getS32BE(int pos, bool* ok)
{
	int x;

	if (pos < 0 || pos > INT_MAX - 3 || pos + 3 >= len)
	{
		*ok = false;
		return 0;
	}
	x = file[pos];
	x = (x << 8) + file[pos + 1];
	x = (x << 8) + file[pos + 2];
	x = (x << 8) + file[pos + 3];
	if (x & 0x80000000)
		x |= ~0xffffffff;
	return x;
}

uint32_t FoFiBase::getU32BE(int pos, bool* ok)
{
	uint32_t x;

	if (pos < 0 || pos > INT_MAX - 3 || pos + 3 >= len)
	{
		*ok = false;
		return 0;
	}
	x = file[pos];
	x = (x << 8) + file[pos + 1];
	x = (x << 8) + file[pos + 2];
	x = (x << 8) + file[pos + 3];
	return x;
}

uint32_t FoFiBase::getU32LE(int pos, bool* ok)
{
	uint32_t x;

	if (pos < 0 || pos > INT_MAX - 3 || pos + 3 >= len)
	{
		*ok = false;
		return 0;
	}
	x = file[pos + 3];
	x = (x << 8) + file[pos + 2];
	x = (x << 8) + file[pos + 1];
	x = (x << 8) + file[pos];
	return x;
}

uint32_t FoFiBase::getUVarBE(int pos, int size, bool* ok)
{
	if (pos < 0 || pos > INT_MAX - size || pos + size > len)
	{
		*ok = false;
		return 0;
	}
	uint32_t x = 0;
	for (int i = 0; i < size; ++i)
		x = (x << 8) + file[pos + i];
	return x;
}

bool FoFiBase::checkRegion(int pos, int size)
{
	return pos >= 0 && size >= 0 && size <= INT_MAX - pos && pos + size <= len;
}
