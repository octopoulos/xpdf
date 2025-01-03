//========================================================================
//
// WebFont.h
//
// Modify/convert embedded PDF fonts to a form usable by web browsers.
//
// Copyright 2019 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "FoFiBase.h"
#include "GfxFont.h"

class FoFiTrueType;
class FoFiType1C;
class XRef;

//------------------------------------------------------------------------

class WebFont
{
public:
	WebFont(GfxFont* gfxFontA, XRef* xref);

	~WebFont();

	// Returns true if the font is, or can be converted to, a TrueType font.
	bool canWriteTTF();

	// Returns true if the font is, or can be converted to, an OpenType font.
	bool canWriteOTF();

	// Write a TrueType (.ttf) file to [fontFilePath].
	// This can only be called if canWriteTTF() returns true.
	// Returns true on success.
	bool writeTTF(const char* fontFilePath);

	// Return the TrueType file as a string.
	// This can only be called if canWriteTTF() returns true.
	// Returns null on error.
	std::string getTTFData();

	// Write an OpenType (.otf) file to [fontFilePath].
	// This can only be called if canWriteOTF() returns true.
	// Returns true on success.
	bool writeOTF(const char* fontFilePath);

	// Return the OpenType file as a string.
	// This can only be called if canWriteOTF() returns true.
	// Returns null on error.
	std::string getOTFData();

private:
	bool      generateTTF(FoFiOutputFunc outFunc, void* stream);
	bool      generateOTF(FoFiOutputFunc outFunc, void* stream);
	uint16_t* makeType1CWidths(int* codeToGID, int nCodes, int* nWidths);
	uint16_t* makeCIDType0CWidths(int* codeToGID, int nCodes, int* nWidths);
	uint8_t*  makeUnicodeCmapTable(int* codeToGID, int nCodes, int* unicodeCmapLength);
	int*      makeUnicodeToGID(int* codeToGID, int nCodes, int* unicodeToGIDLength);

	GfxFont*      gfxFont;    //
	char*         fontBuf;    //
	int           fontLength; //
	FoFiTrueType* ffTrueType; //
	FoFiType1C*   ffType1C;   //
	bool          isOpenType; //
};
