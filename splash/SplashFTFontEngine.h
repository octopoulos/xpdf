//========================================================================
//
// SplashFTFontEngine.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>

#if HAVE_FREETYPE_H
#	include <ft2build.h>
#	include FT_FREETYPE_H

class SplashFontFile;
class SplashFontFileID;

//------------------------------------------------------------------------
// SplashFTFontEngine
//------------------------------------------------------------------------

class SplashFTFontEngine
{
public:
	static SplashFTFontEngine* init(bool aaA, uint32_t flagsA);

	~SplashFTFontEngine();

	// Load fonts.
	SplashFontFile* loadType1Font(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), const VEC_STR& enc);
	SplashFontFile* loadType1CFont(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), const VEC_STR& enc);
	SplashFontFile* loadOpenTypeT1CFont(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), const VEC_STR& enc);
	SplashFontFile* loadCIDFont(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), int* codeToGID, int codeToGIDLen);
	SplashFontFile* loadOpenTypeCFFFont(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), int* codeToGID, int codeToGIDLen);
	SplashFontFile* loadTrueTypeFont(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), int fontNum, int* codeToGID, int codeToGIDLen);

private:
	SplashFTFontEngine(bool aaA, uint32_t flagsA, FT_Library libA);

	bool       aa      = false;   //
	uint32_t   flags   = 0;       //
	FT_Library lib     = nullptr; //
	bool       useCIDs = false;   //

	friend class SplashFTFontFile;
	friend class SplashFTFont;
};

#endif // HAVE_FREETYPE_H
