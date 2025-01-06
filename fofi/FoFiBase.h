//========================================================================
//
// FoFiBase.h
//
// Copyright 1999-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>

//------------------------------------------------------------------------

typedef void (*FoFiOutputFunc)(void* stream, const char* data, size_t len);

//------------------------------------------------------------------------
// FoFiBase
//------------------------------------------------------------------------

class FoFiBase
{
public:
	virtual ~FoFiBase();

protected:
	FoFiBase(const char* fileA, size_t lenA, bool freeFileDataA);
	static char* readFile(std::string_view fileName, int* fileLen);

	// S = signed / U = unsigned
	// 8/16/32/Var = word length, in bytes
	// BE = big endian
	int      getS8(int pos, bool* ok);
	int      getU8(int pos, bool* ok);
	int      getS16BE(int pos, bool* ok);
	int      getU16BE(int pos, bool* ok);
	int      getS32BE(int pos, bool* ok);
	uint32_t getU32BE(int pos, bool* ok);
	uint32_t getU32LE(int pos, bool* ok);
	uint32_t getUVarBE(int pos, int size, bool* ok);

	bool checkRegion(int pos, int size);

	uint8_t* fileData     = nullptr; //
	uint8_t* file         = nullptr; //
	size_t   len          = 0;       //
	bool     freeFileData = false;   //
};
